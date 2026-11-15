#include "Actor/SeatActor.h"
#include "Actor/Revolver.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Character.h"
#include "PlayerController/MainGamePlayerController.h"
#include "Game/MainGameMode.h"
#include "Character/LobbyCharacter.h"


ASeatActor::ASeatActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true; // 멀티플레이어 동기화 필수

	SeatMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SeatMesh"));
	RootComponent = SeatMesh;

	SitTarget = CreateDefaultSubobject<USceneComponent>(TEXT("SitTarget"));
	SitTarget->SetupAttachment(RootComponent);

	StandTarget = CreateDefaultSubobject<USceneComponent>(TEXT("StandTarget"));
	StandTarget->SetupAttachment(RootComponent);

	bIsOccupied = false;
}


void ASeatActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASeatActor, bIsOccupied);
	DOREPLIFETIME(ASeatActor, Occupant);
}

TObjectPtr<AActor> ASeatActor::GetOccupant()
{
	return Occupant;
}

void ASeatActor::SetOccupant(AActor* NewOccupant)
{
	if (!HasAuthority())
	{
		return;
	}

	Occupant = NewOccupant;

	bIsOccupied = IsValid(NewOccupant);
	OnRep_IsOccupied();
}

void ASeatActor::BeginPlay()
{
	Super::BeginPlay();
}


// 인터페이스: 상호작용 텍스트 반환
FText ASeatActor::GetInteractPrompt_Implementation()
{
	if (bIsOccupied)
	{
		return FText::FromString(TEXT("이미 누군가 앉아있습니다."));
	}
	return FText::FromString(TEXT("앉기 (준비)"));
}


// 인터페이스: 실제 상호작용 (서버에서 실행됨)
void ASeatActor::Interact_Implementation(AActor* Interactor)
{
	// 서버가 아니거나, 이미 누가 앉아있다면 무시
	if (!HasAuthority() || bIsOccupied) return;

#if WITH_SERVER_CODE
	ALobbyCharacter* PlayerCharacter = Cast<ALobbyCharacter>(Interactor);
	if (PlayerCharacter)
	{
		if (PlayerCharacter->bIsSitting)
		{
			return;
		}

		// 상태 업데이트
		bIsOccupied = true;
		Occupant = PlayerCharacter;
		OnRep_IsOccupied(); // 서버 자신도 시각적 업데이트 호출

		// 캐릭터 위치를 의자(SitTarget)로 이동 및 회전 동기화
		PlayerCharacter->SetActorLocationAndRotation(StandTarget->GetComponentLocation(), GetActorRotation());
		// 클라이언트 복제 딜레이 없도록 RPC 호출로 즉시 위치 이동
		PlayerCharacter->Client_PrepareSit(StandTarget->GetComponentLocation(), StandTarget->GetComponentRotation());

		// 이 자리의 리볼버 참조를 캐릭터에 전달 (AnimNotify_GrabRevolver에서 사용)
		PlayerCharacter->DeskRevolver = DeskRevolver;

		PlayerCharacter->SetSittingState(true);

		// 몽타주가 끝나는 시점을 서버가 체크하여 상태를 확정짓도록 호출합니다.
		PlayerCharacter->StartSitTransition(this);

		// 모든 클라이언트에게 앉는 애니메이션 재생 지시
		PlayerCharacter->MulticastPlaySitAnimation();

		// 플레이어 컨트롤러의 입력 모드 변경 (로비 -> 메인 게임 UI 모드)
		AMainGamePlayerController* PC = Cast<AMainGamePlayerController>(PlayerCharacter->GetController());
		if (PC)
		{
			// 게임 모드에 해당 플레이어가 앉았음(준비 완료)을 알림
			if (AMainGameMode* GM = Cast<AMainGameMode>(GetWorld()->GetAuthGameMode()))
			{
				GM->PlayerSeated(PC, this);
			}

			// 클라이언트에게 입력 모드를 변경하라고 RPC 호출 필요 (아래에 추가 설명)
			PC->ClientOnSeated();
		}
	}
#endif
}


void ASeatActor::OnRep_IsOccupied()
{
	if (!bIsOccupied)
	{
		return;
	}

	ALobbyCharacter* Character =Cast<ALobbyCharacter>(GetOccupant());

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("Occupant: %s, SeatOrder: %d"),
		*GetNameSafe(Occupant),
		SeatOrder
	);

	if (!IsValid(Character))
	{

		return;
	}
	Character->ApplySeatColor(SeatColors[SeatOrder]);
}
