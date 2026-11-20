#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VRMenuNavigationComponent.generated.h"

class UEnhancedInputComponent;
class UEnhancedInputLocalPlayerSubsystem;
class UInputAction;
class UInputMappingContext;
class UUserWidget;
class UWidgetComponent;
class SOverlay;
class SWindow;
class FSlateVirtualUserHandle;
struct FInputActionValue;

// One local input owner for VR betting, lobby and options navigation.
UCLASS()
class INDIAN_BAB_API UVRMenuNavigationComponent : public UActorComponent
{
    GENERATED_BODY()
    friend class FVRMenuNavigationTest;
    friend class FVRBettingNavigationTest;
public:
    UVRMenuNavigationComponent();
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void RefreshInput();
    void RefreshTarget();
    void SetTarget(UWidgetComponent* Component);
    void RemoveInput();
    void RestoreWindow();
    void EnsureFocus();
    void SendKey(const FKey& Key);
    void OnNavigate(const FInputActionValue& Value, int32 Axis);
    void OnConfirm(const FInputActionValue& Value, int32 Hand);
    void OnConfirmReleased(const FInputActionValue& Value, int32 Hand);
    void OnToggleMainGame(const FInputActionValue& Value);
    void OnAdjustRaise(const FInputActionValue& Value, int32 Direction);
    bool IsBettingNavigationActive() const;

    UPROPERTY(Transient) TObjectPtr<UInputMappingContext> MenuContext;
    UPROPERTY(Transient) TObjectPtr<UInputMappingContext> ToggleContext;
    UPROPERTY(Transient) TObjectPtr<UInputMappingContext> AdjustmentContext;
    UPROPERTY(Transient) TArray<TObjectPtr<UInputAction>> Actions;
    TWeakObjectPtr<UEnhancedInputComponent> BoundInput;
    TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> InputSubsystem;
    TWeakObjectPtr<UWidgetComponent> Target;
    TWeakObjectPtr<UUserWidget> RootWidget;
    TSharedPtr<FSlateVirtualUserHandle> SlateUser;
    TSharedPtr<SOverlay> FocusOverlay;
    TWeakPtr<SWindow> FocusWindow;
    TArray<uint32> BindingHandles;
    bool bNavigationActive = false;
    bool AxisLatched[4] = {};
    uint8 GripHeldMask = 0;
    uint64 LastNavigationFrame = MAX_uint64;
};
