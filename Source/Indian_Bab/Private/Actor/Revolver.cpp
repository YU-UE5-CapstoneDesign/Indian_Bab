#include "Actor/Revolver.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "Widget/RevolverCountWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"

// 생성자
ARevolver::ARevolver()
{
	// 매 프레임마다 Tick()을 호출할 필요가 없다면 false로 꺼두는 것이 게임 성능에 좋습니다.
	PrimaryActorTick.bCanEverTick = true;

	// 멀티플레이어 동기화 필수 - 이게 없으면 클라이언트에서 DeskRevolver 포인터가 null이 됨
	bReplicates = true;
	SetReplicateMovement(true);

	// 스켈레탈 메시 컴포넌트 생성 및 루트(기준점)로 설정
	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	RootComponent = WeaponMesh; // 스켈레탈 메시를 루트로 설정

	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
	CollisionSphere->SetupAttachment(RootComponent); // 스켈레탈 메시를 캡슐 콜리전 컴포넌트에 붙입니다.
	
	// 리볼버 위에 베팅 발수 표시할 월드 스페이스 위젯 컴포넌트 생성
	BulletCountWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("BulletCountWidget"));
	BulletCountWidgetComponent->SetupAttachment(RootComponent);

	// 리볼버 메시 위쪽에 위치 (Z축 오프셋)
	BulletCountWidgetComponent->SetRelativeLocation(FVector(0.0f, 0.0f, CountWidgetHeight));
	// 총이 눕거나 회전해도 위젯 회전은 총의 회전을 상속하지 않습니다.
	BulletCountWidgetComponent->SetAbsolute(false, false, false);

	// 월드 스페이스로 설정 (카메라를 항상 바라보게 하려면 Screen Space 사용 가능)
	BulletCountWidgetComponent->SetWidgetSpace(EWidgetSpace::World);
	BulletCountWidgetComponent->SetTwoSided(false);

	// 위젯 크기 설정
	BulletCountWidgetComponent->SetDrawSize(FVector2D(100.0f, 50.0f));

	// 기본적으로 숨김 상태 (Playing 페이즈에서만 표시)
	BulletCountWidgetComponent->SetVisibility(false);

	// 기본 스탯 초기화
	MaxAmmo = 6;
	CurrentAmmo = MaxAmmo;
	Damage = 50.0f;

	// 사운드 에셋 로드
	static ConstructorHelpers::FObjectFinder<USoundBase> FireSoundAsset(
		TEXT("/Script/Engine.SoundWave'/Game/Revolver357/Audio/FireSound.FireSound'")
	);
	if (FireSoundAsset.Succeeded())
	{
		FireSound = FireSoundAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<USoundBase> DryFireSoundAsset(
		TEXT("/Script/Engine.SoundWave'/Game/Revolver357/Audio/DryFireSound.DryFireSound'")
	);
	if (DryFireSoundAsset.Succeeded())
	{
		DryFireSound = DryFireSoundAsset.Object;
	}
}

// BeginPlay
void ARevolver::BeginPlay()
{
	Super::BeginPlay();

	InitialTableTransform = GetActorTransform();
	bInitialTableTransformSaved = true;

	// 혹시 모르니 게임 시작 시 탄창을 꽉 채워줍니다.
	CurrentAmmo = MaxAmmo;

	// "MainRevolver" 태그가 없으면 위젯 컴포넌트 자체를 숨김
	// 태그는 레벨 에디터에서 메인 리볼버 액터에 직접 추가
	// if (!ActorHasTag(FName("MainRevolver")))
	// {
	// 	BulletCountWidgetComponent->SetVisibility(false);
	// 	BulletCountWidgetComponent->SetActive(false);
	// }
}

void ARevolver::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateCountWidgetTransform();
}

void ARevolver::UpdateCountWidgetTransform()
{
	if (!BulletCountWidgetComponent || !GetWorld() || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!CameraManager)
	{
		return;
	}

	// 부모인 총의 회전과 관계없이 항상 월드 기준 위쪽에 띄웁니다.
	const FVector WidgetLocation = GetActorLocation() + FVector::UpVector * (ActorHasTag(FName("MainRevolver")) ? CountWidgetHeight * 4.f: CountWidgetHeight);
	const FVector ToCamera = CameraManager->GetCameraLocation() - WidgetLocation;
	if (ToCamera.IsNearlyZero())
	{
		return;
	}

	FRotator FacingRotation = ToCamera.Rotation();
	FacingRotation.Roll = 0.0f;
	FacingRotation.Yaw += CountWidgetFacingYawOffset;

	// 이 컴포넌트 변환은 복제하지 않습니다. 각 클라이언트가 자기 카메라를 바라보게 합니다.
	BulletCountWidgetComponent->SetWorldLocationAndRotation(WidgetLocation, FacingRotation);
}

// 격발 기능
void ARevolver::Fire()
{
	// 잔탄이 남아있을 때만 발사
	if (CurrentAmmo > 0)
	{
		CurrentAmmo--;

		// 화면 좌측 상단과 출력 로그에 테스트용 텍스트를 띄웁니다.
		UE_LOG(LogTemp, Warning, TEXT("Bang! Fired a shot. Ammo left: %d"), CurrentAmmo);

		// TODO: 라인트레이스(LineTrace)를 통한 피격 판정 로직 추가
		// TODO: 총구 화염(Muzzle Flash) 파티클
		// TODO: 리볼버 해머/실린더가 돌아가는 '무기 자체 애니메이션' 재생
		PlayFireSound();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Click. Out of ammo!"));
		// TODO: 찰칵거리는 빈 총소리 사운드 재생
	}
}

// 장전 기능
void ARevolver::Reload()
{
	UE_LOG(LogTemp, Warning, TEXT("Reloading..."));

	// 탄창을 다시 꽉 채웁니다.
	CurrentAmmo = MaxAmmo;

	// TODO: 캐릭터의 재장전 애니메이션 및 리볼버 실린더가 열리는 애니메이션/사운드 재생
}

void ARevolver::UpdateBulletCountWidget(int32 CurrentCount, int32 MaxCount)
{
	// MainRevolver 태그가 있는 리볼버에서만 동작
	if (!ActorHasTag(FName("MainRevolver"))) return;

	URevolverCountWidget* CountWidget = Cast<URevolverCountWidget>(BulletCountWidgetComponent->GetUserWidgetObject());
	if (CountWidget)
	{
		CountWidget->UpdateCount(CurrentCount, MaxCount);
	}
}

// 폴드 카운트 위젯을 업데이트하는 함수
void ARevolver::UpdateFoldCountWidget(int32 Count)
{
	//SubRevolver 태그가 있는 리볼버에서만 동작
	if (!ActorHasTag(FName("SubRevolver"))) return;

	URevolverCountWidget* CountWidget = Cast<URevolverCountWidget>(BulletCountWidgetComponent->GetUserWidgetObject());
	if (CountWidget)
	{
		CountWidget->UpdateFoldCount(Count);
	}
}

void ARevolver::Multicast_PlayFireSound_Implementation()
{
	PlayFireSound();
}

void ARevolver::PlayFireSound() const
{
	if (!FireSound)
	{
		return;
	}

	UGameplayStatics::PlaySoundAtLocation(
		this,
		FireSound,
		GetActorLocation()
	);
}

void ARevolver::Multicast_PlayDryFireSound_Implementation()
{
	PlayDryFireSound();
}

void ARevolver::PlayDryFireSound() const
{
	if (!DryFireSound) return;

	UGameplayStatics::PlaySoundAtLocation(
		this,
		DryFireSound,
		GetActorLocation()
	);
}

void ARevolver::SetWidgetPlayingPhase(bool bIsPlaying)
{
	// MainRevolver 태그가 있는 리볼버에서만 동작
	if (!ActorHasTag(FName("MainRevolver")) && !ActorHasTag(FName("SubRevolver"))) return;

	URevolverCountWidget* CountWidget = Cast<URevolverCountWidget>(BulletCountWidgetComponent->GetUserWidgetObject());
	if (CountWidget)
	{
		CountWidget->SetPlayingPhase(bIsPlaying);
	}

	// 위젯 컴포넌트 자체의 가시성도 같이 설정
	BulletCountWidgetComponent->SetVisibility(bIsPlaying);
}

void ARevolver::ReturnToInitialTableTransform()
{
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	if (bInitialTableTransformSaved)
	{
		SetActorTransform(InitialTableTransform, false, nullptr, ETeleportType::TeleportPhysics);
	}

	SetActorHiddenInGame(false);

	if (CollisionSphere)
	{
		CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}

	ForceNetUpdate();
}
