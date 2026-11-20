#include "PlayerController/MainGamePlayerController.h"
#include "Character/LobbyVRCharacter.h"
#include "Components/WidgetComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Game/MainGameState.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "PlayerState/MainPlayerState.h"
#include "Widget/MainGameWidget.h"
#include "Widget/OptionMenuWidget.h"

void AMainGamePlayerController::InitializeVRBettingInput()
{
    UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(InputComponent);
    if (!Input || VRBettingBoundInputComponent == Input) return;
    if (!VRBettingMappingContext)
    {
        VRBettingMappingContext = NewObject<UInputMappingContext>(this,TEXT("VRBettingNavigation"));
        const FKey Keys[] = {EKeys::OculusTouch_Left_Thumbstick_X, EKeys::OculusTouch_Right_Thumbstick_X,
            EKeys::OculusTouch_Left_Grip_Click, EKeys::OculusTouch_Right_Grip_Click,
            EKeys::OculusTouch_Left_X_Click, EKeys::OculusTouch_Left_Y_Click};
        for (int32 I=0; I<UE_ARRAY_COUNT(Keys); ++I)
        {
            UInputAction* Action = NewObject<UInputAction>(this);
            Action->ValueType = I<2 ? EInputActionValueType::Axis1D : EInputActionValueType::Boolean;
            Action->bConsumeInput = true;
            VRBettingInputActions.Add(Action);
            VRBettingMappingContext->MapKey(Action, Keys[I]);
        }
    }
    for (int32 Hand=0; Hand<2; ++Hand)
    {
        for (ETriggerEvent Event : {ETriggerEvent::Triggered,ETriggerEvent::Completed,ETriggerEvent::Canceled})
            Input->BindAction(VRBettingInputActions[Hand],Event,this,&AMainGamePlayerController::OnVRBettingNavigate,Hand);
        Input->BindAction(VRBettingInputActions[Hand+2],ETriggerEvent::Started,this,&AMainGamePlayerController::OnVRBettingConfirm,Hand);
        for (ETriggerEvent Event : {ETriggerEvent::Completed,ETriggerEvent::Canceled})
            Input->BindAction(VRBettingInputActions[Hand+2],Event,this,&AMainGamePlayerController::OnVRBettingConfirmReleased,Hand);
    }
    Input->BindAction(VRBettingInputActions[4],ETriggerEvent::Started,this,&AMainGamePlayerController::OnVRBettingAdjust,-1);
    Input->BindAction(VRBettingInputActions[5],ETriggerEvent::Started,this,&AMainGamePlayerController::OnVRBettingAdjust,1);
    VRBettingBoundInputComponent = Input;
}

UMainGameWidget* AMainGamePlayerController::FindVRBettingWidget()
{
    if (VRBettingWidgetComponent.IsValid() && VRBettingWidget.IsValid()
        && VRBettingWidgetComponent->GetOwner() == GetPawn()
        && VRBettingWidgetComponent->GetUserWidgetObject() == VRBettingWidget.Get()
        && VRBettingWidgetComponent->IsVisible() && !VRBettingWidgetComponent->bHiddenInGame
        && VRBettingWidget->IsVisible()) return VRBettingWidget.Get();
    TInlineComponentArray<UWidgetComponent*> Components(GetPawn());
    for (UWidgetComponent* Component : Components)
    {
        if (!Component->IsVisible() || Component->bHiddenInGame) continue;
        UMainGameWidget* Widget = Cast<UMainGameWidget>(Component->GetUserWidgetObject());
        if (!Widget || !Widget->IsVisible() || Widget->GetOwningPlayer() != this) continue;
        if (VRBettingWidget.IsValid() && VRBettingWidget.Get() != Widget)
            VRBettingWidget->SetVRBettingNavigationEnabled(false);
        VRBettingWidget = Widget;
        VRBettingWidgetComponent = Component;
        return Widget;
    }
    return nullptr;
}

void AMainGamePlayerController::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);
    if (IsLocalController()) RefreshVRBettingInput();
}

void AMainGamePlayerController::RefreshVRBettingInput()
{
    const AMainGameState* GS = GetWorld() ? GetWorld()->GetGameState<AMainGameState>() : nullptr;
    const AMainPlayerState* PS = GetPlayerState<AMainPlayerState>();
    const ALobbyVRCharacter* VR = Cast<ALobbyVRCharacter>(GetPawn());
    bool bEligible = IsLocalController() && VR && CanPerformGameActions() && PS && !PS->isFold
        && GS && GS->CurrentGamePhase == EGamePhase::Playing && !VR->IsDeathPresentationActive()
        && !bExitRequested && !ExitConfirmWidgetInstance && !CardRevealStageInstance
        && !(OptionMenuInstance && OptionMenuInstance->IsInViewport() && OptionMenuInstance->IsVisible())
        && !(OfflineWidgetInstance && OfflineWidgetInstance->IsInViewport() && OfflineWidgetInstance->IsVisible());
    if (bEligible)
    {
        const UWidgetComponent* Menu = FindVRMenuComponent();
        bEligible = !(Menu && Menu->IsVisible() && !Menu->bHiddenInGame);
    }
    if (bEligible) bEligible = FindVRBettingWidget() != nullptr;
    if (bEligible) InitializeVRBettingInput();
    SetVRBettingInputActive(bEligible && VRBettingMappingContext != nullptr);
}

void AMainGamePlayerController::SetVRBettingInputActive(bool bActive)
{
    if (VRBettingWidget.IsValid()) VRBettingWidget->SetVRBettingNavigationEnabled(bActive);
    if (bVRBettingInputActive == bActive) return;
    if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
        if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
        {
            FModifyContextOptions Options;
            Options.bIgnoreAllPressedKeysUntilRelease = true;
            if (bActive) Subsystem->AddMappingContext(VRBettingMappingContext,100,Options);
            else if (VRBettingMappingContext) Subsystem->RemoveMappingContext(VRBettingMappingContext,Options);
        }
    bVRBettingInputActive = bActive;
    bVRStickLatched[0] = bVRStickLatched[1] = false;
    VRConfirmHeldMask = 0;
}

void AMainGamePlayerController::OnVRBettingNavigate(const FInputActionValue& Value, int32 Hand)
{
    if (Hand<0 || Hand>1) return;
    const float Axis = Value.Get<float>();
    if (FMath::Abs(Axis)<=.25f) { bVRStickLatched[Hand] = false; return; }
    RefreshVRBettingInput();
    if (!bVRBettingInputActive || bVRStickLatched[Hand] || FMath::Abs(Axis)<.65f) return;
    bVRStickLatched[Hand] = true;
    if (LastVRNavigationFrame == GFrameCounter) return;
    LastVRNavigationFrame = GFrameCounter;
    if (VRBettingWidget.IsValid()) VRBettingWidget->MoveVRBettingSelection(Axis>0 ? 1 : -1);
}

void AMainGamePlayerController::OnVRBettingConfirm(const FInputActionValue& Value, int32 Hand)
{
    RefreshVRBettingInput();
    if (!bVRBettingInputActive || Hand<0 || Hand>1) return;
    const bool bFirstPress = VRConfirmHeldMask == 0;
    VRConfirmHeldMask |= uint8(1<<Hand);
    if (bFirstPress && VRBettingWidget.IsValid()) VRBettingWidget->ConfirmVRBettingSelection();
}

void AMainGamePlayerController::OnVRBettingConfirmReleased(const FInputActionValue& Value, int32 Hand)
{
    if (Hand>=0 && Hand<2) VRConfirmHeldMask &= ~uint8(1<<Hand);
}

void AMainGamePlayerController::OnVRBettingAdjust(const FInputActionValue& Value, int32 Direction)
{
    RefreshVRBettingInput();
    if (bVRBettingInputActive && VRBettingWidget.IsValid()) VRBettingWidget->AdjustVRRaise(Direction);
}
