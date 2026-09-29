#include "Character/LobbyCharacter.h"
#include "EnhancedInputComponent.h"
#include "PlayerController/MainGamePlayerController.h"
#include "InputActionValue.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GroomComponent.h"
#include "Interface/InteractableInterface.h"
#include "Animation/AnimInstance.h"
#include "Net/UnrealNetwork.h"
#include "Actor/SeatActor.h"
#include "Actor/Revolver.h"
#include "Components/SphereComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Game/MainGameMode.h"
#include "Game/MainGameState.h"
#include "PlayerState/MainPlayerState.h"
#include "Widget/PlayerNameWidget.h"
#include "Components/WidgetComponent.h"
#include "DrawDebugHelpers.h"
#include "Kismet/KismetMathLibrary.h"

// Sets default values
ALobbyCharacter::ALobbyCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	bIsSitting = false; // 기본값은 서 있는 상태

	// 1인칭 메타휴먼 바디 생성 및 설정
	FirstPersonMetaHumanBody = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("First Person MetaHuman Body"));
	FirstPersonMetaHumanBody->SetupAttachment(GetMesh());
	FirstPersonMetaHumanBody->SetHiddenInGame(true);
	
	FirstPersonMetaHumanBody->SetCollisionProfileName(FName("NoCollision"));
	FirstPersonMetaHumanBody->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;

	// 1인칭 메타휴먼 얼굴 생성 및 설정
	FirstPersonMetaHumanTorso = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("First Person MetaHuman Torso"));
	FirstPersonMetaHumanTorso->SetupAttachment(FirstPersonMetaHumanBody);
	FirstPersonMetaHumanTorso->SetHiddenInGame(true);

	FirstPersonMetaHumanTorso->SetCollisionProfileName(FName("NoCollision"));
	FirstPersonMetaHumanTorso->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;

	// 3인칭 메타휴먼 바디 생성 및 설정
	ThirdPersonMetaHumanBody = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Third Person MetaHuman Body"));
	ThirdPersonMetaHumanBody->SetupAttachment(GetMesh());
	ThirdPersonMetaHumanBody->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;

	// 3인칭 메타휴먼 토르소 생성 및 설정
	ThirdPersonMetaHumanTorso = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Third Person MetaHuman Torso"));
	ThirdPersonMetaHumanTorso->SetupAttachment(ThirdPersonMetaHumanBody);
	ThirdPersonMetaHumanTorso->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;

	// 3인칭 메타휴먼 얼굴 생성 및 설정
	ThirdPersonMetaHumanFace = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Third Person MetaHuman Face"));
	ThirdPersonMetaHumanFace->SetupAttachment(ThirdPersonMetaHumanBody);
	ThirdPersonMetaHumanFace->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;

	// 3인칭 얼굴에 Groom(Hair, Eyebrows, Beard, Mustache, Eyelashes, Fuzz) 컴포넌트 추가
	(ThirdPersonMetaHumanHair = CreateDefaultSubobject< UGroomComponent>(TEXT("Hair")))->SetupAttachment(ThirdPersonMetaHumanFace);
	(ThirdPersonMetaHumanEyebrows = CreateDefaultSubobject< UGroomComponent>(TEXT("Eyebrows")))->SetupAttachment(ThirdPersonMetaHumanFace);
	(ThirdPersonMetaHumanBeard = CreateDefaultSubobject< UGroomComponent>(TEXT("Beard")))->SetupAttachment(ThirdPersonMetaHumanFace);
	(ThirdPersonMetaHumanMustache = CreateDefaultSubobject< UGroomComponent>(TEXT("Mustache")))->SetupAttachment(ThirdPersonMetaHumanFace);
	(ThirdPersonMetaHumanEyelashes = CreateDefaultSubobject< UGroomComponent>(TEXT("Eyelashes")))->SetupAttachment(ThirdPersonMetaHumanFace);
	(ThirdPersonMetaHumanFuzz = CreateDefaultSubobject< UGroomComponent>(TEXT("Fuzz")))->SetupAttachment(ThirdPersonMetaHumanFace);

	ThirdPersonMetaHumanHair->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;
	ThirdPersonMetaHumanEyebrows->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;
	ThirdPersonMetaHumanBeard->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;
	ThirdPersonMetaHumanMustache->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;
	ThirdPersonMetaHumanEyelashes->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;
	ThirdPersonMetaHumanFuzz->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;

	// 1인칭 리볼버 메시 (기본 숨김 - 총을 잡는 순간 표시)
	FP_RevolverMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FP_RevolverMesh"));
	FP_RevolverMesh->SetupAttachment(FirstPersonMetaHumanBody);
	FP_RevolverMesh->SetCollisionProfileName(FName("NoCollision"));
	FP_RevolverMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
	FP_RevolverMesh->SetVisibility(false);

	// 3인칭 리볼버 메시 (기본 숨김 - 총을 잡는 순간 표시)
	TP_RevolverMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("TP_RevolverMesh"));
	TP_RevolverMesh->SetupAttachment(ThirdPersonMetaHumanBody);
	TP_RevolverMesh->SetCollisionProfileName(FName("NoCollision"));
	TP_RevolverMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;
	TP_RevolverMesh->SetVisibility(false);

	FP_RevolverMesh->SetOnlyOwnerSee(true);
	TP_RevolverMesh->SetOwnerNoSee(true);

	FirstPersonMetaHumanBody->SetOnlyOwnerSee(true);
	FirstPersonMetaHumanTorso->SetOnlyOwnerSee(true);

	ThirdPersonMetaHumanBody->SetOwnerNoSee(true);
	ThirdPersonMetaHumanTorso->SetOwnerNoSee(true);
	ThirdPersonMetaHumanFace->SetOwnerNoSee(true);

	ThirdPersonMetaHumanHair->SetOwnerNoSee(true);
	ThirdPersonMetaHumanEyebrows->SetOwnerNoSee(true);
	ThirdPersonMetaHumanBeard->SetOwnerNoSee(true);
	ThirdPersonMetaHumanMustache->SetOwnerNoSee(true);
	ThirdPersonMetaHumanEyelashes->SetOwnerNoSee(true);
	ThirdPersonMetaHumanFuzz->SetOwnerNoSee(true);

	if (USkeletalMeshComponent* CharacterMesh = GetMesh())
	{
		CharacterMesh->SetVisibility(false, false);
		CharacterMesh->SetHiddenInGame(true, false);
		CharacterMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	}

	// Create the Camera Component
	CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("First Person Camera"));
	CameraComponent->SetupAttachment(GetRootComponent());
	// 실제 카메라 부착과 추적 방식은 PC/VR 자식에서 설정합니다.
	CameraComponent->bUsePawnControlRotation = false;
	CameraComponent->bEnableFirstPersonFieldOfView = true;
	CameraComponent->bEnableFirstPersonScale = true;
	CameraComponent->FirstPersonFieldOfView = 70.0f;
	CameraComponent->FirstPersonScale = 0.6f;

	// // 닉네임
	NameWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("NameWidget"));
	NameWidgetComponent->SetupAttachment(GetRootComponent());
	NameWidgetComponent->SetRelativeLocation(FVector(0.f, 0.f, 100.f));
	NameWidgetComponent->SetWidgetSpace(EWidgetSpace::World);
	NameWidgetComponent->SetDrawAtDesiredSize(false);
	NameWidgetComponent->SetDrawSize(FVector2D(420.f, 120.f));
	NameWidgetComponent->SetWorldScale3D(FVector(0.075f));
	NameWidgetComponent->SetTwoSided(true);

	// 카드 스태틱 메쉬
	CardDisplayMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CardDisplayMesh"));
	CardDisplayMesh->SetupAttachment(GetRootComponent());
	CardDisplayMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CardDisplayMesh->SetCastShadow(false);
	CardDisplayMesh->SetVisibility(false);
	CardDisplayMesh->SetRelativeLocation(FVector(0.f, 0.f, 100.f));
	CardDisplayMesh->SetRelativeScale3D(FVector(0.1f));
}

// Called when the game starts or when spawned
void ALobbyCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocallyControlled()) return;

	//MainGamePC = Cast<AMainGamePlayerController>(GetController());
}

// 서버에서 스팀 닉네임 및 카드 바인딩
void ALobbyCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	BindPlayerStateDelegates();
}

// 스팀 닉네임 및 카드 바인딩 함수(PS에서 스팀 닉네임이나 카드 상태 변화하면 Updaate함수 실행)
void ALobbyCharacter::BindPlayerStateDelegates()
{
	AMainPlayerState* PS = GetPlayerState<AMainPlayerState>();
	if (!PS) return;

	PS->OnSteamNicknameChanged.RemoveAll(this);
	PS->OnSteamNicknameChanged.AddUObject(this, &ALobbyCharacter::UpdateNameWidget);

	PS->OnCardChanged.RemoveAll(this);
	PS->OnCardChanged.AddUObject(this, &ALobbyCharacter::UpdateCardWidget);
	PS->OnCardChanged.AddUObject(this, &ALobbyCharacter::UpdateCardMesh);
	PS->OnTriggerCountChanged.RemoveAll(this);
	PS->OnTriggerCountChanged.AddUObject(this, &ALobbyCharacter::UpdateDeskRevolverCount);

	// 플레이어 상태 변화 구독 (생존 상태)
	PS->OnAliveStateChanged.RemoveAll(this);
	PS->OnAliveStateChanged.AddUObject(this, &ALobbyCharacter::OnAliveStateChanged);

	UpdateNameWidget();
	UpdateCardWidget();
	UpdateCardMesh();
	UpdatePlayerNameColor();

	if (IsValid(DeskRevolver))
	{
		UpdateDeskRevolverCount(PS->TotalTriggerCount);
	}

	// 게임 스테이트의 턴 변경 델리게이트 구독
	if (UWorld* World = GetWorld())
	{
		if (AMainGameState* GS = World->GetGameState<AMainGameState>())
		{
			GS->OnCurrentTurnPlayerChanged.RemoveAll(this);
			GS->OnCurrentTurnPlayerChanged.AddUObject(this, &ALobbyCharacter::UpdatePlayerNameColor);
		}
	}
}

void ALobbyCharacter::UpdateNameWidget()
{
	if (!NameWidgetComponent) return;

	AMainPlayerState* PS = GetPlayerState<AMainPlayerState>();
	if (!PS) return;

	if (!NameWidgetComponent->GetUserWidgetObject())
	{
		NameWidgetComponent->InitWidget();
	}

    UPlayerNameWidget* Widget = Cast<UPlayerNameWidget>(NameWidgetComponent->GetUserWidgetObject());
    if (!Widget) return;

	if (IsLocallyControlled())
	{
		Widget->SetPlayerName(TEXT(""));
		Widget->SetCardText(TEXT(""));
		return;
	}

    FString Name = PS->GetSteamNickname();
	if (Name.IsEmpty())
	{
        Name = TEXT("Unknown");
	}
    Widget->SetPlayerName(Name);
}

void ALobbyCharacter::UpdateCardWidget()
{
	if (!NameWidgetComponent) return;

	AMainPlayerState* PS = GetPlayerState<AMainPlayerState>();
	if (!PS) return;

	if (!NameWidgetComponent->GetUserWidgetObject())
	{
		NameWidgetComponent->InitWidget();
	}

	UPlayerNameWidget* Widget = Cast<UPlayerNameWidget>(NameWidgetComponent->GetUserWidgetObject());
    if (!Widget) return;

	if (IsLocallyControlled())
	{
		Widget->SetCardText(TEXT(""));
		return;
	}

	const FCardData Card = PS->GetMyCard();
	if (Card.Value == 0)
	{
		Widget->SetCardText(TEXT(""));
		return;
	}
	
	Widget->SetCardText("");
}

void ALobbyCharacter::UpdateCardMesh()
{
	if(!CardDisplayMesh) return;

	AMainPlayerState* PS = GetPlayerState<AMainPlayerState>();
	if (!PS) return;

	if (IsLocallyControlled())
	{
		CardDisplayMesh->SetVisibility(false);
		return;
	}

	const FCardData Card = PS->GetMyCard();
	if (Card.Value == 0)
	{
		CardDisplayMesh->SetVisibility(false);
		return;
	}
	
	UStaticMesh* LoadedCardMesh = Card.CardMesh.LoadSynchronous();
	CardDisplayMesh->SetStaticMesh(LoadedCardMesh);
	CardDisplayMesh->SetVisibility(IsValid(LoadedCardMesh));
}

void ALobbyCharacter::UpdatePlayerNameColor()
{
	if (!NameWidgetComponent) return;

	AMainPlayerState* PS = GetPlayerState<AMainPlayerState>();
	if (!PS) return;

	if (!NameWidgetComponent->GetUserWidgetObject())
	{
		NameWidgetComponent->InitWidget();
	}

	UPlayerNameWidget* Widget = Cast<UPlayerNameWidget>(NameWidgetComponent->GetUserWidgetObject());
	if (!Widget) return;

	// 로컬 플레이어는 이름 표시 안함
	if (IsLocallyControlled())
	{
		return;
	}

	FLinearColor TargetColor = Widget->DefaultColor;

	// 죽은 상태이면 빨간색
	if (!PS->isAlive)
	{
		TargetColor = Widget->DeadColor;
		UE_LOG(LogTemp, Warning, TEXT("[LobbyChar] Player %d is dead - Name Color: Red"), PS->GetPlayerId());
	}
	else
	{
		// 게임 스테이트에서 현재 턴 플레이어 확인
		if (UWorld* World = GetWorld())
		{
			if (AMainGameState* GS = World->GetGameState<AMainGameState>())
			{
				// 자신의 턴이면 파란색
				if (GS->CurrentTurnPlayerId == PS->GetPlayerId())
				{
					TargetColor = Widget->ActiveTurnColor;
					UE_LOG(LogTemp, Warning, TEXT("[LobbyChar] Player %d is in turn - Name Color: Blue"), PS->GetPlayerId());
				}
				// 그 외는 기본색
				else
				{
					TargetColor = Widget->DefaultColor;
					UE_LOG(LogTemp, Warning, TEXT("[LobbyChar] Player %d is idle - Name Color: Default"), PS->GetPlayerId());
				}
			}
		}
	}

	Widget->SetNameTextColor(TargetColor);
}

void ALobbyCharacter::OnAliveStateChanged(bool bIsAlive)
{
	UpdatePlayerNameColor();
}

// Called every frame
void ALobbyCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateAimFromView();

	DrawMainShotAimLine();
}

void ALobbyCharacter::UpdateAimFromView()
{
}

// Called to bind functionality to input
void ALobbyCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void ALobbyCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// bIsSitting 변수를 멀티플레이 환경에서 동기화
	DOREPLIFETIME(ALobbyCharacter, bIsSitting);
	DOREPLIFETIME(ALobbyCharacter, bIsSittingEnded);
	DOREPLIFETIME(ALobbyCharacter, GunHoldReason);
	DOREPLIFETIME(ALobbyCharacter, DeskRevolver);
	DOREPLIFETIME(ALobbyCharacter, ReplicatedAim);
	DOREPLIFETIME(ALobbyCharacter, ActiveRevolver);
}

void ALobbyCharacter::ServerInteract_Implementation(AActor* InteractableActor)
{
	// 서버에서 인터페이스의 Interact 함수 실행
	if (InteractableActor && InteractableActor->Implements<UInteractableInterface>())
	{
		IInteractableInterface::Execute_Interact(InteractableActor, this);
	}
}

void ALobbyCharacter::SetSittingState(bool bSitting)
{
	if (HasAuthority())
	{
		bIsSitting = bSitting;
		OnRep_IsSitting(); // 서버 자신도 시야/회전 제한이 즉각 적용되도록 수동 호출
	}
}

void ALobbyCharacter::MulticastPlaySitAnimation_Implementation()
{
	// 3인칭 메타휴먼 바디에 애니메이션 몽타주가 할당되어 있다면 재생
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (SitMontage && AnimInstance)
	{
		// 몽타주 재생 (애니메이션 BP에 'DefaultSlot' 등의 슬롯 설정이 되어 있어야 함)
		AnimInstance->Montage_Play(SitMontage, 1.0f);
	}
}

// 모든 클라이언트에서 공통 총 잡기 몽타주 처리를 실행합니다.
void ALobbyCharacter::Multicast_PlayGrabGunMontage_Implementation(EGunHoldReason Reason)
{
	PlayGrabGunMontage(Reason);
}

// 총을 잡는 이유에 맞는 몽타주를 재생하고 종료 처리를 연결합니다.
void ALobbyCharacter::PlayGrabGunMontage(EGunHoldReason Reason)
{
	GunHoldReason = Reason;

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (!AnimInstance)
	{
		if (HasAuthority())
		{
			OnGrabGunMontageEnded(nullptr, false);
		}
		return;
	}

	UAnimMontage* MontageToPlay = nullptr;
	if (Reason == EGunHoldReason::Fold)
	{
		MontageToPlay = AimMyselfMontage;
	}
	else if (Reason == EGunHoldReason::Win)
	{
		MontageToPlay = WinAimMontage;
	}

	if (MontageToPlay && AnimInstance->Montage_Play(MontageToPlay, 1.0f) > 0.0f)
	{
		FOnMontageEnded EndDelegate;
		EndDelegate.BindUObject(this, &ALobbyCharacter::OnGrabGunMontageEnded);
		AnimInstance->Montage_SetEndDelegate(EndDelegate, MontageToPlay);
		return;
	}

	if (HasAuthority())
	{
		OnGrabGunMontageEnded(nullptr, false);
	}
}

void ALobbyCharacter::Multicast_PutBackGunMontage_Implementation(EGunHoldReason Reason)
{
    UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (!AnimInstance)
	{
		FinishedReason = Reason;
		if (HasAuthority())
		{
			OnPutBackGunMontageEnded(nullptr, false);
		}
		return;
	}

	bIsPuttingBackGun = true;
	
	// 메인 리볼버를 내려놓기 시작하면 조준선 숨김
	if (Reason == EGunHoldReason::Win)
	{
		SetMainShotAimLineVisible(false);
	}
    UAnimMontage* MontageToPlay = nullptr;

    if (Reason == EGunHoldReason::Fold)
    {
        MontageToPlay = EndAimMyselfMontage;
    }
    else if (Reason == EGunHoldReason::Win)
    {
        MontageToPlay = WinEndMontage;
    }

	FinishedReason = GunHoldReason;
	GunHoldReason = EGunHoldReason::None;

	if (!MontageToPlay)
	{
		bIsPuttingBackGun = false;
		if (HasAuthority())
		{
			OnPutBackGunMontageEnded(nullptr, false);
		}
		return;
	}

	AnimInstance->Montage_Play(MontageToPlay, 1.0f);

	if (HasAuthority())
	{
		FOnMontageEnded EndDelegate;
		EndDelegate.BindUObject(this, &ALobbyCharacter::OnPutBackGunMontageEnded);
		AnimInstance->Montage_SetEndDelegate(EndDelegate, MontageToPlay);
	}
}

void ALobbyCharacter::OnRep_GunHoldReason()
{
	// GunHoldReason이 변경되면 ABP가 자동으로 감지해서 스테이트 트랜지션에 활용
}

void ALobbyCharacter::AttachRevolverToSocket()
{
	ARevolver* RevolverToAttach = ActiveRevolver ? ActiveRevolver.Get() : DeskRevolver.Get();
	// UE_LOG(LogTemp, Warning,
	// 	TEXT("[AttachRevolverToSocket] Char=%s Active=%s Desk=%s Attach=%s"),
	// 	*GetName(),
	// 	*GetNameSafe(ActiveRevolver),
	// 	*GetNameSafe(DeskRevolver),
	// 	*GetNameSafe(RevolverToAttach)
	// );
	if (!RevolverToAttach) return;

	// 1) 책상 위 리볼버 Prop 숨기기 + 콜리전 제거
	RevolverToAttach->SetActorHiddenInGame(true);
	RevolverToAttach->CollisionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 2) 책상 리볼버와 동일한 스켈레탈 메시 에셋을 FP/TP 컴포넌트에 복사
	USkeletalMesh* RevolverMeshAsset = RevolverToAttach->WeaponMesh->GetSkeletalMeshAsset();
	FP_RevolverMesh->SetSkeletalMeshAsset(RevolverMeshAsset);
	TP_RevolverMesh->SetSkeletalMeshAsset(RevolverMeshAsset);

	// 3) 1인칭 리볼버 → FirstPersonMetaHumanBody의 Revolver 소켓에 부착 후 표시
	FP_RevolverMesh->AttachToComponent(
		FirstPersonMetaHumanBody,
		FAttachmentTransformRules::SnapToTargetIncludingScale,
		FName("Revolver")
	);
	FP_RevolverMesh->SetVisibility(true);

	// 4) 3인칭 리볼버 → ThirdPersonMetaHumanBody의 Revolver 소켓에 부착 후 표시
	TP_RevolverMesh->AttachToComponent(
		ThirdPersonMetaHumanBody,
		FAttachmentTransformRules::SnapToTargetIncludingScale,
		FName("Revolver")
	);
	TP_RevolverMesh->SetVisibility(true);
	DrawMainShotAimLine();
	
}

// 1인칭과 3인칭 손의 총 메시를 숨기고 비웁니다.
void ALobbyCharacter::ClearHeldRevolverMeshes()
{
	if (FP_RevolverMesh)
	{
		FP_RevolverMesh->SetVisibility(false);
		FP_RevolverMesh->SetSkeletalMeshAsset(nullptr);
	}

	if (TP_RevolverMesh)
	{
		TP_RevolverMesh->SetVisibility(false);
		TP_RevolverMesh->SetSkeletalMeshAsset(nullptr);
	}
}

// 손의 총 메시를 정리하고 책상 총을 다시 표시합니다.
void ALobbyCharacter::ReturnRevolverToDesk()
{
	ARevolver* RevolverToReturn = ActiveRevolver ? ActiveRevolver.Get() : DeskRevolver.Get();

	ClearHeldRevolverMeshes();

	if (!RevolverToReturn) return;
	RevolverToReturn->SetActorHiddenInGame(false);

	if (RevolverToReturn->CollisionSphere)
	{
		RevolverToReturn->CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}
}

// 즉시 착석과 앉기 몽타주 종료에서 함께 사용하는 상태 처리 함수
void ALobbyCharacter::CompleteSeatedState()
{
	bIsSitting = true;
	bIsSittingEnded = true;
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	bUseControllerRotationYaw = false;
}

void ALobbyCharacter::StartSitTransition(ASeatActor* TargetSeat)
{
	CurrentSeat = TargetSeat;

	if (HasAuthority() && SitMontage)
	{
		// 서버에서만 몽타주 종료 시점을 캐치하도록 델리게이트 바인딩
		UAnimInstance* MainAnimInstance = GetMesh()->GetAnimInstance();
		if (MainAnimInstance)
		{
			// 혹시 이미 바인딩되어 있다면 제거 후 추가
			MainAnimInstance->OnMontageEnded.RemoveDynamic(this, &ALobbyCharacter::OnSitMontageEnded);
			MainAnimInstance->OnMontageEnded.AddDynamic(this, &ALobbyCharacter::OnSitMontageEnded);
		}
	}
}

void ALobbyCharacter::OnSitMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	// 앉기 몽타주가 끝났다면 (서버에서만 실행됨)
	if (Montage == SitMontage && HasAuthority())
	{
		// 루트 모션으로 인해 네트워크 오차가 발생했을 수 있으므로 최종 SitTarget 위치로 캡슐을 강제 보정(Snap)
		if (CurrentSeat && CurrentSeat->SitTarget)
		{
			//SetActorLocationAndRotation(CurrentSeat->SitTarget->GetComponentLocation(), CurrentSeat->SitTarget->GetComponentRotation());
			Client_LockCameraAfterSit(CurrentSeat->SitTarget->GetComponentRotation());
		}

		// // 완벽하게 안착했으므로 무브먼트 컴포넌트를 비활성화
		// GetCharacterMovement()->DisableMovement();

		// 즉시 착석과 동일한 이동 차단 및 착석 완료 상태를 적용합니다.
		CompleteSeatedState();

		// 델리게이트 해제 (메모리 릭 방지)
		UAnimInstance* MainAnimInstance = GetMesh()->GetAnimInstance();
		if (MainAnimInstance)
		{
			MainAnimInstance->OnMontageEnded.RemoveDynamic(this, &ALobbyCharacter::OnSitMontageEnded);
		}
	}
}

void ALobbyCharacter::OnGrabGunMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (bInterrupted) return;
	if (!HasAuthority()) return;
#if WITH_SERVER_CODE
	if (GunHoldReason == EGunHoldReason::Fold)
	{
		AMainGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AMainGameMode>() : nullptr;
		if (!GM) return;

		GM->HandleFoldMontageFinished(this);
	}
	else if(GunHoldReason == EGunHoldReason::Win)
	{
		AMainGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AMainGameMode>() : nullptr;
		if (!GM) return;

		GM->HandleMainMontageFinished(this);
	}
#endif
}

void ALobbyCharacter::OnPutBackGunMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (bInterrupted) return;
	if (!HasAuthority()) return;

	bIsPuttingBackGun = false;

#if WITH_SERVER_CODE
	AMainGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AMainGameMode>() : nullptr;
	if (!GM) return;

	GM->HandlePutBackGunMontageFinished(this, FinishedReason);
#endif
}

void ALobbyCharacter::OnRep_IsSitting()
{
    if (bIsSitting)
    {
        bUseControllerRotationYaw = false;
        GetCharacterMovement()->StopMovementImmediately();
        GetCharacterMovement()->DisableMovement();
    }
}

void ALobbyCharacter::OnRep_PlayerState()
{
    Super::OnRep_PlayerState();

	BindPlayerStateDelegates();
}

void ALobbyCharacter::OnRep_DeskRevolver()
{
	AMainPlayerState* PS = GetPlayerState<AMainPlayerState>();
	if (!PS || !IsValid(DeskRevolver))
	{
		return;
	}

	// PlayerState와 DeskRevolver의 복제 순서와 무관하게 최신 값을 다시 적용합니다.
	UpdateDeskRevolverCount(PS->TotalTriggerCount);
}

// 카메라 적용은 장치별 자식 클래스에서 구현합니다.
void ALobbyCharacter::OnSeatedCameraReady(const FRotator& SeatRotation)
{
}

void ALobbyCharacter::Client_LockCameraAfterSit_Implementation(FRotator FinalSitRotation)
{
	OnSeatedCameraReady(FinalSitRotation);
}

void ALobbyCharacter::Client_PrepareSit_Implementation(FVector TargetLocation, FRotator TargetRotation)
{
	// 서버의 복제 딜레이를 기다리지 않고, 클라이언트 스스로 즉시 의자 앞으로 캡슐을 강제 회전 및 이동시킵니다!
	SetActorLocationAndRotation(TargetLocation, TargetRotation);
}

void ALobbyCharacter::Server_UpdateAim_Implementation(FRotator NewAim)
{
	ReplicatedAim = NewAim; // 서버가 값을 받아서 모든 클라이언트에게 자동 전파
}

void ALobbyCharacter::SetActiveRevolver(ARevolver* NewRevolver)
{
	if (!HasAuthority()) return;

	ActiveRevolver = NewRevolver;
	ForceNetUpdate();
}

void ALobbyCharacter::BeginManualMainRevolverPhase()
{
	if (!HasAuthority()) return;

	GunHoldReason = EGunHoldReason::Win;
	bShowMainShotAimLine = false;
	bMainRevolverGrabbed = false;
	bIsPuttingBackGun = false;

	if (ActiveRevolver)
	{
		ActiveRevolver->SetActorHiddenInGame(false);
		if (ActiveRevolver->CollisionSphere)
		{
			ActiveRevolver->CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		}
	}

	ForceNetUpdate();
}

// 메인 총의 사용 상태를 정리하고 원래 책상 위치로 돌려놓습니다.
void ALobbyCharacter::ReturnMainRevolverToTableImmediately()
{

	ClearHeldRevolverMeshes();

	bShowMainShotAimLine = false;
	bMainRevolverGrabbed = false;
	bIsPuttingBackGun = false;
	GunHoldReason = EGunHoldReason::None;

	if (ActiveRevolver)
	{
		ActiveRevolver->ReturnToInitialTableTransform();
		ActiveRevolver = nullptr;
	}

	ForceNetUpdate();
}

void ALobbyCharacter::MarkMainRevolverGrabbed()
{
	if (!HasAuthority()) return;

	bMainRevolverGrabbed = true;
	ForceNetUpdate();
}

bool ALobbyCharacter::IsMainRevolverGrabbed() const
{
	return bMainRevolverGrabbed;
}

void ALobbyCharacter::SetMainShotAimLineVisible(bool bVisible)
{
	bShowMainShotAimLine = bVisible;
}

// 조준 표시는 필요한 자식 클래스에서 구현합니다.
void ALobbyCharacter::DrawMainShotAimLine()
{
}

// 구체적인 착석 방식은 PC/VR 자식에서 구현합니다.
void ALobbyCharacter::InitSeatedAtSeat(ASeatActor* TargetSeat)
{
}

// 공통 부모는 특정 장치의 조준 방향을 선택하지 않습니다.
bool ALobbyCharacter::GetMainShotTrace(float TraceDistance, FVector& OutStart, FVector& OutEnd) const
{
    return false;
}

// 자기 서브 리볼버 카운트 업데이트 함수
void ALobbyCharacter::UpdateDeskRevolverCount(int32 TriggerCount)
{
	if (!IsValid(DeskRevolver))
	{
		return;
	}

	DeskRevolver->UpdateFoldCountWidget(TriggerCount);
}
