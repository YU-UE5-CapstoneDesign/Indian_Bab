#include "Actor/CardRevealStage.h"
#include "Actor/SeatActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"
#include "EngineUtils.h"

ACardRevealStage::ACardRevealStage()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;
	SetActorEnableCollision(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

void ACardRevealStage::StartReveal(APlayerController* LocalPlayer, const TArray<FCardRevealEntry>& Entries, float Duration)
{
	if (!LocalPlayer)
	{
		Destroy();
		return;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	LocalPlayer->GetPlayerViewPoint(ViewLocation, ViewRotation);
	// VR 머리 방향과 관계없이 앉은 자리의 앞쪽 테이블에 배치합니다.
	for (TActorIterator<ASeatActor> It(GetWorld()); It; ++It)
	{
		ASeatActor* Seat = *It;
		if (Seat->GetOccupant() == LocalPlayer->GetPawn() && Seat->SitTarget)
		{
			const FVector SeatLocation = Seat->SitTarget->GetComponentLocation();
			ViewLocation.X = SeatLocation.X;
			ViewLocation.Y = SeatLocation.Y;
			ViewRotation.Yaw = Seat->SitTarget->GetComponentRotation().Yaw;
			break;
		}
	}
	StartRevealAtView(ViewLocation, ViewRotation, Entries, Duration);
}

void ACardRevealStage::StartRevealAtView(FVector ViewLocation, FRotator ViewRotation, const TArray<FCardRevealEntry>& Entries, float Duration)
{
	if (Entries.IsEmpty())
	{
		Destroy();
		return;
	}

	const FVector Forward = FRotator(0.0f, ViewRotation.Yaw, 0.0f).Vector();
	FVector StageLocation = ViewLocation + Forward * StageDistance + FVector(0.0f, 0.0f, -StageDrop);
	FHitResult TableHit;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CardRevealTable), false);
	if (GetWorld()->LineTraceSingleByChannel(TableHit, StageLocation + FVector(0.0f, 0.0f, 60.0f),
		StageLocation - FVector(0.0f, 0.0f, 100.0f), ECC_Visibility, QueryParams)
		&& TableHit.ImpactPoint.Z >= ViewLocation.Z - 100.0f
		&& TableHit.ImpactPoint.Z <= ViewLocation.Z - 25.0f)
	{
		StageLocation.Z = TableHit.ImpactPoint.Z + 2.0f;
	}
	SetActorLocationAndRotation(StageLocation, FRotator(0.0f, ViewRotation.Yaw + 90.0f, 0.0f));
	// CardScale을 기준 크기에서의 배율로 바꿔 모든 자식 메시, 텍스트, 간격과 이동 거리에 적용합니다.
	constexpr float ReferenceCardScale = 0.038f;
	const float LayoutScale = FMath::Max(CardScale, 0.001f) / ReferenceCardScale;
	SceneRoot->SetRelativeScale3D(FVector(LayoutScale));

	ElapsedTime = 0.0f;
	SetLifeSpan(FMath::Max(Duration, 0.1f));
	Visuals.Reserve(Entries.Num());

	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		const FCardRevealEntry& Entry = Entries[Index];
		FCardVisual& Visual = Visuals.AddDefaulted_GetRef();
		Visual.bHasToken = Entry.bAddTokenSelected;

		Visual.Pivot = NewObject<USceneComponent>(this);
		Visual.Pivot->SetupAttachment(SceneRoot);
		Visual.Pivot->RegisterComponent();

		auto CreateMesh = [this, &Visual]()
		{
			UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(this);
			Mesh->SetupAttachment(Visual.Pivot);
			Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Mesh->SetCastShadow(false);
			Mesh->RegisterComponent();
			return Mesh;
		};

		Visual.Front = CreateMesh();
		Visual.Front->SetStaticMesh(Entry.Card.CardMesh.LoadSynchronous());
		if (FrontMaterial)
		{
			// 원본 카드 에셋은 그대로 두고 공개 연출에만 Unlit 머티리얼을 적용합니다.
			for (int32 MaterialIndex = 0; MaterialIndex < Visual.Front->GetNumMaterials(); ++MaterialIndex)
			{
				Visual.Front->SetMaterial(MaterialIndex, FrontMaterial);
			}
		}
		Visual.Front->SetRelativeLocation(FVector(0.0f, 0.0f, 1.0f));
		// 원본 카드 메시의 그림 면은 로컬 -Z이므로 테이블 위에서는 뒤집어 올립니다.
		Visual.Front->SetRelativeRotation(FRotator(180.0f, 0.0f, 0.0f));
		Visual.Front->SetRelativeScale3D(FVector(ReferenceCardScale));
		Visual.Front->SetVisibility(false);

		Visual.Back = CreateMesh();
		Visual.Back->SetStaticMesh(PlaneMesh);
		Visual.Back->SetMaterial(0, BackMaterial);
		Visual.Back->SetRelativeScale3D(BackScale);

		Visual.Token = CreateMesh();
		Visual.Token->SetStaticMesh(PlaneMesh);
		Visual.Token->SetMaterial(0, CoinMaterial);
		Visual.Token->SetRelativeScale3D(FVector(0.01f));
		Visual.Token->SetVisibility(false);

		if (Entry.bWinner)
		{
			// 이름이 긴 플레이어들이 서로 겹치지 않도록 승리자 이름만 만듭니다.
			Visual.NameText = NewObject<UTextRenderComponent>(this);
			Visual.NameText->SetupAttachment(Visual.Pivot);
			Visual.NameText->SetText(FText::FromString(FString::Printf(TEXT("WINNER\n%s"), *Entry.PlayerName)));
			Visual.NameText->SetWorldSize(WinnerNameSize);
			Visual.NameText->SetTextRenderColor(WinnerNameColor);
			Visual.NameText->SetHorizontalAlignment(EHTA_Center);
			Visual.NameText->SetRelativeLocation(WinnerNameOffset);
			Visual.NameText->SetRelativeRotation(FRotator(90.0f, 90.0f, 0.0f));
			Visual.NameText->RegisterComponent();
			Visual.NameText->SetVisibility(false);
		}

		Visual.ValueText = NewObject<UTextRenderComponent>(this);
		Visual.ValueText->SetupAttachment(Visual.Pivot);
		const FString ValueLabel = Entry.bAddTokenSelected
			? FString::Printf(TEXT("+3  >  %d"), Entry.ComparisonValue)
			: FString::FromInt(Entry.ComparisonValue);
		Visual.ValueText->SetText(FText::FromString(ValueLabel));
		Visual.ValueText->SetWorldSize(ComparisonValueSize);
		Visual.ValueText->SetTextRenderColor(ComparisonValueColor);
		Visual.ValueText->SetHorizontalAlignment(EHTA_Center);
		Visual.ValueText->SetRelativeLocation(ComparisonValueOffset);
		Visual.ValueText->SetRelativeRotation(FRotator(90.0f, 90.0f, 0.0f));
		Visual.ValueText->RegisterComponent();
		Visual.ValueText->SetVisibility(false);
	}
	Tick(0.0f);
}

void ACardRevealStage::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ElapsedTime += DeltaSeconds;

	const float LastCardStart = FMath::Max(0, Visuals.Num() - 1) * CardInterval;
	// BP에서 시간을 바꿔도 마지막 카드와 토큰이 공개된 뒤 승리자를 강조합니다.
	const float WinnerStart = FMath::Max3(Visuals.Num() * CardInterval + WinnerDelay,
		LastCardStart + CardFlipDuration, LastCardStart + TokenDelay + TokenDropDuration);
	for (int32 Index = 0; Index < Visuals.Num(); ++Index)
	{
		FCardVisual& Visual = Visuals[Index];
		const float CardStart = Index * CardInterval;
		const float CardProgress = FMath::Clamp((ElapsedTime - CardStart) / FMath::Max(CardFlipDuration, 0.01f), 0.0f, 1.0f);
		const float Ease = FMath::InterpEaseOut(0.0f, 1.0f, CardProgress, 2.0f);
		const float TargetX = (Index - (Visuals.Num() - 1) * 0.5f) * CardSpacing;
		Visual.Pivot->SetRelativeLocation(FVector(TargetX * Ease, 0.0f,
			2.0f + (1.0f - Ease) * 35.0f + FMath::Sin(Ease * PI) * 12.0f));
		Visual.Pivot->SetRelativeRotation(FRotator(0.0f, 0.0f, (1.0f - Ease) * 180.0f));
		Visual.Back->SetVisibility(CardProgress < 0.5f);
		Visual.Front->SetVisibility(CardProgress >= 0.5f && Visual.Front->GetStaticMesh() != nullptr);
		if (Visual.NameText)
		{
			Visual.NameText->SetVisibility(CardProgress >= 0.9f && ElapsedTime >= WinnerStart);
		}
		Visual.ValueText->SetVisibility(CardProgress >= 0.9f);

		const float TokenStart = CardStart + TokenDelay;
		const float TokenProgress = FMath::Clamp((ElapsedTime - TokenStart) / FMath::Max(TokenDropDuration, 0.01f), 0.0f, 1.0f);
		const float TokenEase = FMath::InterpEaseOut(0.0f, 1.0f, TokenProgress, 2.0f);
		Visual.Token->SetVisibility(Visual.bHasToken && ElapsedTime >= TokenStart);
		Visual.Token->SetRelativeLocation(TokenOffset + FVector(0.0f, 0.0f, (1.0f - TokenEase) * 30.0f));
		Visual.Token->SetRelativeRotation(FRotator(0.0f, (1.0f - TokenEase) * 720.0f, 0.0f));
		Visual.Token->SetRelativeScale3D(FVector(FMath::Lerp(0.01f, TokenScale, TokenEase)));
		if (Visual.bHasToken && TokenProgress >= 1.0f && !Visual.bDropSoundPlayed)
		{
			Visual.bDropSoundPlayed = true;
			if (CoinDropSound)
			{
				UGameplayStatics::PlaySoundAtLocation(this, CoinDropSound, Visual.Token->GetComponentLocation(), CoinDropVolume);
			}
		}

	}
}
