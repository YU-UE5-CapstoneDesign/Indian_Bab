#include "Components/VRMenuNavigationComponent.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Character/LobbyVRCharacter.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/ListView.h"
#include "Components/PanelWidget.h"
#include "Components/Slider.h"
#include "Components/WidgetComponent.h"
#include "Components/WidgetInteractionComponent.h"
#include "Components/WidgetSwitcher.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "PlayerController/MainGamePlayerController.h"
#include "Widget/MainGameWidget.h"
#include "Rendering/DrawElements.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SWindow.h"

namespace
{
class SVRMenuFocusFrame : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SVRMenuFocusFrame) {} SLATE_ARGUMENT(int32, UserIndex) SLATE_END_ARGS()
    void Construct(const FArguments& Args) { UserIndex = Args._UserIndex; SetVisibility(EVisibility::HitTestInvisible); }
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D::ZeroVector; }
    virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
        FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bEnabled) const override
    {
        const TSharedPtr<SWidget> Focus = FSlateApplication::Get().GetUserFocusedWidget(UserIndex);
        if (!Focus || !Focus->IsEnabled() || !Focus->GetVisibility().IsVisible()) return Layer;
        const FGeometry& FocusGeometry = Focus->GetCachedGeometry();
        const FVector2D Start = Geometry.AbsoluteToLocal(FocusGeometry.LocalToAbsolute(FVector2D::ZeroVector));
        const FVector2D End = Geometry.AbsoluteToLocal(FocusGeometry.LocalToAbsolute(FocusGeometry.GetLocalSize()));
        if ((End-Start).IsNearlyZero()) return Layer;
        TArray<FVector2D> Points = {Start, FVector2D(End.X,Start.Y), End, FVector2D(Start.X,End.Y), Start};
        FSlateDrawElement::MakeLines(Elements,Layer+1,Geometry.ToPaintGeometry(),Points,
            ESlateDrawEffect::None,FLinearColor(1.f,.7f,.05f),true,3.f);
        return Layer+1;
    }
private:
    int32 UserIndex = 0;
};

void GatherControls(UWidget* Widget, TArray<TSharedPtr<SWidget>>& Controls, TArray<UListView*>* Lists = nullptr)
{
    if (!Widget || !Widget->IsVisible() || !Widget->GetIsEnabled()) return;
    if (UUserWidget* UserWidget = Cast<UUserWidget>(Widget))
    {
        if (UserWidget->WidgetTree) GatherControls(UserWidget->WidgetTree->RootWidget,Controls,Lists);
        return;
    }
    if (UComboBoxString* Combo = Cast<UComboBoxString>(Widget)) Combo->SetEnableGamepadNavigationMode(true);
    UListView* List = Cast<UListView>(Widget);
    if (List && Lists) Lists->Add(List);
    if (Widget->IsA<UButton>() || Widget->IsA<UCheckBox>() || Widget->IsA<UComboBoxString>()
        || Widget->IsA<USlider>() || Widget->IsA<UEditableTextBox>() || (List && !List->GetListItems().IsEmpty()))
    {
        TSharedRef<SWidget> SlateWidget = Widget->TakeWidget();
        if (SlateWidget->SupportsKeyboardFocus()) Controls.Add(SlateWidget);
    }
    if (UWidgetSwitcher* Switcher = Cast<UWidgetSwitcher>(Widget)) GatherControls(Switcher->GetActiveWidget(),Controls,Lists);
    else if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
        for (int32 I=0; I<Panel->GetChildrenCount(); ++I) GatherControls(Panel->GetChildAt(I),Controls,Lists);
}
}

UVRMenuNavigationComponent::UVRMenuNavigationComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = .05f;
}

void UVRMenuNavigationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime,TickType,ThisTickFunction);
    RefreshInput();
    RefreshTarget();
}

void UVRMenuNavigationComponent::RefreshInput()
{
    ALobbyVRCharacter* VR = Cast<ALobbyVRCharacter>(GetOwner());
    APlayerController* PC = VR && VR->IsLocallyControlled() ? Cast<APlayerController>(VR->GetController()) : nullptr;
    UEnhancedInputComponent* Input = PC ? Cast<UEnhancedInputComponent>(PC->InputComponent) : nullptr;
    if (Input && Input == BoundInput.Get()) return;
    RemoveInput();
    if (!Input || !PC->GetLocalPlayer()) return;
    auto* Subsystem = PC->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
    if (!Subsystem) return;
    if (!MenuContext)
    {
        MenuContext = NewObject<UInputMappingContext>(this,TEXT("VRMenuFocus"));
        ToggleContext = NewObject<UInputMappingContext>(this,TEXT("VRMainGameVisibility"));
        AdjustmentContext = NewObject<UInputMappingContext>(this,TEXT("VRRaiseAdjustment"));
        const FKey Keys[] = {EKeys::OculusTouch_Left_Thumbstick_X,EKeys::OculusTouch_Left_Thumbstick_Y,
            EKeys::OculusTouch_Right_Thumbstick_X,EKeys::OculusTouch_Right_Thumbstick_Y,
            EKeys::OculusTouch_Left_Grip_Click,EKeys::OculusTouch_Right_Grip_Click,EKeys::OculusTouch_Right_B_Click,
            EKeys::OculusTouch_Left_X_Click,EKeys::OculusTouch_Left_Y_Click};
        for (int32 I=0; I<UE_ARRAY_COUNT(Keys); ++I)
        {
            UInputAction* Action = NewObject<UInputAction>(this);
            Action->ValueType = I<4 ? EInputActionValueType::Axis1D : EInputActionValueType::Boolean;
            Action->bConsumeInput = true;
            Actions.Add(Action);
            (I==6 ? ToggleContext : I>6 ? AdjustmentContext : MenuContext)->MapKey(Action,Keys[I]);
        }
    }
    for (int32 Axis=0; Axis<4; ++Axis)
        for (ETriggerEvent Event : {ETriggerEvent::Triggered,ETriggerEvent::Completed,ETriggerEvent::Canceled})
            BindingHandles.Add(Input->BindAction(Actions[Axis],Event,this,&UVRMenuNavigationComponent::OnNavigate,Axis).GetHandle());
    for (int32 Hand=0; Hand<2; ++Hand)
    {
        BindingHandles.Add(Input->BindAction(Actions[Hand+4],ETriggerEvent::Started,this,&UVRMenuNavigationComponent::OnConfirm,Hand).GetHandle());
        for (ETriggerEvent Event : {ETriggerEvent::Completed,ETriggerEvent::Canceled})
            BindingHandles.Add(Input->BindAction(Actions[Hand+4],Event,this,&UVRMenuNavigationComponent::OnConfirmReleased,Hand).GetHandle());
    }
    BindingHandles.Add(Input->BindAction(Actions[6],ETriggerEvent::Started,this,&UVRMenuNavigationComponent::OnToggleMainGame).GetHandle());
    BindingHandles.Add(Input->BindAction(Actions[7],ETriggerEvent::Started,this,&UVRMenuNavigationComponent::OnAdjustRaise,-1).GetHandle());
    BindingHandles.Add(Input->BindAction(Actions[8],ETriggerEvent::Started,this,&UVRMenuNavigationComponent::OnAdjustRaise,1).GetHandle());
    BoundInput = Input;
    InputSubsystem = Subsystem;
    FModifyContextOptions Options;
    Options.bIgnoreAllPressedKeysUntilRelease = true;
    Subsystem->AddMappingContext(ToggleContext,150,Options);
    if (FSlateApplication::IsInitialized())
        SlateUser = FSlateApplication::Get().FindOrCreateVirtualUser(VR->WidgetInteractionRight->VirtualUserIndex);
}

void UVRMenuNavigationComponent::RefreshTarget()
{
    UWidgetComponent* Found = nullptr;
    int32 BestPriority = 0;
    if (BoundInput.IsValid())
    {
        const APawn* Pawn = Cast<APawn>(GetOwner());
        const auto* GamePC = Pawn ? Cast<AMainGamePlayerController>(Pawn->GetController()) : nullptr;
        const bool bBettingAllowed = GamePC && GamePC->CanUseVRBettingNavigation();
        TInlineComponentArray<UWidgetComponent*> Components(GetOwner());
        for (UWidgetComponent* Component : Components)
        {
            if (!Component->IsVisible() || Component->bHiddenInGame || !Component->GetUserWidgetObject()
                || !Component->GetUserWidgetObject()->IsVisible()) continue;
            const FString Name = Component->GetName();
            const auto* Betting = Cast<UMainGameWidget>(Component->GetUserWidgetObject());
            const int32 Priority = Name.Contains(TEXT("VRMenuWidget")) ? 4 : Name.Contains(TEXT("ResultWidget")) ? 3
                : Name.Contains(TEXT("ReadyWidget")) ? 2 : Name.Contains(TEXT("MainMenuWidget")) ? 1
                : Betting && bBettingAllowed && Betting->GetOwningPlayer()==GamePC ? 1 : 0;
            if (Priority>BestPriority) { Found = Component; BestPriority = Priority; }
        }
    }
    SetTarget(Found);
    EnsureFocus();
}

void UVRMenuNavigationComponent::SetTarget(UWidgetComponent* Component)
{
    UUserWidget* Widget = Component ? Component->GetUserWidgetObject() : nullptr;
    if (Target.Get()==Component && RootWidget.Get()==Widget) return;
    const bool bKeepHeldInput = bNavigationActive && Component != nullptr;
    if (auto* Betting = Cast<UMainGameWidget>(RootWidget.Get())) Betting->SetVRBettingNavigationEnabled(false);
    RestoreWindow();
    if (SlateUser && FSlateApplication::IsInitialized()) FSlateApplication::Get().ClearUserFocus(SlateUser->GetUserIndex());
    Target = Component;
    RootWidget = Widget;
    if (!bKeepHeldInput)
    {
        GripHeldMask = 0;
        for (bool& Latched : AxisLatched) Latched = false;
    }
    if (InputSubsystem.IsValid())
    {
        FModifyContextOptions Options;
        Options.bIgnoreAllPressedKeysUntilRelease = true;
        if (Component && !bNavigationActive) InputSubsystem->AddMappingContext(MenuContext,200,Options);
        else if (!Component && bNavigationActive) InputSubsystem->RemoveMappingContext(MenuContext,Options);
        if (Cast<UMainGameWidget>(Widget)) InputSubsystem->AddMappingContext(AdjustmentContext,200,Options);
        else InputSubsystem->RemoveMappingContext(AdjustmentContext,Options);
    }
    bNavigationActive = Component != nullptr;
    if (auto* Betting = Cast<UMainGameWidget>(Widget)) Betting->SetVRBettingNavigationEnabled(bNavigationActive);
}

void UVRMenuNavigationComponent::EnsureFocus()
{
    if (!Target.IsValid() || !RootWidget.IsValid() || !SlateUser || !FSlateApplication::IsInitialized()) return;
    if (IsBettingNavigationActive()) return;
    const TSharedPtr<SWindow> Window = Target->GetVirtualWindow();
    if (!Window) return;
    if (!FocusOverlay)
    {
        FocusOverlay = SNew(SOverlay)
            +SOverlay::Slot()[RootWidget->TakeWidget()]
            +SOverlay::Slot()[SNew(SVRMenuFocusFrame).UserIndex(SlateUser->GetUserIndex())];
        Window->SetContent(FocusOverlay.ToSharedRef());
        FocusWindow = Window;
    }
    TArray<TSharedPtr<SWidget>> Controls;
    GatherControls(RootWidget.Get(),Controls);
    const auto Focus = FSlateApplication::Get().GetUserFocusedWidget(SlateUser->GetUserIndex());
    if (Controls.Contains(Focus) || (Focus && FSlateApplication::Get().AnyMenusVisible())) return;
    const auto User = FSlateApplication::Get().GetUser(SlateUser->GetUserIndex());
    for (const auto& Control : Controls) if (User && User->IsWidgetInFocusPath(Control)) return;
    if (!Controls.IsEmpty()) FSlateApplication::Get().SetUserFocus(SlateUser->GetUserIndex(),Controls[0],EFocusCause::Navigation);
    else FSlateApplication::Get().ClearUserFocus(SlateUser->GetUserIndex());
}

void UVRMenuNavigationComponent::SendKey(const FKey& Key)
{
    if (!SlateUser || !Target.IsValid()) return;
    const int32 UserIndex = SlateUser->GetUserIndex();
    FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(Key,FModifierKeysState(),UserIndex,false,0,0));
    FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(Key,FModifierKeysState(),UserIndex,false,0,0));
}

void UVRMenuNavigationComponent::OnNavigate(const FInputActionValue& Value, int32 Axis)
{
    if (Axis<0 || Axis>3) return;
    const float Amount = Value.Get<float>();
    if (FMath::Abs(Amount)<=.25f) { AxisLatched[Axis] = false; return; }
    RefreshTarget();
    if (!bNavigationActive || AxisLatched[Axis] || FMath::Abs(Amount)<.65f) return;
    if (IsBettingNavigationActive() && Axis%2==1) return;
    AxisLatched[Axis] = true;
    if (LastNavigationFrame==GFrameCounter) return;
    LastNavigationFrame = GFrameCounter;
    if (auto* Betting = Cast<UMainGameWidget>(RootWidget.Get()))
    {
        Betting->MoveVRBettingSelection(Amount>0 ? 1 : -1);
        return;
    }
    const FKey Key = Axis%2==0 ? (Amount>0 ? EKeys::Gamepad_DPad_Right : EKeys::Gamepad_DPad_Left)
        : (Amount>0 ? EKeys::Gamepad_DPad_Up : EKeys::Gamepad_DPad_Down);
    SendKey(Key);
}

void UVRMenuNavigationComponent::OnConfirm(const FInputActionValue& Value, int32 Hand)
{
    if (Hand<0 || Hand>1) return;
    RefreshTarget();
    if (!bNavigationActive) return;
    const bool bFirstPress = GripHeldMask==0;
    GripHeldMask |= uint8(1<<Hand);
    if (!bFirstPress) return;
    if (auto* Betting = Cast<UMainGameWidget>(RootWidget.Get()))
    {
        Betting->ConfirmVRBettingSelection();
        return;
    }
    // List navigation changes the highlighted row; activation must still use its
    // normal mouse-click route (including native and Blueprint item-click events).
    TArray<TSharedPtr<SWidget>> Controls;
    TArray<UListView*> Lists;
    GatherControls(RootWidget.Get(),Controls,&Lists);
    const auto User = FSlateApplication::Get().GetUser(SlateUser->GetUserIndex());
    for (UListView* List : Lists)
    {
        if (!User || !User->IsWidgetInFocusPath(List->GetCachedWidget())) continue;
        UObject* Item = List->GetSelectedItem();
        if (!Item && !List->GetListItems().IsEmpty()) { Item = List->GetItemAt(0); List->SetSelectedItem(Item); }
        UUserWidget* Row = List->GetEntryWidgetFromItem(Item);
        if (!Row || !Row->GetIsEnabled()) return;
        const auto Window = Target->GetVirtualWindow();
        if (!Window) return;
        const FGeometry& Geometry = Row->GetCachedGeometry();
        const FVector2D Position = Window->GetCachedGeometry().AbsoluteToLocal(Geometry.LocalToAbsolute(Geometry.GetLocalSize()*.5f));
        FWidgetPath Path(Target->GetHitWidgetPath(Position,false));
        if (!Path.IsValid()) return;
        const TSet<FKey> Pressed = {EKeys::LeftMouseButton};
        const TSet<FKey> Released;
        const int32 UserIndex = SlateUser->GetUserIndex();
        FSlateApplication::Get().RoutePointerDownEvent(Path,FPointerEvent(UserIndex,2,Position,Position,Pressed,EKeys::LeftMouseButton,0,FModifierKeysState()));
        FSlateApplication::Get().RoutePointerUpEvent(Path,FPointerEvent(UserIndex,2,Position,Position,Released,EKeys::LeftMouseButton,0,FModifierKeysState()));
        return;
    }
    SendKey(EKeys::Virtual_Accept);
}

void UVRMenuNavigationComponent::OnConfirmReleased(const FInputActionValue& Value, int32 Hand)
{
    if (Hand>=0 && Hand<2) GripHeldMask &= ~uint8(1<<Hand);
}

void UVRMenuNavigationComponent::OnToggleMainGame(const FInputActionValue& Value)
{
    if (ALobbyVRCharacter* VR = Cast<ALobbyVRCharacter>(GetOwner())) VR->ToggleMainGameWidget();
}

bool UVRMenuNavigationComponent::IsBettingNavigationActive() const
{
    return bNavigationActive && Cast<UMainGameWidget>(RootWidget.Get()) != nullptr;
}

void UVRMenuNavigationComponent::OnAdjustRaise(const FInputActionValue& Value, int32 Direction)
{
    RefreshTarget();
    if (IsBettingNavigationActive()) CastChecked<UMainGameWidget>(RootWidget.Get())->AdjustVRRaise(Direction);
}

void UVRMenuNavigationComponent::RestoreWindow()
{
    if (const TSharedPtr<SWindow> Window = FocusWindow.Pin())
        if (RootWidget.IsValid() && Window->GetContent()==FocusOverlay) Window->SetContent(RootWidget->TakeWidget());
    FocusOverlay.Reset();
    FocusWindow.Reset();
}

void UVRMenuNavigationComponent::RemoveInput()
{
    SetTarget(nullptr);
    if (InputSubsystem.IsValid())
    {
        if (MenuContext) InputSubsystem->RemoveMappingContext(MenuContext);
        if (ToggleContext) InputSubsystem->RemoveMappingContext(ToggleContext);
        if (AdjustmentContext) InputSubsystem->RemoveMappingContext(AdjustmentContext);
    }
    if (BoundInput.IsValid()) for (uint32 Handle : BindingHandles) BoundInput->RemoveBindingByHandle(Handle);
    BindingHandles.Reset();
    BoundInput.Reset();
    InputSubsystem.Reset();
    SlateUser.Reset();
}

void UVRMenuNavigationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    RemoveInput();
    Super::EndPlay(EndPlayReason);
}
