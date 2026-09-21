#include "Character/LobbyPCCharacter.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "Kismet/KismetMathLibrary.h"
#include "Interface/InteractableInterface.h"
#include "Actor/SeatActor.h"
#include "Actor/Revolver.h"
#include "Animation/AnimInstance.h"
#include "PCCrosshairWidget.h"
#include "UObject/ConstructorHelpers.h"

// 기존 카메라와 메시를 재사용하고 PC 입력에 필요한 기본값을 설정합니다.
ALobbyPCCharacter::ALobbyPCCharacter()
{
    static ConstructorHelpers::FClassFinder<UPCCrosshairWidget> CrosshairBP(
        TEXT("/Game/Blueprint/Widget/WBP_PCCrosshairWidget"));
    if (CrosshairBP.Succeeded()) PCCrosshairWidgetClass = CrosshairBP.Class;
    bUseControllerRotationYaw = true;
    bUseControllerRotationPitch = false;
    bUseControllerRotationRoll = false;
    GetCharacterMovement()->bOrientRotationToMovement = false;
    CameraComponent->SetupAttachment(FirstPersonMetaHumanBody, FName("head"));
    CameraComponent->SetRelativeLocationAndRotation(FVector(-2.8f, 8.5f, 0.0f), FRotator(0.0f, 90.0f, -90.0f));
    CameraComponent->bUsePawnControlRotation = true;
    CameraComponent->bLockToHmd = false;
}

// BP 기본값 적용 후에도 PC 카메라의 HMD 추적은 끕니다.
void ALobbyPCCharacter::BeginPlay()
{
    Super::BeginPlay();
    if (CameraComponent) CameraComponent->bLockToHmd = false;
    if (FirstPersonMetaHumanBody)
        DefaultFirstPersonAnimClass = FirstPersonMetaHumanBody->GetAnimClass();
}

// 착석 중에는 이동하지 않습니다.
void ALobbyPCCharacter::Move(const FVector2D& Axis)
{
    if (!IsLocallyControlled() || !Controller || bIsSitting) return;
    AddMovementInput(GetActorForwardVector(), Axis.Y);
    AddMovementInput(GetActorRightVector(), Axis.X);
}

// 기존 마우스 감도와 축 방향을 유지합니다.
void ALobbyPCCharacter::Look(const FVector2D& Axis, float Sensitivity)
{
    if (!IsLocallyControlled() || !Controller) return;
    AddControllerYawInput(Axis.X * Sensitivity);
    AddControllerPitchInput(-Axis.Y * Sensitivity);
}

// 게임모드의 공통 착석 요청을 PC 즉시 착석에 연결합니다.
void ALobbyPCCharacter::InitSeatedAtSeat(ASeatActor* TargetSeat)
{
    InitPCSeatedAtSeat(TargetSeat);
}

// PC 피격 판정은 화면 정중앙 시점을 사용합니다.
bool ALobbyPCCharacter::GetMainShotTrace(float TraceDistance, FVector& OutStart, FVector& OutEnd) const
{
    const APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC) return false;
    FRotator ViewRotation;
    PC->GetPlayerViewPoint(OutStart, ViewRotation);
    OutEnd = OutStart + ViewRotation.Vector() * TraceDistance;
    return true;
}

// PC 표시를 멀티캐스트로 정리한 뒤 공통 총 반환 처리를 실행합니다.
void ALobbyPCCharacter::ReturnMainRevolverToTableImmediately()
{
    if (HasAuthority()) Multicast_ClearPCMainRevolver();
    Super::ReturnMainRevolverToTableImmediately();
}

void ALobbyPCCharacter::InitPCSeatedAtSeat(ASeatActor* TargetSeat)
{
	if (!HasAuthority() || !TargetSeat || !TargetSeat->SitTarget) return;

	// 좌석 및 좌석의 총 지정
	CurrentSeat = TargetSeat;
	DeskRevolver = TargetSeat->DeskRevolver;

	FVector Location = TargetSeat->SitTarget->GetComponentLocation();
	Location.Z += GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + PCSeatHeightOffset;
	const FRotator Rotation(0.0f, TargetSeat->SitTarget->GetComponentRotation().Yaw, 0.0f);

	SetActorLocationAndRotation(Location, Rotation, false, nullptr, ETeleportType::TeleportPhysics);
	CompleteSeatedState();

	Client_InitPCSeated(Location, Rotation);
	ForceNetUpdate();
}

void ALobbyPCCharacter::Client_InitPCSeated_Implementation(FVector Location, FRotator Rotation)
{
	SetActorLocationAndRotation(Location, Rotation, false, nullptr, ETeleportType::TeleportPhysics);
	CompleteSeatedState();
	OnRep_IsSitting();
}

void ALobbyPCCharacter::Multicast_BeginPCMainRevolver_Implementation(ARevolver* Revolver)
{
	if (!Revolver) return;

	// RPC에 총 참조를 함께 보내 속성 복제보다 몽타주가 먼저 시작되는 경우를 처리합니다.
	ActiveRevolver = Revolver;
	bMainRevolverGrabbed = false;
	bIsPuttingBackGun = false;
	// 이미 멀티캐스트로 도착했으므로 추가 RPC 없이 공통 재생 처리만 실행합니다.
	PlayGrabGunMontage(EGunHoldReason::Win);
}

void ALobbyPCCharacter::Multicast_CompletePCMainRevolverGrab_Implementation()
{
	if (GunHoldReason != EGunHoldReason::Win || !ActiveRevolver) return;
	AttachRevolverToSocket();
	bMainRevolverGrabbed = true;
	bShowMainShotAimLine = true;
    if (IsLocallyControlled())
        ApplySeatedCamera(GetActorRotation(), GetActorRotation());
    UpdateMainAimPresentation();
}

void ALobbyPCCharacter::Multicast_ClearPCMainRevolver_Implementation()
{
	// 제한시간 만료 시에도 남은 잡기 애니메이션과 원격 화면의 총을 정리합니다.
	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		if (WinAimMontage) AnimInstance->Montage_Stop(0.0f, WinAimMontage);
	}
	ClearHeldRevolverMeshes();
	bMainRevolverGrabbed = false;
	bShowMainShotAimLine = false;
	GunHoldReason = EGunHoldReason::None;
}

void ALobbyPCCharacter::OnInteract(const FInputActionValue& Value)
{
	// 카메라 위치에서 시선 방향으로 레이캐스트 쏘기
	FVector StartLoc = CameraComponent->GetComponentLocation();
	FVector EndLoc = StartLoc + (CameraComponent->GetForwardVector() * InteractRange);

	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this); // 자기 자신은 무시

	if (GetWorld()->LineTraceSingleByChannel(HitResult, StartLoc, EndLoc, ECC_Visibility, QueryParams))
	{
		AActor* HitActor = HitResult.GetActor();

		// 맞은 액터가 IInteractableInterface를 상속받았는지 확인!
		if (HitActor && HitActor->Implements<UInteractableInterface>())
		{
			// 서버에 상호작용 요청
			ServerInteract(HitActor);
		}
	}
}

void ALobbyPCCharacter::UpdateAimFromView()
{
    UpdateMainAimPresentation();

	// 내가 조종하는 캐릭터이고, 앉아있을 때만 작동
	if (bIsSitting && IsLocallyControlled())
	{

		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			// (현재 마우스 좌우 방향) - (의자에 안착한 캡슐의 고정된 방향) = 순수하게 목이 돌아간 각도
			const FRotator Aim = UKismetMathLibrary::NormalizedDeltaRotator(GetActorRotation(), PC->GetControlRotation());

			// 내 화면을 위해 로컬 변수 즉시 업데이트
			ReplicatedAim = Aim;

			// 남들도 내 고개 돌아가는 걸 볼 수 있게 서버로 전송
			Server_UpdateAim(ReplicatedAim);
		}
	}
    // The PC ABP reads this scalar; derive it on every proxy from the replicated rotator.
    ReplicatedAimYaw = -ReplicatedAim.Yaw;
    ReplicatedAimPitch = -ReplicatedAim.Pitch;
}

void ALobbyPCCharacter::UpdateMainAimPresentation()
{
    const bool bSharePose = GunHoldReason == EGunHoldReason::Win;
    if (bSharePose == bUsingSharedAimPose || !FirstPersonMetaHumanBody
        || !ThirdPersonMetaHumanBody || !CameraComponent) return;
    if (bSharePose)
    {
        if (!ThirdPersonMetaHumanBody->GetAnimClass()) return;
        bSavedFirstPersonFOV = CameraComponent->bEnableFirstPersonFieldOfView;
        bSavedFirstPersonScale = CameraComponent->bEnableFirstPersonScale;
        // Both visible bodies now copy the same master pose, without the FP-only rig.
        FirstPersonMetaHumanBody->SetAnimInstanceClass(ThirdPersonMetaHumanBody->GetAnimClass());
        CameraComponent->bEnableFirstPersonFieldOfView = false;
        CameraComponent->bEnableFirstPersonScale = false;
    }
    else
    {
        FirstPersonMetaHumanBody->SetAnimInstanceClass(DefaultFirstPersonAnimClass);
        CameraComponent->bEnableFirstPersonFieldOfView = bSavedFirstPersonFOV;
        CameraComponent->bEnableFirstPersonScale = bSavedFirstPersonScale;
    }
    bUsingSharedAimPose = bSharePose;
}

void ALobbyPCCharacter::OnRep_IsSitting()
{
    Super::OnRep_IsSitting();
	// 캡슐(몸통) 전체가 마우스를 따라 도는 것을 막습니다.
	bUseControllerRotationYaw = !bIsSitting;

	if (IsLocallyControlled())
	{
		if (bIsSitting && bIsSittingEnded)
		{
			// 즉시 착석은 좌석에 맞춘 몸체 방향을 초기 시선으로 사용합니다.
			ApplySeatedCamera(GetActorRotation(), GetActorRotation());
		}
		else if (bIsSitting)
		{
			// 앉는 애니메이션이 재생되는 동안 카메라는 마우스를 무시하고 머리 뼈(head)를 따라가며 돌아앉는 연출을 보여줍니다.
			CameraComponent->SetRelativeLocationAndRotation(FVector(-2.8f, 8.5, 0.0f), FRotator(0.0f, 90.0f, -90.0f));
			CameraComponent->bUsePawnControlRotation = false;
			APlayerController* PC = Cast<APlayerController>(GetController());
			PC->SetControlRotation(GetActorRotation()); // 카메라가 현재 몸통이 바라보는 방향으로 즉시 회전하도록 강제
		}
		else
		{
			// 일어섰을 때 초기화 (제한 완벽 해제)
			CameraComponent->bUsePawnControlRotation = true;

			if (APlayerController* PC = Cast<APlayerController>(GetController()))
			{
				if (APlayerCameraManager* CamManager = PC->PlayerCameraManager)
				{
					CamManager->ViewYawMin = 0.0f;
					CamManager->ViewYawMax = 359.999f;
					CamManager->ViewPitchMin = -70.0f;
					CamManager->ViewPitchMax = 80.0f;
				}
			}
		}
	}
}

void ALobbyPCCharacter::ApplySeatedCamera(const FRotator& InitialViewRotation, const FRotator& FinalSitRotation)
{
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		// 애니메이션이 끝난 바로 그 순간의 '실제 카메라가 바라보는 앞방향(Forward Vector)'을 추출하여 회전값으로 변환합니다.
		// GetComponentRotation()을 그대로 쓰면 카메라에 적용된 상대 회전값(Roll -90, Yaw 90) 때문에 ControlRotation 적용 시 축이 90도 꼬여버립니다.
		//FRotator CurrentCameraRot = CameraComponent->GetForwardVector().Rotation();

		// 마우스 컨트롤(ControlRotation)을 현재 카메라가 보고 있는 방향으로 완벽하게 덮어씌웁니다.
		// 이렇게 하면 애니메이션에서 마우스로 조작 권한이 넘어갈 때 화면이 단 1픽셀도 튀지 않습니다!
		// CurrentCameraRot에서 초기 시선으로 적용
		PC->SetControlRotation(InitialViewRotation);

		// 다시 마우스로 카메라를 움직일 수 있도록 활성화
		CameraComponent->bUsePawnControlRotation = true;

		// 시야각 제한 (최종 안착 방향 기준 좌/우 60도)
		if (APlayerCameraManager* CamManager = PC->PlayerCameraManager)
		{
			float CenterYaw = FinalSitRotation.Yaw;

			// 언리얼 카메라 매니저 버그 방지 (0~360 사이 값으로 정규화)
			float MinYaw = FMath::Fmod(CenterYaw - 60.0f + 360.0f, 360.0f);
			float MaxYaw = FMath::Fmod(CenterYaw + 60.0f + 360.0f, 360.0f);

			CamManager->ViewYawMin = MinYaw;
			CamManager->ViewYawMax = MaxYaw;
			CamManager->ViewPitchMin = -45.0f;
			CamManager->ViewPitchMax = 45.0f;
		}
	}
}

void ALobbyPCCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(PlayerInputComponent))
    {
        if (IA_Interact)
        {
            EnhancedInputComponent->BindAction(IA_Interact, ETriggerEvent::Triggered, this, &ALobbyPCCharacter::OnInteract);
        }
    }
}

void ALobbyPCCharacter::DrawMainShotAimLine()
{
    // 캐릭터 Tick에서 갱신하므로 HUD나 조준점 자체가 숨겨져도 다시 표시할 수 있습니다.
    const bool bVisible = ShouldShowMainShotCrosshair();
    if (bVisible && !PCCrosshairWidget && PCCrosshairWidgetClass)
    {
        APlayerController* PC = Cast<APlayerController>(GetController());
        PCCrosshairWidget = CreateWidget<UPCCrosshairWidget>(PC, PCCrosshairWidgetClass);
        if (PCCrosshairWidget) PCCrosshairWidget->AddToPlayerScreen(10);
    }
    if (PCCrosshairWidget) PCCrosshairWidget->SetCrosshairVisible(bVisible);
}

void ALobbyPCCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (PCCrosshairWidget)
    {
        PCCrosshairWidget->RemoveFromParent();
        PCCrosshairWidget = nullptr;
    }
    Super::EndPlay(EndPlayReason);
}

bool ALobbyPCCharacter::ShouldShowMainShotCrosshair() const
{
    const APlayerController* PC = Cast<APlayerController>(GetController());
    return PC
        && PC->IsLocalController()
        && bShowMainShotAimLine
        && GunHoldReason == EGunHoldReason::Win
        && bMainRevolverGrabbed
        && ActiveRevolver != nullptr
        && !bIsPuttingBackGun;
}


// PC 착석 몽타주가 끝나면 현재 시선을 유지하면서 카메라 제한을 적용합니다.
void ALobbyPCCharacter::OnSeatedCameraReady(const FRotator& SeatRotation)
{
    if (!CameraComponent) return;
    ApplySeatedCamera(CameraComponent->GetForwardVector().Rotation(), SeatRotation);
}
