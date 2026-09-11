#include "Character/LobbyVRCharacter.h"

#include "Actor/Revolver.h"
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
#include "PlayerController/MainGamePlayerController.h"
#include "Widget/GameResultWidget.h"
#include "Widget/MainGameWidget.h"
#include "Widget/ReadyWidget.h"
#include "Widget/TurnInfoWidget.h"
#include "Kismet/KismetMathLibrary.h"
#include "Net/UnrealNetwork.h"
#include "EnhancedInputComponent.h"

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
	InitializeTurnInfoWidgetComponent();
}


void ALobbyVRCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	ConfigureLocalVRTracking();
	ConfigureWidgetInteraction();
	InitializeTurnInfoWidgetComponent();
}

void ALobbyVRCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	ConfigureLocalVRTracking();
	ConfigureWidgetInteraction();
	InitializeTurnInfoWidgetComponent();
}

void ALobbyVRCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();
	ConfigureLocalVRTracking();
	ConfigureWidgetInteraction();
	InitializeTurnInfoWidgetComponent();
}

void ALobbyVRCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateVRPointers();
	UpdateArmPosition();
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

	bIsSitting = true;
	bIsSittingEnded = true;
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

	bIsSitting = true;
	bIsSittingEnded = true;
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
	ShowResultWidget(WinnerName, WinnerPlayerId);
}

void ALobbyVRCharacter::PressRightWidgetInteraction()
{
	if (!IsLocallyControlled() || !WidgetInteractionRight || !WidgetInteractionRight->IsActive())
	{
		return;
	}

	WidgetInteractionRight->PressPointerKey(EKeys::LeftMouseButton);
}

void ALobbyVRCharacter::ReleaseRightWidgetInteraction()
{
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
	if (!IsLocallyControlled() || !WidgetInteractionLeft || !WidgetInteractionLeft->IsActive())
	{
		return;
	}

	WidgetInteractionLeft->PressPointerKey(EKeys::LeftMouseButton);
}

void ALobbyVRCharacter::ReleaseLeftWidgetInteraction()
{
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

void ALobbyVRCharacter::OnRep_IsSitting()
{
	ConfigureVRSeatedState();
	ConfigureWidgetInteraction();
}

void ALobbyVRCharacter::Server_UpdateArm_Implementation(const FTransform& NewLeftArm, const FTransform& NewRightArm, const FTransform& NewRightAim)
{
	LeftArm = NewLeftArm;
	RightArm = NewRightArm;
	RightAimTransform = NewRightAim;
	bHasRightAimTransform = !NewRightAim.ContainsNaN();
	ApplyReplicatedArmTransforms();
}

void ALobbyVRCharacter::OnRep_ArmTransforms()
{
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
		CameraComponent->bLockToHmd = bLocalTrackingOwner;
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
	}

	if (CameraComponent)
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

	UpdateLaserPointer(MotionControllerRightAim, TEXT("Right"));
	UpdateLaserPointer(MotionControllerLeftAim, TEXT("Left"));
}

void ALobbyVRCharacter::UpdateLaserPointer(const UMotionControllerComponent* AimController, const TCHAR* PointerName) const
{
	if (!AimController || !GetWorld() || !bShowVRPointer)
	{
		return;
	}

	FVector Start;
	FVector TraceEnd;
	if (AimController == MotionControllerRightAim.Get())
	{
		if (!GetRightHandShotTrace(Start, TraceEnd)) return;
	}
	else
	{
		Start = AimController->GetComponentLocation();
		TraceEnd = Start + AimController->GetForwardVector() * VRPointerMaxDistance;
	}

	FHitResult HitResult;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(VRPointerTrace), false, this);
	const bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, Start, TraceEnd, ECC_Visibility, QueryParams);
	const FVector End = bHit ? HitResult.ImpactPoint : TraceEnd;

	const FColor LineColor = FColor(80, 220, 255);
	DrawDebugLine(
		GetWorld(),
		Start,
		End,
		LineColor,
		false,
		0.0f,
		0,
		LaserThickness);

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

	UE_LOG(LogTemp, Warning, TEXT("[VR UI] ReadyWidget shown. Local=%s Collision=%d WidgetClass=%s WidgetObject=%s"),
		IsLocallyControlled() ? TEXT("true") : TEXT("false"),
		static_cast<int32>(ReadyWidgetComponent->GetCollisionEnabled()),
		*GetNameSafe(ReadyWidgetComponent->GetWidgetClass()),
		*GetNameSafe(ReadyWidgetComponent->GetUserWidgetObject()));
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
	InitializeTurnInfoWidgetComponent();
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

	DOREPLIFETIME(ALobbyVRCharacter, LeftArm);
	DOREPLIFETIME(ALobbyVRCharacter, RightArm);
}

void ALobbyVRCharacter::GrabGun()
{
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

	if (GunHoldReason != EGunHoldReason::Win || !ActiveRevolver)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[VR Gun] GrabGun blocked. Need Reason=Win(%d) and ActiveRevolver. Reason=%d ActiveRevolver=%s"),
			static_cast<int32>(EGunHoldReason::Win),
			static_cast<int32>(GunHoldReason),
			*GetNameSafe(ActiveRevolver));
		return;
	}

	if (ActiveRevolver->ActorHasTag(FName("MainRevolver")))
	{
		AttachMainRevolverToRightGrip();

		if (!HasAuthority())
		{
			Server_GrabMainRevolver();
		}

		UE_LOG(LogTemp, Warning, TEXT("[VR Gun] MainRevolver grab requested. Revolver=%s"), *GetNameSafe(ActiveRevolver));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[VR Gun] Non-main revolver grab path. Revolver=%s"), *GetNameSafe(ActiveRevolver));
	AttachRevolverToSocket();
	DrawMainShotAimLine();
}

void ALobbyVRCharacter::Server_GrabMainRevolver_Implementation()
{
	if (GunHoldReason != EGunHoldReason::Win || !ActiveRevolver || !ActiveRevolver->ActorHasTag(FName("MainRevolver")))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[VR Gun] Server_GrabMainRevolver blocked. Char=%s Reason=%d ActiveRevolver=%s IsMain=%s"),
			*GetNameSafe(this),
			static_cast<int32>(GunHoldReason),
			*GetNameSafe(ActiveRevolver),
			ActiveRevolver && ActiveRevolver->ActorHasTag(FName("MainRevolver")) ? TEXT("true") : TEXT("false"));
		return;
	}

	AttachMainRevolverToRightGrip();
	UE_LOG(LogTemp, Warning, TEXT("[VR Gun] Server attached MainRevolver. Char=%s Revolver=%s"), *GetNameSafe(this), *GetNameSafe(ActiveRevolver));
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

	const bool bWasMainRevolverGrabbed = IsMainRevolverGrabbed();

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

	if (HasAuthority() && ActiveRevolver->ActorHasTag(FName("MainRevolver")))
	{
		MarkMainRevolverGrabbed();

#if WITH_SERVER_CODE
		if (!bWasMainRevolverGrabbed)
		{
			if (AMainGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AMainGameMode>() : nullptr)
			{
				GM->HandleMainRevolverGrabbed(this);
			}
		}
#endif
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[VR Gun] MainRevolver attached to right grip. Char=%s Revolver=%s GripLocation=%s RevolverLocation=%s Authority=%s"),
		*GetNameSafe(this),
		*GetNameSafe(ActiveRevolver),
		*MotionControllerRightGrip->GetComponentLocation().ToString(),
		*ActiveRevolver->GetActorLocation().ToString(),
		HasAuthority() ? TEXT("true") : TEXT("false"));
}
