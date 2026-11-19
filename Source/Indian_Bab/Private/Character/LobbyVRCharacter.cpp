#include "Character/LobbyVRCharacter.h"

#include "Actor/Revolver.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "GroomComponent.h"
#include "Actor/SeatActor.h"
#include "Game/MainGameMode.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/WidgetInteractionComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "InputCoreTypes.h"
#include "MotionControllerComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "UObject/ConstructorHelpers.h"
#include "PlayerController/MainGamePlayerController.h"
#include "Widget/GameResultWidget.h"
#include "Widget/MainGameWidget.h"
#include "Widget/ReadyWidget.h"
#include "Widget/TurnInfoWidget.h"
#include "Kismet/KismetMathLibrary.h"
#include "Net/UnrealNetwork.h"
#include "EnhancedInputComponent.h"
#include "GameInstanceSubsystem/IndianBabGameInstance.h"
#include "PlayerState/MainPlayerState.h"

namespace
{
	const TCHAR* GetVRUIStateLogName(EVRActiveUI UIState)
	{
		switch (UIState)
		{
		case EVRActiveUI::MainMenu:
			return TEXT("MainMenu");
		case EVRActiveUI::Ready:
			return TEXT("Ready");
		case EVRActiveUI::InGame:
			return TEXT("InGame");
		case EVRActiveUI::Result:
			return TEXT("Result");
		case EVRActiveUI::None:
		default:
			return TEXT("None");
		}
	}
}

ALobbyVRCharacter::ALobbyVRCharacter()
{
	VROrigin = CreateDefaultSubobject<USceneComponent>(TEXT("VROrigin"));
	VROrigin->SetupAttachment(GetRootComponent());

	if (CameraComponent)
	{
		CameraComponent->SetupAttachment(VROrigin);
		CameraComponent->SetRelativeLocation(FVector::ZeroVector);
		CameraComponent->SetRelativeRotation(FRotator::ZeroRotator);
		CameraComponent->bUsePawnControlRotation = false;
		CameraComponent->bLockToHmd = false;
	}
	
	MetaHumanBodyAnimation = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("AnimationBody"));
	MetaHumanBodyAnimation->SetupAttachment(GetMesh());
	MetaHumanBodyAnimation->SetOwnerNoSee(true);

	MetaHumanTorsoAnimation = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("AnimationTorso"));
	MetaHumanTorsoAnimation->SetupAttachment(MetaHumanBodyAnimation);
	MetaHumanTorsoAnimation->SetOwnerNoSee(true);
	MetaHumanTorsoAnimation->SetLeaderPoseComponent(MetaHumanBodyAnimation);

	MetaHumanFaceAnimation = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("AnimationFace"));
	MetaHumanFaceAnimation->SetupAttachment(MetaHumanBodyAnimation);
	MetaHumanFaceAnimation->SetOwnerNoSee(true);
	MetaHumanFaceAnimation->SetLeaderPoseComponent(MetaHumanBodyAnimation);

	VRAnimationRevolverMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("VRAnimationRevolver"));
	VRAnimationRevolverMesh->SetupAttachment(MetaHumanBodyAnimation, FName("Revolver"));
	VRAnimationRevolverMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VRAnimationRevolverMesh->SetVisibility(false);
	VRAnimationRevolverMesh->SetHiddenInGame(true);

	MotionControllerRightGrip = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("MotionControllerRightGrip"));
	MotionControllerRightGrip->SetupAttachment(VROrigin);
	MotionControllerRightGrip->MotionSource = FName(TEXT("RightGrip"));

	MotionControllerLeftGrip = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("MotionControllerLeftGrip"));
	MotionControllerLeftGrip->SetupAttachment(VROrigin);
	MotionControllerLeftGrip->MotionSource = FName(TEXT("LeftGrip"));

	MotionControllerRightAim = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("MotionControllerRightAim"));
	MotionControllerRightAim->SetupAttachment(VROrigin);
	MotionControllerRightAim->MotionSource = FName(TEXT("RightAim"));

	MotionControllerLeftAim = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("MotionControllerLeftAim"));
	MotionControllerLeftAim->SetupAttachment(VROrigin);
	MotionControllerLeftAim->MotionSource = FName(TEXT("LeftAim"));

	HandRight = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HandRight"));
	HandRight->SetupAttachment(MotionControllerRightGrip);
	HandRight->SetCollisionProfileName(TEXT("NoCollision"));
	HandRight->SetOnlyOwnerSee(true);

	HandLeft = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HandLeft"));
	HandLeft->SetupAttachment(MotionControllerLeftGrip);
	HandLeft->SetCollisionProfileName(TEXT("NoCollision"));
	HandLeft->SetOnlyOwnerSee(true);

	WidgetInteractionRight = CreateDefaultSubobject<UWidgetInteractionComponent>(TEXT("WidgetInteractionRight"));
	WidgetInteractionRight->SetupAttachment(MotionControllerRightAim);

	WidgetInteractionLeft = CreateDefaultSubobject<UWidgetInteractionComponent>(TEXT("WidgetInteractionLeft"));
	WidgetInteractionLeft->SetupAttachment(MotionControllerLeftAim);

	PointerLaserRight = CreateDefaultSubobject<UNiagaraComponent>(TEXT("PointerLaserRight"));
	PointerLaserRight->SetupAttachment(MotionControllerRightAim);
	PointerLaserRight->SetAutoActivate(false);

	PointerLaserLeft = CreateDefaultSubobject<UNiagaraComponent>(TEXT("PointerLaserLeft"));
	PointerLaserLeft->SetupAttachment(MotionControllerLeftAim);
	PointerLaserLeft->SetAutoActivate(false);

	// 빔은 로컬 클라에서만 켜지므로 데디는 에셋을 찾지 않습니다.
	if (!IsRunningDedicatedServer())
	{
		static ConstructorHelpers::FObjectFinder<UNiagaraSystem> PointerLaserSystem(TEXT("/Game/VRTemplate/VFX/NS_MenuLaser.NS_MenuLaser"));
		if (PointerLaserSystem.Succeeded())
		{
			PointerLaserRight->SetAsset(PointerLaserSystem.Object);
			PointerLaserLeft->SetAsset(PointerLaserSystem.Object);
		}
	}

	ReadyWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("ReadyWidget"));
	ReadyWidgetComponent->SetupAttachment(CameraComponent);
	ReadyWidgetComponent->SetWidgetSpace(EWidgetSpace::World);
	ReadyWidgetComponent->SetTwoSided(true);
	ReadyWidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ReadyWidgetComponent->SetVisibility(false);
	ReadyWidgetComponent->SetHiddenInGame(true);

	ResultWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("ResultWidget"));
	ResultWidgetComponent->SetupAttachment(CameraComponent);
	ResultWidgetComponent->SetWidgetSpace(EWidgetSpace::World);
	ResultWidgetComponent->SetTwoSided(true);
	ResultWidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ResultWidgetComponent->SetVisibility(false);
	ResultWidgetComponent->SetHiddenInGame(true);

	TurnInfoWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("TurnInfoWidget"));
	TurnInfoWidgetComponent->SetupAttachment(CameraComponent);
	TurnInfoWidgetComponent->SetWidgetSpace(EWidgetSpace::World);
	TurnInfoWidgetComponent->SetTwoSided(true);
	TurnInfoWidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TurnInfoWidgetComponent->SetVisibility(false);
	TurnInfoWidgetComponent->SetHiddenInGame(true);

	ConfigureVRSeatedState();
	ConfigureWidgetInteraction();
}

void ALobbyVRCharacter::BeginPlay()
{
	Super::BeginPlay();

	ConfigureLocalVRTracking();
	ConfigureVRSeatedState();
	ConfigureWidgetInteraction();
	ApplyVRWidgetComponentState(TurnInfoWidgetComponent, false);
}


void ALobbyVRCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	ConfigureLocalVRTracking();
	ConfigureWidgetInteraction();
}

void ALobbyVRCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	ConfigureLocalVRTracking();
	ConfigureWidgetInteraction();
}

void ALobbyVRCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();
	ConfigureLocalVRTracking();
	ConfigureWidgetInteraction();
}

void ALobbyVRCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateVRPointers();
	if (IsDeathPresentationActive()) return;
	UpdateArmPosition();
	SmoothReplicatedArms(DeltaTime);
}

void ALobbyVRCharacter::InitSeatedAtSeat(ASeatActor* TargetSeat)
{
	if (!HasAuthority() || !TargetSeat || !TargetSeat->SitTarget)
	{
		return;
	}

	DeskRevolver = TargetSeat->DeskRevolver;

	const FVector OriginalSitLocation = TargetSeat->SitTarget->GetComponentLocation();
	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	const float CapsuleHalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 0.0f;
	const float CapsuleSeatOffset = bUseCapsuleHalfHeightSeatOffset ? CapsuleHalfHeight : 0.0f;

	FVector SitLocation = OriginalSitLocation;
	SitLocation.Z += CapsuleSeatOffset + SeatHeightOffset;

	FRotator SitRotation = TargetSeat->SitTarget->GetComponentRotation();
	SitRotation.Pitch = 0.0f;
	SitRotation.Roll = 0.0f;

	SetActorLocationAndRotation(SitLocation, SitRotation, false, nullptr, ETeleportType::TeleportPhysics);

	UE_LOG(LogTemp, Warning,
		TEXT("[LobbyVRCharacter] AutoSeat Seat=%s OriginalSitLocation=%s CapsuleHalfHeight=%.2f bUseCapsuleHalfHeight=%s SeatHeightOffset=%.2f FinalSitLocation=%s AppliedActorLocation=%s"),
		*GetNameSafe(TargetSeat),
		*OriginalSitLocation.ToString(),
		CapsuleHalfHeight,
		bUseCapsuleHalfHeightSeatOffset ? TEXT("true") : TEXT("false"),
		SeatHeightOffset,
		*SitLocation.ToString(),
		*GetActorLocation().ToString());

	CompleteSeatedState();
	ConfigureVRSeatedState();
	ConfigureWidgetInteraction();
	DrawSeatDebugCapsule();

	Client_InitSeatedAtSeat(SitLocation, SitRotation);
	ForceNetUpdate();
}

void ALobbyVRCharacter::Client_InitSeatedAtSeat_Implementation(FVector TargetLocation, FRotator TargetRotation)
{
	TargetRotation.Pitch = 0.0f;
	TargetRotation.Roll = 0.0f;

	SetActorLocationAndRotation(TargetLocation, TargetRotation, false, nullptr, ETeleportType::TeleportPhysics);

	CompleteSeatedState();
	ConfigureVRSeatedState();
	ConfigureWidgetInteraction();
	DrawSeatDebugCapsule();

	if (UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled())
	{
		UHeadMountedDisplayFunctionLibrary::ResetOrientationAndPosition(0.0f, EOrientPositionSelector::OrientationAndPosition);
	}

	ShowReadyWidgetAfterDelay();
}

void ALobbyVRCharacter::Client_ShowReadyWidget_Implementation()
{
	ShowReadyWidgetAfterDelay();
}

void ALobbyVRCharacter::Client_HideReadyWidget_Implementation()
{
	HideReadyWidget();
}

void ALobbyVRCharacter::Client_ShowMainGameWidget_Implementation()
{
	ShowMainGameWidget();
}

void ALobbyVRCharacter::Client_HideMainGameWidget_Implementation()
{
	HideMainGameWidget();
}

void ALobbyVRCharacter::Client_ShowResultWidget_Implementation(const FString& WinnerName, int32 WinnerPlayerId)
{
	if (AMainGamePlayerController* PC = Cast<AMainGamePlayerController>(GetController()))
		PC->DismissEliminationNoticeForResults();
	ShowResultWidget(WinnerName, WinnerPlayerId);

	// VR 클라이언트에서도 최종 결과 BGM 재생
	if (IsLocallyControlled())
	{
		if (UIndianBabGameInstance* GI =
			Cast<UIndianBabGameInstance>(GetGameInstance()))
		{
			GI->PlayVictoryBGM();
		}
	}
}

void ALobbyVRCharacter::PressRightWidgetInteraction()
{
	bRightTriggerHeld = true;
	if (bLeftTriggerHeld)
	{
		// 양쪽 트리거를 동시에 누르면 시점 기준점을 다시 잡습니다.
		RecenterSeatedView();
	}

	if (!IsLocallyControlled() || !WidgetInteractionRight || !WidgetInteractionRight->IsActive())
	{
		return;
	}

	WidgetInteractionRight->PressPointerKey(EKeys::LeftMouseButton);
}

void ALobbyVRCharacter::ReleaseRightWidgetInteraction()
{
	bRightTriggerHeld = false;

	if (!IsLocallyControlled() || !WidgetInteractionRight || !WidgetInteractionRight->IsActive())
	{
		return;
	}

	const bool bReadyWidgetHovered = WidgetInteractionRight->GetHoveredWidgetComponent() == ReadyWidgetComponent;
	const bool bResultWidgetHovered = WidgetInteractionRight->GetHoveredWidgetComponent() == ResultWidgetComponent;
	WidgetInteractionRight->ReleasePointerKey(EKeys::LeftMouseButton);
	if (UWidgetComponent* HoveredWidgetComponent = WidgetInteractionRight->GetHoveredWidgetComponent())
	{
		if (UMainGameWidget* MainGameWidget = Cast<UMainGameWidget>(HoveredWidgetComponent->GetUserWidgetObject()))
		{
			MainGameWidget->HandleVRClickAtWidgetLocation(WidgetInteractionRight->Get2DHitLocation());
		}
	}
	if (bReadyWidgetHovered)
	{
		if (UReadyWidget* ReadyWidget = Cast<UReadyWidget>(ReadyWidgetComponent ? ReadyWidgetComponent->GetUserWidgetObject() : nullptr))
		{
			ReadyWidget->ConfirmReady();
			RecenterSeatedView();
		}
	}
	if (bResultWidgetHovered)
	{
		if (UGameResultWidget* ResultWidget = Cast<UGameResultWidget>(ResultWidgetComponent ? ResultWidgetComponent->GetUserWidgetObject() : nullptr))
		{
			ResultWidget->ConfirmBackToMainMenu();
		}
	}
}

void ALobbyVRCharacter::PressLeftWidgetInteraction()
{
	bLeftTriggerHeld = true;
	if (bRightTriggerHeld)
	{
		// 양쪽 트리거를 동시에 누르면 시점 기준점을 다시 잡습니다.
		RecenterSeatedView();
	}

	if (!IsLocallyControlled() || !WidgetInteractionLeft || !WidgetInteractionLeft->IsActive())
	{
		return;
	}

	WidgetInteractionLeft->PressPointerKey(EKeys::LeftMouseButton);
}

void ALobbyVRCharacter::ReleaseLeftWidgetInteraction()
{
	bLeftTriggerHeld = false;

	if (!IsLocallyControlled() || !WidgetInteractionLeft || !WidgetInteractionLeft->IsActive())
	{
		return;
	}

	const bool bReadyWidgetHovered = WidgetInteractionLeft->GetHoveredWidgetComponent() == ReadyWidgetComponent;
	const bool bResultWidgetHovered = WidgetInteractionLeft->GetHoveredWidgetComponent() == ResultWidgetComponent;
	WidgetInteractionLeft->ReleasePointerKey(EKeys::LeftMouseButton);
	if (UWidgetComponent* HoveredWidgetComponent = WidgetInteractionLeft->GetHoveredWidgetComponent())
	{
		if (UMainGameWidget* MainGameWidget = Cast<UMainGameWidget>(HoveredWidgetComponent->GetUserWidgetObject()))
		{
			MainGameWidget->HandleVRClickAtWidgetLocation(WidgetInteractionLeft->Get2DHitLocation());
		}
	}
	if (bReadyWidgetHovered)
	{
		if (UReadyWidget* ReadyWidget = Cast<UReadyWidget>(ReadyWidgetComponent ? ReadyWidgetComponent->GetUserWidgetObject() : nullptr))
		{
			ReadyWidget->ConfirmReady();
			RecenterSeatedView();
		}
	}
	if (bResultWidgetHovered)
	{
		if (UGameResultWidget* ResultWidget = Cast<UGameResultWidget>(ResultWidgetComponent ? ResultWidgetComponent->GetUserWidgetObject() : nullptr))
		{
			ResultWidget->ConfirmBackToMainMenu();
		}
	}
}

void ALobbyVRCharacter::RecenterSeatedView()
{
	if (!IsLocallyControlled() || !UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled())
	{
		return;
	}

	// 지금 머리 위치를 아바타 눈 위치(VROrigin)로 다시 맞춥니다. 바라보는 방향은 건드리지 않습니다.
	UHeadMountedDisplayFunctionLibrary::ResetOrientationAndPosition(0.0f, EOrientPositionSelector::Position);
}

void ALobbyVRCharacter::OnRep_IsSitting()
{
	Super::OnRep_IsSitting();
	ConfigureVRSeatedState();
	ConfigureWidgetInteraction();
}

void ALobbyVRCharacter::Server_UpdateArm_Implementation(const FTransform& NewLeftArm, const FTransform& NewRightArm, const FTransform& NewRightAim)
{
	if (IsDeathPresentationActive()) return;
	LeftArm = NewLeftArm;
	RightArm = NewRightArm;
	NetLeftArm = NewLeftArm;
	NetRightArm = NewRightArm;
	RightAimTransform = NewRightAim;
	bHasRightAimTransform = !NewRightAim.ContainsNaN();
	ApplyReplicatedArmTransforms();
}

void ALobbyVRCharacter::OnRep_ArmTransforms()
{
	// 첫 값만 바로 맞추고, 이후에는 Tick의 보간이 목표값을 따라갑니다.
	if (!bHasNetArms)
	{
		bHasNetArms = true;
		LeftArm = NetLeftArm;
		RightArm = NetRightArm;
		ApplyReplicatedArmTransforms();
	}
}

void ALobbyVRCharacter::SmoothReplicatedArms(float DeltaTime)
{
	if (GetLocalRole() != ROLE_SimulatedProxy || !bHasNetArms)
	{
		return;
	}

	auto Smooth = [this, DeltaTime](FTransform& Current, const FTransform& Target)
	{
		if (Target.ContainsNaN())
		{
			return;
		}
		if (FVector::DistSquared(Current.GetLocation(), Target.GetLocation()) > FMath::Square(RemoteArmSnapDistance))
		{
			Current = Target;
			return;
		}
		Current.SetLocation(FMath::VInterpTo(Current.GetLocation(), Target.GetLocation(), DeltaTime, RemoteArmInterpSpeed));
		Current.SetRotation(FMath::QInterpTo(Current.GetRotation(), Target.GetRotation(), DeltaTime, RemoteArmInterpSpeed));
		Current.SetScale3D(Target.GetScale3D());
	};

	Smooth(LeftArm, NetLeftArm);
	Smooth(RightArm, NetRightArm);
	ApplyReplicatedArmTransforms();
}

void ALobbyVRCharacter::UpdateAimFromView()
{
	if (!bIsSitting || !IsLocallyControlled())
	{
		return;
	}

	FRotator HeadRotation = FRotator::ZeroRotator;
	FVector HeadPosition = FVector::ZeroVector;

	if (UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled())
	{
		// HMD pose can be applied during XR camera update/late update without changing
		// the camera component transform visible to the game thread.
		UHeadMountedDisplayFunctionLibrary::GetOrientationAndPosition(HeadRotation, HeadPosition);
	}
	else if (CameraComponent)
	{
		HeadRotation = UKismetMathLibrary::NormalizedDeltaRotator(
			CameraComponent->GetComponentRotation(),
			GetActorRotation());
	}

	HeadRotation.Normalize();
	ReplicatedAim = HeadRotation;

	if (HasAuthority())
	{
		ForceNetUpdate();
	}
	else
	{
		Server_UpdateAim(ReplicatedAim);
	}
}

// 기존 파란선과 동일한 오른손 Aim 방향으로 서버 피격 판정을 계산합니다.
bool ALobbyVRCharacter::GetRightHandShotTrace(FVector& OutStart, FVector& OutEnd) const
{
	if (!MotionControllerRightAim || (!IsLocallyControlled() && !bHasRightAimTransform)) return false;
	const FTransform HandTransform = IsLocallyControlled()
		? MotionControllerRightAim->GetComponentTransform() : RightAimTransform;
	if (HandTransform.ContainsNaN()) return false;
	OutStart = HandTransform.GetLocation();
	const FVector Direction = HandTransform.GetUnitAxis(EAxis::X);
	OutEnd = OutStart + Direction * VRPointerMaxDistance;
	return !Direction.IsNearlyZero();
}

void ALobbyVRCharacter::UpdateArmPosition() {
	if (!IsLocallyControlled() || !MotionControllerLeftGrip || !MotionControllerRightGrip)
	{
		return;
	}
	LeftArm = MotionControllerLeftGrip->GetComponentTransform();
	RightArm = MotionControllerRightGrip->GetComponentTransform();
	if (MotionControllerRightAim)
	{
		Server_UpdateArm(LeftArm, RightArm, MotionControllerRightAim->GetComponentTransform());
	}
}

void ALobbyVRCharacter::ConfigureLocalVRTracking()
{
	const bool bLocalTrackingOwner = IsLocallyControlled();

	if (CameraComponent)
	{
		CameraComponent->bLockToHmd = bLocalTrackingOwner && !VRMainAnimationCameraState.bActive;
	}

	auto ConfigureMotionControllerTracking = [bLocalTrackingOwner](UMotionControllerComponent* MotionController)
	{
		if (!MotionController)
		{
			return;
		}

		MotionController->SetComponentTickEnabled(bLocalTrackingOwner);
		MotionController->PrimaryComponentTick.SetTickFunctionEnable(bLocalTrackingOwner);
	};

	ConfigureMotionControllerTracking(MotionControllerRightGrip);
	ConfigureMotionControllerTracking(MotionControllerLeftGrip);
	ConfigureMotionControllerTracking(MotionControllerRightAim);
	ConfigureMotionControllerTracking(MotionControllerLeftAim);
}

void ALobbyVRCharacter::ApplyReplicatedArmTransforms()
{
	if (IsLocallyControlled())
	{
		return;
	}

	if (MotionControllerLeftGrip)
	{
		MotionControllerLeftGrip->SetWorldTransform(LeftArm, false, nullptr, ETeleportType::TeleportPhysics);
	}

	if (MotionControllerRightGrip)
	{
		MotionControllerRightGrip->SetWorldTransform(RightArm, false, nullptr, ETeleportType::TeleportPhysics);
	}
}

void ALobbyVRCharacter::ConfigureVRSeatedState()
{
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	if (bIsSitting)
	{
		if (UCharacterMovementComponent* Movement = GetCharacterMovement())
		{
			Movement->DisableMovement();
			Movement->StopMovementImmediately();
		}

		if (bUseSeatedVROriginLocation && VROrigin)
		{
			VROrigin->SetRelativeLocation(SeatedVROriginLocation);
		}
	}

	if (CameraComponent && !VRMainAnimationCameraState.bActive)
	{
		if (CameraComponent->GetAttachParent() != VROrigin)
		{
			CameraComponent->AttachToComponent(VROrigin, FAttachmentTransformRules::KeepRelativeTransform);
		}

		CameraComponent->SetRelativeLocation(FVector::ZeroVector);
		CameraComponent->SetRelativeRotation(FRotator::ZeroRotator);
		CameraComponent->bUsePawnControlRotation = false;
		CameraComponent->bLockToHmd = IsLocallyControlled();
	}
}

void ALobbyVRCharacter::ConfigureWidgetInteraction()
{
	const bool bLocalInteractionOwner = IsLocallyControlled();
	int32 VirtualUserIndex = 0;
	int32 RightPointerIndex = 0;
	int32 LeftPointerIndex = 1;

	if (WidgetInteractionRight)
	{
		WidgetInteractionRight->SetActive(bLocalInteractionOwner, true);
		WidgetInteractionRight->SetComponentTickEnabled(bLocalInteractionOwner);
		WidgetInteractionRight->bEnableHitTesting = bLocalInteractionOwner;
		WidgetInteractionRight->bShowDebug = bLocalInteractionOwner && bShowWidgetInteractionDebug;

		if (bLocalInteractionOwner)
		{
			WidgetInteractionRight->InteractionDistance = VRPointerMaxDistance;
			WidgetInteractionRight->TraceChannel = ECC_Visibility;
			WidgetInteractionRight->InteractionSource = EWidgetInteractionSource::World;
			WidgetInteractionRight->VirtualUserIndex = VirtualUserIndex;
			WidgetInteractionRight->PointerIndex = RightPointerIndex;
		}
	}



	if (WidgetInteractionLeft)
	{
		WidgetInteractionLeft->SetActive(bLocalInteractionOwner, true);
		WidgetInteractionLeft->SetComponentTickEnabled(bLocalInteractionOwner);
		WidgetInteractionLeft->bEnableHitTesting = bLocalInteractionOwner;
		WidgetInteractionLeft->bShowDebug = bLocalInteractionOwner && bShowWidgetInteractionDebug;

		if (bLocalInteractionOwner)
		{
			WidgetInteractionLeft->InteractionDistance = VRPointerMaxDistance;
			WidgetInteractionLeft->TraceChannel = ECC_Visibility;
			WidgetInteractionLeft->InteractionSource = EWidgetInteractionSource::World;
			WidgetInteractionLeft->VirtualUserIndex = VirtualUserIndex;
			WidgetInteractionLeft->PointerIndex = LeftPointerIndex;
		}
	}

}

void ALobbyVRCharacter::ShowReadyWidgetAfterDelay()
{
	if (!IsLocallyControlled()) return;

	UWorld* World = GetWorld();
	if (!World) return;

	World->GetTimerManager().ClearTimer(ReadyWidgetDelayTimerHandle);
	World->GetTimerManager().SetTimer(
		ReadyWidgetDelayTimerHandle,
		this,
		&ALobbyVRCharacter::ShowReadyWidget,
		FMath::Max(ReadyWidgetDelaySeconds, 0.0f),
		false);
}

void ALobbyVRCharacter::SetActiveVRUI(EVRActiveUI ActiveUI)
{
	SetComponentsForVRUIState(EVRActiveUI::MainMenu, ActiveUI == EVRActiveUI::MainMenu);
	SetComponentsForVRUIState(EVRActiveUI::Ready, ActiveUI == EVRActiveUI::Ready);
	SetComponentsForVRUIState(EVRActiveUI::InGame, ActiveUI == EVRActiveUI::InGame);
	SetComponentsForVRUIState(EVRActiveUI::Result, ActiveUI == EVRActiveUI::Result);

	UE_LOG(LogTemp, Warning, TEXT("[VR UI] Active UI changed to %s"), GetVRUIStateLogName(ActiveUI));
}

void ALobbyVRCharacter::SetComponentsForVRUIState(EVRActiveUI UIState, bool bActive)
{
	TArray<FName> ComponentNames;
	switch (UIState)
	{
	case EVRActiveUI::MainMenu:
		ComponentNames.Add(TEXT("MainMenuWidget"));
		break;
	case EVRActiveUI::Ready:
		ComponentNames.Add(TEXT("ReadyWidget"));
		break;
	case EVRActiveUI::InGame:
		ComponentNames.Add(TEXT("MainGameWidget"));
		break;
	case EVRActiveUI::Result:
		ComponentNames.Add(TEXT("ResultWidget"));
		break;
	case EVRActiveUI::None:
	default:
		return;
	}

	TArray<UWidgetComponent*> WidgetComponents;
	GetComponents<UWidgetComponent>(WidgetComponents);

	TSet<UWidgetComponent*> AppliedComponents;
	for (UWidgetComponent* WidgetComponent : WidgetComponents)
	{
		for (const FName& ComponentName : ComponentNames)
		{
			if (DoesWidgetComponentMatchName(WidgetComponent, ComponentName))
			{
				ApplyVRWidgetComponentState(WidgetComponent, bActive);
				AppliedComponents.Add(WidgetComponent);
				break;
			}
		}
	}

	if (UIState == EVRActiveUI::Ready && ReadyWidgetComponent && !AppliedComponents.Contains(ReadyWidgetComponent))
	{
		ApplyVRWidgetComponentState(ReadyWidgetComponent, bActive);
	}
	else if (UIState == EVRActiveUI::Result && ResultWidgetComponent && !AppliedComponents.Contains(ResultWidgetComponent))
	{
		ApplyVRWidgetComponentState(ResultWidgetComponent, bActive);
	}
}

void ALobbyVRCharacter::ApplyVRWidgetComponentState(UWidgetComponent* WidgetComponent, bool bActive)
{
	if (!WidgetComponent)
	{
		return;
	}

	WidgetComponent->SetVisibility(bActive, true);
	WidgetComponent->SetHiddenInGame(!bActive);
	const bool bInteractive = bActive && IsLocallyControlled();
	WidgetComponent->SetCollisionEnabled(bInteractive ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	WidgetComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	WidgetComponent->SetCollisionResponseToChannel(ECC_Visibility, bInteractive ? ECR_Block : ECR_Ignore);
	WidgetComponent->SetGenerateOverlapEvents(false);
}

bool ALobbyVRCharacter::DoesWidgetComponentMatchName(const UWidgetComponent* WidgetComponent, FName ComponentName)
{
	if (!WidgetComponent)
	{
		return false;
	}

	const FString WidgetComponentName = WidgetComponent->GetName();
	const FString TargetName = ComponentName.ToString();
	return WidgetComponentName.Equals(TargetName, ESearchCase::IgnoreCase)
		|| WidgetComponentName.Contains(TargetName, ESearchCase::IgnoreCase);
}

void ALobbyVRCharacter::InitializeMainGameWidgetComponents()
{
	if (!IsLocallyControlled())
	{
		return;
	}

	AMainGamePlayerController* PC = Cast<AMainGamePlayerController>(GetController());
	if (!PC || !PC->IsLocalPlayerController())
	{
		return;
	}

	TArray<UWidgetComponent*> WidgetComponents;
	GetComponents<UWidgetComponent>(WidgetComponents);

	for (UWidgetComponent* WidgetComponent : WidgetComponents)
	{
		if (!DoesWidgetComponentMatchName(WidgetComponent, FName(TEXT("MainGameWidget"))))
		{
			continue;
		}

		WidgetComponent->SetOwnerPlayer(PC->GetLocalPlayer());
		WidgetComponent->InitWidget();

		if (UMainGameWidget* MainGameWidget = Cast<UMainGameWidget>(WidgetComponent->GetUserWidgetObject()))
		{
			MainGameWidget->SetOwningPlayer(PC);
			MainGameWidget->InitWidget();
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[VR UI] MainGameWidget component found but widget object is not UMainGameWidget. Component=%s Widget=%s"),
				*GetNameSafe(WidgetComponent),
				*GetNameSafe(WidgetComponent->GetUserWidgetObject()));
		}
	}
}

void ALobbyVRCharacter::InitializeTurnInfoWidgetComponent()
{
	if (!TurnInfoWidgetComponent)
	{
		return;
	}

	const bool bLocalOwner = IsLocallyControlled();
	TurnInfoWidgetComponent->SetVisibility(bLocalOwner, true);
	TurnInfoWidgetComponent->SetHiddenInGame(!bLocalOwner);
	TurnInfoWidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	if (!bLocalOwner)
	{
		return;
	}

	AMainGamePlayerController* PC = Cast<AMainGamePlayerController>(GetController());
	if (!PC || !PC->IsLocalPlayerController())
	{
		return;
	}

	if (TurnInfoWidgetClass)
	{
		TurnInfoWidgetComponent->SetWidgetClass(TurnInfoWidgetClass);
	}
	if (!TurnInfoWidgetComponent->GetWidgetClass())
	{
		return;
	}

	TurnInfoWidgetComponent->SetOwnerPlayer(PC->GetLocalPlayer());
	TurnInfoWidgetComponent->InitWidget();

	if (UTurnInfoWidget* TurnInfoWidget = Cast<UTurnInfoWidget>(TurnInfoWidgetComponent->GetUserWidgetObject()))
	{
		TurnInfoWidget->SetOwningPlayer(PC);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[VR UI] TurnInfoWidget component found but widget object is not UTurnInfoWidget. Component=%s Widget=%s"),
			*GetNameSafe(TurnInfoWidgetComponent),
			*GetNameSafe(TurnInfoWidgetComponent->GetUserWidgetObject()));
	}
}

void ALobbyVRCharacter::UpdateVRPointers()
{
	if (!IsLocallyControlled())
	{
		return;
	}
	UpdateLaserPointer(MotionControllerLeftAim, TEXT("Left"));
	UpdateLaserPointer(MotionControllerRightAim, TEXT("Right"));

}

void ALobbyVRCharacter::UpdateLaserPointer(const UMotionControllerComponent* AimController, const TCHAR* PointerName) 
{
	const bool bRightHand = AimController == MotionControllerRightAim.Get();
	UNiagaraComponent* Laser = bRightHand ? PointerLaserRight.Get() : PointerLaserLeft.Get();

	FVector Start;
	FVector TraceEnd;
	bool bHasTrace = AimController && GetWorld() && bShowVRPointer;
	if (bHasTrace)
	{
		if (bRightHand)
		{
			bHasTrace = GetRightHandShotTrace(Start, TraceEnd);
		}
		else
		{
			Start = AimController->GetComponentLocation();
			TraceEnd = Start + AimController->GetForwardVector() * VRPointerMaxDistance;
		}
	}

	if (!bHasTrace)
	{
		if (Laser && Laser->IsActive())
		{
			Laser->DeactivateImmediate();
		}
		return;
	}

	FHitResult HitResult;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(VRPointerTrace), false, this);
	const bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, Start, TraceEnd, ECC_Visibility, QueryParams);
	const FVector End = bHit ? HitResult.ImpactPoint : TraceEnd;

	if (Laser)
	{
		UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(Laser, TEXT("User.PointArray"), TArray<FVector>{ Start, End });
		if (!Laser->IsActive())
		{
			Laser->Activate(true);
		}
	}

	AActor* NewHitRevolver = nullptr;

	if (bHit)
	{
		AActor* HitActor = HitResult.GetActor();

		if (IsValid(HitActor) && HitActor->IsA<ARevolver>())
		{
			NewHitRevolver = HitActor;

		}
	}

	if (CurrentFocusedRevolver != NewHitRevolver)
	{
		if (IsValid(CurrentFocusedRevolver))
		{
			SetOutline(CurrentFocusedRevolver, false);
		}

		CurrentFocusedRevolver = NewHitRevolver;

		if (IsValid(CurrentFocusedRevolver))
		{
			SetOutline(CurrentFocusedRevolver, true);
		}
	}

	if (bLogVRPointerHits && bHit)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[LobbyVRCharacter] %s pointer hit actor: %s component: %s blocking: %s distance: %.2f"),
			PointerName,
			*GetNameSafe(HitResult.GetActor()),
			*GetNameSafe(HitResult.GetComponent()),
			HitResult.bBlockingHit ? TEXT("true") : TEXT("false"),
			HitResult.Distance);
	}
}

void ALobbyVRCharacter::DrawSeatDebugCapsule() const
{
	if (!bShowSeatDebugCapsule || !GetWorld())
	{
		return;
	}

	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	if (!Capsule)
	{
		return;
	}

	DrawDebugCapsule(
		GetWorld(),
		Capsule->GetComponentLocation(),
		Capsule->GetScaledCapsuleHalfHeight(),
		Capsule->GetScaledCapsuleRadius(),
		Capsule->GetComponentQuat(),
		FColor::Green,
		false,
		DebugSeatCapsuleDuration,
		0,
		2.0f);
}

void ALobbyVRCharacter::ShowReadyWidget()
{
	if (!IsLocallyControlled() || !ReadyWidgetComponent) return;

	ConfigureWidgetInteraction();

	// Blueprint component overrides must not hide or collapse the local VR UI.
	ReadyWidgetComponent->SetRelativeLocation(ReadyWidgetRelativeLocation);
	ReadyWidgetComponent->SetRelativeRotation(ReadyWidgetRelativeRotation);
	ReadyWidgetComponent->SetRelativeScale3D(FVector(ReadyWidgetWorldScale));
	ReadyWidgetComponent->SetOwnerNoSee(false);
	ReadyWidgetComponent->SetOnlyOwnerSee(false);
	ReadyWidgetComponent->SetRenderInMainPass(true);
	ReadyWidgetComponent->SetDrawAtDesiredSize(false);
	ReadyWidgetComponent->SetDrawSize(ReadyWidgetDrawSize);
	ReadyWidgetComponent->SetPivot(FVector2D(0.5f, 0.5f));
	ReadyWidgetComponent->SetTickWhenOffscreen(true);

	if (ReadyWidgetClass)
	{
		ReadyWidgetComponent->SetWidgetClass(ReadyWidgetClass);
	}

	AMainGamePlayerController* PC = Cast<AMainGamePlayerController>(GetController());
	if (!PC || !PC->IsLocalPlayerController()) return;

	ReadyWidgetComponent->SetOwnerPlayer(PC->GetLocalPlayer());
	ReadyWidgetComponent->InitWidget();

	if (UReadyWidget* ReadyWidget = Cast<UReadyWidget>(ReadyWidgetComponent->GetUserWidgetObject()))
	{
		ReadyWidget->SetOwningPlayer(PC);
		UE_LOG(LogTemp, Warning, TEXT("[VR UI] ReadyWidget owning player set to %s"), *GetNameSafe(PC));
	}

	SetActiveVRUI(EVRActiveUI::Ready);
	ReadyWidgetComponent->RequestRedraw();
	ReadyWidgetComponent->MarkRenderStateDirty();

	UE_LOG(LogTemp, Warning, TEXT("[VR UI] ReadyWidget shown. Local=%s Collision=%d WidgetClass=%s WidgetObject=%s RelativeLocation=%s RelativeRotation=%s RelativeScale=%s DrawSize=%s"),
		IsLocallyControlled() ? TEXT("true") : TEXT("false"),
		static_cast<int32>(ReadyWidgetComponent->GetCollisionEnabled()),
		*GetNameSafe(ReadyWidgetComponent->GetWidgetClass()),
		*GetNameSafe(ReadyWidgetComponent->GetUserWidgetObject()),
		*ReadyWidgetComponent->GetRelativeLocation().ToString(),
		*ReadyWidgetComponent->GetRelativeRotation().ToString(),
		*ReadyWidgetComponent->GetRelativeScale3D().ToString(),
		*ReadyWidgetComponent->GetDrawSize().ToString());
}

void ALobbyVRCharacter::HideReadyWidget()
{
	if (!ReadyWidgetComponent) return;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReadyWidgetDelayTimerHandle);
	}

	SetActiveVRUI(EVRActiveUI::None);


	UE_LOG(LogTemp, Warning, TEXT("[VR UI] ReadyWidget hidden"));
}

void ALobbyVRCharacter::ShowMainGameWidget()
{
	if (!IsLocallyControlled()) return;

	ConfigureWidgetInteraction();
	ApplyVRWidgetComponentState(TurnInfoWidgetComponent, false);
	InitializeMainGameWidgetComponents();
	SetActiveVRUI(EVRActiveUI::InGame);
}

void ALobbyVRCharacter::HideMainGameWidget()
{
	if (!IsLocallyControlled()) return;

	SetComponentsForVRUIState(EVRActiveUI::InGame, false);
	UE_LOG(LogTemp, Warning, TEXT("[VR UI] MainGameWidget hidden"));
}

void ALobbyVRCharacter::ShowResultWidget(const FString& WinnerName, int32 WinnerPlayerId)
{
	if (!IsLocallyControlled() || !ResultWidgetComponent) return;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReadyWidgetDelayTimerHandle);
	}

	SetActiveVRUI(EVRActiveUI::None);
	ConfigureWidgetInteraction();


	if (ResultWidgetClass)
	{
		ResultWidgetComponent->SetWidgetClass(ResultWidgetClass);
	}

	AMainGamePlayerController* PC = Cast<AMainGamePlayerController>(GetController());
	if (!PC || !PC->IsLocalPlayerController()) return;

	ResultWidgetComponent->SetOwnerPlayer(PC->GetLocalPlayer());
	ResultWidgetComponent->InitWidget();

	if (UGameResultWidget* ResultWidget = Cast<UGameResultWidget>(ResultWidgetComponent->GetUserWidgetObject()))
	{
		ResultWidget->SetOwningPlayer(PC);
		ResultWidget->SetResult(WinnerName, PC->GetPlayerIdSafe() == WinnerPlayerId);
	}

	SetActiveVRUI(EVRActiveUI::Result);

	UE_LOG(LogTemp, Warning, TEXT("[VR UI] ResultWidget shown. Winner=%s WidgetClass=%s WidgetObject=%s"),
		*WinnerName,
		*GetNameSafe(ResultWidgetComponent->GetWidgetClass()),
		*GetNameSafe(ResultWidgetComponent->GetUserWidgetObject()));
}
void ALobbyVRCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ALobbyVRCharacter, NetLeftArm, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(ALobbyVRCharacter, NetRightArm, COND_SkipOwner);
}

void ALobbyVRCharacter::GrabGun()
{
	if (const AMainPlayerState* PS = GetPlayerState<AMainPlayerState>(); PS && !PS->isAlive) return;
	if (MotionControllerRightGrip && GetWorld())
	{
		const FVector Start = MotionControllerRightGrip->GetComponentLocation();
		const FVector TraceEnd = Start + MotionControllerRightGrip->GetForwardVector() * VRPointerMaxDistance;

		FHitResult HitResult;
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(VRGunGrabTrace), false, this);
		const bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, Start, TraceEnd, ECC_Visibility, QueryParams);

		UE_LOG(LogTemp, Warning,
			TEXT("[VR Gun] GrabGun input. Char=%s Local=%s Reason=%d ActiveRevolver=%s Hit=%s Actor=%s Component=%s Distance=%.2f"),
			*GetNameSafe(this),
			IsLocallyControlled() ? TEXT("true") : TEXT("false"),
			static_cast<int32>(GunHoldReason),
			*GetNameSafe(ActiveRevolver),
			bHit ? TEXT("true") : TEXT("false"),
			*GetNameSafe(HitResult.GetActor()),
			*GetNameSafe(HitResult.GetComponent()),
			bHit ? HitResult.Distance : -1.0f);
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[VR Gun] GrabGun input. Char=%s Local=%s Reason=%d ActiveRevolver=%s RightGrip=%s World=%s"),
			*GetNameSafe(this),
			IsLocallyControlled() ? TEXT("true") : TEXT("false"),
			static_cast<int32>(GunHoldReason),
			*GetNameSafe(ActiveRevolver),
			*GetNameSafe(MotionControllerRightGrip),
			GetWorld() ? TEXT("valid") : TEXT("null"));
	}

	if (ActiveRevolver && ActiveRevolver->ActorHasTag(FName("MainRevolver")))
	{
		if (!bMainRevolverGrabInProgress && !bMainRevolverGrabbed && !bIsPuttingBackGun)
			Server_GrabMainRevolver();
		return;
	}

	if (GunHoldReason != EGunHoldReason::Win || !ActiveRevolver)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[VR Gun] GrabGun blocked. Need Reason=Win(%d) and ActiveRevolver. Reason=%d ActiveRevolver=%s"),
			static_cast<int32>(EGunHoldReason::Win),
			static_cast<int32>(GunHoldReason),
			*GetNameSafe(ActiveRevolver));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[VR Gun] Non-main revolver grab path. Revolver=%s"), *GetNameSafe(ActiveRevolver));
	AttachRevolverToSocket();
	DrawMainShotAimLine();
}

void ALobbyVRCharacter::Server_GrabMainRevolver_Implementation()
{
	if (const AMainPlayerState* PS = GetPlayerState<AMainPlayerState>(); PS && !PS->isAlive) return;
	if (bMainRevolverGrabInProgress || bMainRevolverGrabbed || bIsPuttingBackGun) return;
	if (!ActiveRevolver || !ActiveRevolver->ActorHasTag(FName("MainRevolver")) || !GetWorld())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[VR Gun] Server_GrabMainRevolver blocked. Char=%s Reason=%d ActiveRevolver=%s IsMain=%s"),
			*GetNameSafe(this),
			static_cast<int32>(GunHoldReason),
			*GetNameSafe(ActiveRevolver),
			ActiveRevolver && ActiveRevolver->ActorHasTag(FName("MainRevolver")) ? TEXT("true") : TEXT("false"));
		return;
	}

	bMainRevolverGrabInProgress = true;
#if WITH_SERVER_CODE
	if (AMainGameMode* GM = GetWorld()->GetAuthGameMode<AMainGameMode>())
		GM->HandleMainRevolverGrabStarted(this);
#endif
	Multicast_PlayVRMainRevolverGrabAnimation(ActiveRevolver.Get());
}

// PC처럼 집기 시작과 완료를 분리하고, 완료 콜백에서만 사격 타이머로 전환합니다.
void ALobbyVRCharacter::Multicast_PlayVRMainRevolverGrabAnimation_Implementation(ARevolver* Revolver)
{
    if (IsDeathPresentationActive()) return;
    if (!IsValid(Revolver)) return;
    ActiveRevolver = Revolver;
    bMainRevolverGrabInProgress = true;
    bMainRevolverGrabbed = false;
    bIsPuttingBackGun = false;
    GunHoldReason = EGunHoldReason::Win;
    SetMainShotAimLineVisible(false);
    if (!PlayVRMainRevolverAnimation(Revolver, false))
        OnGrabGunMontageEnded(nullptr, false);
}

void ALobbyVRCharacter::Multicast_CompleteVRMainRevolverGrab_Implementation()
{
    if (IsDeathPresentationActive()) return;
    RestoreVRMainGrabPresentation();
    if (GunHoldReason != EGunHoldReason::Win || !ActiveRevolver) return;
    AttachMainRevolverToRightGrip();
    bMainRevolverGrabInProgress = false;
    bMainRevolverGrabbed = true;
}

void ALobbyVRCharacter::Multicast_ClearVRMainRevolver_Implementation()
{
    if (IsDeathPresentationActive()) return;
    RestoreVRMainGrabPresentation();
    bMainRevolverGrabInProgress = false;
    bMainRevolverGrabbed = false;
    bIsPuttingBackGun = false;
    GunHoldReason = EGunHoldReason::None;
    SetMainShotAimLineVisible(false);
}

bool ALobbyVRCharacter::PlayVRMainRevolverAnimation(ARevolver* Revolver, bool bPuttingBack)
{
    if (IsDeathPresentationActive()) return false;
    if (!IsValid(Revolver) || !Revolver->WeaponMesh || !MetaHumanBodyAnimation || !MainRevolverAnimationBody
        || !MainRevolverAnimationClass || !VRAnimationRevolverMesh) return false;
    const bool bFold = GunHoldReason == EGunHoldReason::Fold;
    UAnimMontage* Montage = bPuttingBack
        ? (bFold ? EndAimMyselfMontage.Get() : WinEndMontage.Get())
        : (bFold ? AimMyselfMontage.Get() : WinAimMontage.Get());
    if (!Montage) return false;
    if (bVRMainGrabPresentationActive && bVRMainPutBackPresentation == bPuttingBack) return true;
    RestoreVRMainGrabPresentation();

    // 기존 폴드용 설정은 보관하고 메인 총 연출 동안에만 메시/AnimBP를 적용합니다.
    USkeletalMeshComponent* AnimationMeshes[] = {MetaHumanBodyAnimation, MetaHumanTorsoAnimation, MetaHumanFaceAnimation};
    USkeletalMesh* MeshAssets[] = {MainRevolverAnimationBody.Get(), MainRevolverAnimationTorso.Get(), MainRevolverAnimationFace.Get()};
    for (int32 Index = 0; Index < 3; ++Index)
    {
        USkeletalMeshComponent* Component = AnimationMeshes[Index];
        if (!Component) continue;
        FVRAnimationAssetState& State = VRAnimationAssetStates.AddDefaulted_GetRef();
        State.Component = Component;
        State.Mesh = Component->GetSkeletalMeshAsset();
        State.AnimClass = Component->GetAnimClass();
        Component->SetSkeletalMeshAsset(MeshAssets[Index]);
    }
    MetaHumanBodyAnimation->SetAnimInstanceClass(MainRevolverAnimationClass);
    UAnimInstance* Anim = MetaHumanBodyAnimation->GetAnimInstance();
    if (!Anim) { RestoreVRMainGrabPresentation(); return false; }

    TArray<UPrimitiveComponent*> Components;
    GetComponents<UPrimitiveComponent>(Components, true);
    if (Revolver->WeaponMesh) Components.AddUnique(Revolver->WeaponMesh);
    for (UPrimitiveComponent* Component : Components)
    {
        if (!IsValid(Component) ||
            (!Component->IsA<USkeletalMeshComponent>() && !Component->IsA<UGroomComponent>())) continue;
        FVRGrabMeshState& State = VRGrabMeshStates.AddDefaulted_GetRef();
        State.Component = Component;
        State.bVisible = Component->GetVisibleFlag();
        State.bHiddenInGame = Component->bHiddenInGame;
        State.bOwnerNoSee = Component->bOwnerNoSee;
        State.bOnlyOwnerSee = Component->bOnlyOwnerSee;
        const bool bAnimationMesh = Component == MetaHumanBodyAnimation
            || Component == MetaHumanTorsoAnimation || Component == MetaHumanFaceAnimation;
        // 집기 Notify 전까지는 책상 위 총을 그대로 표시합니다.
        if (Component != Revolver->WeaponMesh || bPuttingBack)
            Component->SetVisibility(bAnimationMesh, false);
        if (bAnimationMesh)
        {
            Component->SetHiddenInGame(false, false);
            Component->SetOnlyOwnerSee(false);
            Component->SetOwnerNoSee(Component == MetaHumanFaceAnimation);
        }
    }
    VRAnimationRevolverMesh->SetSkeletalMeshAsset(Revolver->WeaponMesh->GetSkeletalMeshAsset());
    const FName Socket = MetaHumanBodyAnimation->DoesSocketExist(FName("Revolver"))
        ? FName("Revolver") : FName("hand_r");
    VRAnimationRevolverMesh->AttachToComponent(MetaHumanBodyAnimation,
        FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket);
    VRAnimationRevolverMesh->SetHiddenInGame(false);
    VRAnimationRevolverMesh->SetVisibility(bPuttingBack);
    bVRMainGrabPresentationActive = true;
    bVRMainPutBackPresentation = bPuttingBack;
    if (Anim->Montage_Play(Montage) <= 0.0f)
    {
        RestoreVRMainGrabPresentation();
        return false;
    }
    FOnMontageEnded Delegate;
    Delegate.BindUObject(this, &ALobbyVRCharacter::OnVRMainRevolverAnimationEnded);
    Anim->Montage_SetEndDelegate(Delegate, Montage);

    if (IsLocallyControlled() && CameraComponent && MetaHumanBodyAnimation->DoesSocketExist(FName("head")))
    {
        VRMainAnimationCameraState.Parent = CameraComponent->GetAttachParent();
        VRMainAnimationCameraState.Socket = CameraComponent->GetAttachSocketName();
        VRMainAnimationCameraState.RelativeTransform = CameraComponent->GetRelativeTransform();
        VRMainAnimationCameraState.bLockToHmd = CameraComponent->bLockToHmd;
        VRMainAnimationCameraState.bUsePawnControlRotation = CameraComponent->bUsePawnControlRotation;
        VRMainAnimationCameraState.bActive = true;

        // BP에는 카메라가 둘 있으므로 실제 시점도 지정 카메라 하나만 사용하게 합니다.
        TInlineComponentArray<UCameraComponent*> Cameras(this);
        for (UCameraComponent* Camera : Cameras)
        {
            VRMainAnimationCameraState.CameraActivationStates.Emplace(Camera, Camera->IsActive());
            Camera->SetActive(Camera == CameraComponent);
        }

        // PC와 같은 눈 위치로 머리 애니메이션을 따라가며, 종료 시 HMD 추적을 복구합니다.
        CameraComponent->bLockToHmd = false;
        CameraComponent->bUsePawnControlRotation = false;
        CameraComponent->AttachToComponent(MetaHumanBodyAnimation,
            FAttachmentTransformRules::SnapToTargetNotIncludingScale, FName("head"));
        CameraComponent->SetRelativeLocationAndRotation(
            FVector(-2.8f, 8.5f, 0.0f), FRotator(0.0f, 90.0f, -90.0f));
    }
    return true;
}

void ALobbyVRCharacter::RestoreVRMainGrabPresentation()
{
    // 메시를 원래 에셋으로 되돌리기 전에 카메라를 원래 부모로 옮깁니다.
    if (VRMainAnimationCameraState.bActive && CameraComponent)
    {
        if (USceneComponent* Parent = VRMainAnimationCameraState.Parent.Get())
            CameraComponent->AttachToComponent(Parent, FAttachmentTransformRules::KeepRelativeTransform,
                VRMainAnimationCameraState.Socket);
        else
            CameraComponent->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
        CameraComponent->SetRelativeTransform(VRMainAnimationCameraState.RelativeTransform);
        CameraComponent->bLockToHmd = VRMainAnimationCameraState.bLockToHmd;
        CameraComponent->bUsePawnControlRotation = VRMainAnimationCameraState.bUsePawnControlRotation;
    }
    for (const auto& State : VRMainAnimationCameraState.CameraActivationStates)
        if (UCameraComponent* Camera = State.Key.Get()) Camera->SetActive(State.Value);
    VRMainAnimationCameraState = FVRMainAnimationCameraState();
    bVRMainGrabPresentationActive = false;
    bVRMainPutBackPresentation = false;
    if (MetaHumanBodyAnimation)
        if (UAnimInstance* Anim = MetaHumanBodyAnimation->GetAnimInstance())
        {
            if (WinAimMontage) Anim->Montage_Stop(0.0f, WinAimMontage);
            if (WinEndMontage) Anim->Montage_Stop(0.0f, WinEndMontage);
            if (AimMyselfMontage) Anim->Montage_Stop(0.0f, AimMyselfMontage);
            if (EndAimMyselfMontage) Anim->Montage_Stop(0.0f, EndAimMyselfMontage);
        }
    const bool bHadHiddenMeshes = VRGrabMeshStates.Num() > 0;
    for (const FVRGrabMeshState& State : VRGrabMeshStates)
        if (UPrimitiveComponent* Component = State.Component.Get())
        {
            Component->SetVisibility(State.bVisible, false);
            Component->SetHiddenInGame(State.bHiddenInGame, false);
            Component->SetOwnerNoSee(State.bOwnerNoSee);
            Component->SetOnlyOwnerSee(State.bOnlyOwnerSee);
        }
    VRGrabMeshStates.Reset();
    for (const FVRAnimationAssetState& State : VRAnimationAssetStates)
        if (USkeletalMeshComponent* Component = State.Component.Get())
        {
            Component->SetSkeletalMeshAsset(State.Mesh);
            Component->SetAnimInstanceClass(State.AnimClass);
        }
    VRAnimationAssetStates.Reset();
    if (bHadHiddenMeshes)
        ForceRestoreTrackingMeshes();
}

void ALobbyVRCharacter::AttachRevolverToSocket()
{
    if (bVRMainGrabPresentationActive)
    {
        if (ActiveRevolver && ActiveRevolver->WeaponMesh)
            ActiveRevolver->WeaponMesh->SetVisibility(false);
        VRAnimationRevolverMesh->SetVisibility(true);
        return;
    }
    Super::AttachRevolverToSocket();
}

void ALobbyVRCharacter::ReturnRevolverToDesk()
{
    if (bVRMainGrabPresentationActive)
    {
        VRAnimationRevolverMesh->SetVisibility(false);
        return;
    }
    Super::ReturnRevolverToDesk();
}

void ALobbyVRCharacter::OnVRMainRevolverAnimationEnded(UAnimMontage* Montage, bool bInterrupted)
{
    if (!bVRMainGrabPresentationActive || !Montage) return;
    const bool bPuttingBack = bVRMainPutBackPresentation;
    const bool bFoldMontage = Montage == AimMyselfMontage.Get() || Montage == EndAimMyselfMontage.Get();
    const bool bExpectedMontage = bPuttingBack
        ? (Montage == WinEndMontage.Get() || Montage == EndAimMyselfMontage.Get())
        : (Montage == WinAimMontage.Get() || Montage == AimMyselfMontage.Get());
    if (!bExpectedMontage) return;
    RestoreVRMainGrabPresentation();

    if (bFoldMontage)
    {
        if (bPuttingBack)
        {
            Super::ReturnRevolverToDesk();
            OnPutBackGunMontageEnded(Montage, bInterrupted);
        }
        else
        {
            Super::AttachRevolverToSocket();
            Super::OnGrabGunMontageEnded(Montage, bInterrupted);
        }
        return;
    }

    if (bPuttingBack) OnPutBackGunMontageEnded(Montage, bInterrupted);
    else
    {
        bMainRevolverGrabInProgress = false;
        OnGrabGunMontageEnded(Montage, bInterrupted);
    }
}

void ALobbyVRCharacter::PlayPutBackGunMontage(EGunHoldReason Reason)
{
    if (IsDeathPresentationActive()) return;
    if (Reason != EGunHoldReason::Win && Reason != EGunHoldReason::Fold)
    {
        Super::PlayPutBackGunMontage(Reason);
        return;
    }
    GunHoldReason = Reason;
    ARevolver* Revolver = (Reason == EGunHoldReason::Fold && DeskRevolver)
        ? DeskRevolver.Get() : ActiveRevolver.Get();
    if (!PlayVRMainRevolverAnimation(Revolver, true))
    {
        Super::PlayPutBackGunMontage(Reason);
        return;
    }
    // 기존 반환 처리와 같은 상태 갱신과 종료 콜백을 사용합니다.
    bIsPuttingBackGun = true;
    SetMainShotAimLineVisible(false);
    FinishedReason = GunHoldReason;
    GunHoldReason = EGunHoldReason::None;
}

void ALobbyVRCharacter::ReturnMainRevolverToTableImmediately()
{
    if (HasAuthority()) Multicast_ClearVRMainRevolver();
    else RestoreVRMainGrabPresentation();
    Super::ReturnMainRevolverToTableImmediately();
}

void ALobbyVRCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    RestoreVRMainGrabPresentation();
    Super::EndPlay(EndPlayReason);
}

void ALobbyVRCharacter::AttachMainRevolverToRightGrip()
{
	if (!MotionControllerRightGrip || !ActiveRevolver)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[VR Gun] AttachMainRevolverToRightGrip blocked. RightGrip=%s ActiveRevolver=%s"),
			*GetNameSafe(MotionControllerRightGrip),
			*GetNameSafe(ActiveRevolver));
		return;
	}

	ActiveRevolver->SetActorHiddenInGame(false);
	ActiveRevolver->AttachToComponent(MotionControllerRightGrip, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	ActiveRevolver->SetActorRelativeLocation(FVector::ZeroVector);
	ActiveRevolver->SetActorRelativeRotation(FRotator::ZeroRotator);

	if (ActiveRevolver->CollisionSphere)
	{
		ActiveRevolver->CollisionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	bShowMainShotAimLine = true;
	DrawMainShotAimLine();
	ActiveRevolver->ForceNetUpdate();

	UE_LOG(LogTemp, Warning,
		TEXT("[VR Gun] MainRevolver attached to right grip. Char=%s Revolver=%s GripLocation=%s RevolverLocation=%s Authority=%s"),
		*GetNameSafe(this),
		*GetNameSafe(ActiveRevolver),
		*MotionControllerRightGrip->GetComponentLocation().ToString(),
		*ActiveRevolver->GetActorLocation().ToString(),
		HasAuthority() ? TEXT("true") : TEXT("false"));
}

// VR은 기존 파란선과 같은 RightAim 구간을 사용합니다.
bool ALobbyVRCharacter::GetMainShotTrace(float TraceDistance, FVector& OutStart, FVector& OutEnd) const
{
    return GetRightHandShotTrace(OutStart, OutEnd);
}

void ALobbyVRCharacter::PlayGrabGunMontage(EGunHoldReason Reason)
{
	if (IsDeathPresentationActive()) return;
	GunHoldReason = Reason;

	if (Reason == EGunHoldReason::Fold)
	{
		ARevolver* Revolver = DeskRevolver ? DeskRevolver.Get() : ActiveRevolver.Get();
		if (PlayVRMainRevolverAnimation(Revolver, false))
		{
			return;
		}
	}

	Super::PlayGrabGunMontage(Reason);
}

void ALobbyVRCharacter::PrepareDeathPresentation()
{
	RestoreVRMainGrabPresentation();
}

void ALobbyVRCharacter::ForceRestoreTrackingMeshes()
{
	USkeletalMeshComponent* ThirdPersonMeshes[] = {
		ThirdPersonMetaHumanBody, ThirdPersonMetaHumanTorso, ThirdPersonMetaHumanFace};
	for (USkeletalMeshComponent* Component : ThirdPersonMeshes)
	{
		if (!Component) continue;
		Component->SetVisibility(true, false);
		Component->SetHiddenInGame(false, false);
		Component->SetOnlyOwnerSee(false);
		Component->SetOwnerNoSee(true);
	}

	UGroomComponent* Grooms[] = {
		ThirdPersonMetaHumanHair, ThirdPersonMetaHumanEyebrows, ThirdPersonMetaHumanBeard,
		ThirdPersonMetaHumanMustache, ThirdPersonMetaHumanEyelashes, ThirdPersonMetaHumanFuzz};
	for (UGroomComponent* Component : Grooms)
	{
		if (!Component) continue;
		Component->SetVisibility(true, false);
		Component->SetHiddenInGame(false, false);
		Component->SetOnlyOwnerSee(false);
		Component->SetOwnerNoSee(true);
	}

	USkeletalMeshComponent* Hands[] = {HandRight.Get(), HandLeft.Get()};
	for (USkeletalMeshComponent* Component : Hands)
	{
		if (!Component) continue;
		Component->SetVisibility(true, false);
		Component->SetHiddenInGame(false, false);
		Component->SetOnlyOwnerSee(true);
		Component->SetOwnerNoSee(false);
	}

	USkeletalMeshComponent* AnimationMeshes[] = {
		MetaHumanBodyAnimation, MetaHumanTorsoAnimation, MetaHumanFaceAnimation};
	for (USkeletalMeshComponent* Component : AnimationMeshes)
	{
		if (Component) Component->SetVisibility(false, false);
	}
	if (VRAnimationRevolverMesh) VRAnimationRevolverMesh->SetVisibility(false, false);
}
