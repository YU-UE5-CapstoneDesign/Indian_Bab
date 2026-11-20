#include "Widget/MainGameWidget.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "PlayerController/MainGamePlayerController.h"
#include "PlayerState/MainPlayerState.h"
#include "Rendering/DrawElements.h"

namespace
{
constexpr EVRBettingSelection SelectionOrder[] = {
    EVRBettingSelection::Raise, EVRBettingSelection::CheckCall,
    EVRBettingSelection::Fold, EVRBettingSelection::AddToken};
}

void UMainGameWidget::SetVRBettingNavigationEnabled(bool bEnabled)
{
    if (bVRBettingNavigationEnabled == bEnabled) return;
    bVRBettingNavigationEnabled = bEnabled;
    bVRWasOwningPlayerTurn = false;
    PendingVRTokenSelection.Reset();
    if (!bEnabled) SetVRBettingSelection(EVRBettingSelection::None);
    else RefreshVRBettingSelection();
}

bool UMainGameWidget::CanSelectVRBettingAction(EVRBettingSelection Selection) const
{
    const auto Usable = [](const UWidget* Widget)
    { return Widget && Widget->IsVisible() && Widget->GetIsEnabled(); };
    switch (Selection)
    {
    case EVRBettingSelection::Fold: return CanUseBettingButtons() && Usable(Button_Fold);
    case EVRBettingSelection::CheckCall: return CanUseBettingButtons() && Usable(Button_CheckCall);
    case EVRBettingSelection::Raise: return CanUseBettingButtons() && GetMaxRaiseCount() > 0 && Usable(Button_Raise);
    case EVRBettingSelection::AddToken: return CanUseAddToken() && Usable(CheckBox_133);
    default: return false;
    }
}

void UMainGameWidget::SetVRBettingSelection(EVRBettingSelection Selection)
{
    if (VRBettingSelection == Selection) return;
    VRBettingSelection = Selection;
    OnVRBettingSelectionChanged(Selection);
    InvalidateLayoutAndVolatility();
}

void UMainGameWidget::RefreshVRBettingSelection()
{
    if (!bVRBettingNavigationEnabled) return;
    if (PendingVRTokenSelection.IsSet() && (!CanUseAddToken()
        || (MainPS && MainPS->IsAddTokenSelected() == PendingVRTokenSelection.GetValue())
        || FPlatformTime::Seconds() - VRTokenRequestTime > 2.0))
        PendingVRTokenSelection.Reset();
    const bool bOwnTurn = IsOwningPlayerTurn();
    const bool bNewTurn = bOwnTurn && !bVRWasOwningPlayerTurn;
    bVRWasOwningPlayerTurn = bOwnTurn;
    if (bNewTurn && CanSelectVRBettingAction(EVRBettingSelection::CheckCall))
        SetVRBettingSelection(EVRBettingSelection::CheckCall);
    if (CanSelectVRBettingAction(VRBettingSelection)) return;
    if (CanSelectVRBettingAction(EVRBettingSelection::CheckCall))
    {
        SetVRBettingSelection(EVRBettingSelection::CheckCall);
        return;
    }
    for (EVRBettingSelection Selection : SelectionOrder)
        if (CanSelectVRBettingAction(Selection)) { SetVRBettingSelection(Selection); return; }
    SetVRBettingSelection(EVRBettingSelection::None);
}

void UMainGameWidget::MoveVRBettingSelection(int32 Direction)
{
    if (!bVRBettingNavigationEnabled || Direction == 0) return;
    RefreshBettingButtons();
    RefreshVRBettingSelection();
    int32 Index = 0;
    for (int32 I=0; I<UE_ARRAY_COUNT(SelectionOrder); ++I)
        if (SelectionOrder[I] == VRBettingSelection) { Index = I; break; }
    for (int32 Step=0; Step<UE_ARRAY_COUNT(SelectionOrder); ++Step)
    {
        Index = (Index + (Direction > 0 ? 1 : -1) + UE_ARRAY_COUNT(SelectionOrder)) % UE_ARRAY_COUNT(SelectionOrder);
        if (CanSelectVRBettingAction(SelectionOrder[Index]))
        { SetVRBettingSelection(SelectionOrder[Index]); return; }
    }
}

void UMainGameWidget::ConfirmVRBettingSelection()
{
    if (!bVRBettingNavigationEnabled || LastVRConfirmationFrame == GFrameCounter) return;
    const EVRBettingSelection SelectionAtPress = VRBettingSelection;
    RefreshBettingButtons();
    RefreshVRBettingSelection();
    // A state change must not confirm a different action than the one displayed.
    if (SelectionAtPress != VRBettingSelection || !CanSelectVRBettingAction(VRBettingSelection)) return;
    LastVRConfirmationFrame = GFrameCounter;
    switch (VRBettingSelection)
    {
    case EVRBettingSelection::Fold: OnButtonFold(); break;
    case EVRBettingSelection::CheckCall: OnButtonCheckCall(); break;
    case EVRBettingSelection::Raise: OnButtonRaise(); break;
    case EVRBettingSelection::AddToken:
        if (PendingVRTokenSelection.IsSet() || !MainPS) return;
        PendingVRTokenSelection = !MainPS->IsAddTokenSelected();
        VRTokenRequestTime = FPlatformTime::Seconds();
        MainGamePC->RequestSetAddTokenSelected(PendingVRTokenSelection.GetValue());
        break;
    default: break;
    }
}

void UMainGameWidget::AdjustVRRaise(int32 Direction)
{
    if (!bVRBettingNavigationEnabled) return;

    RefreshBettingButtons();
    RefreshVRBettingSelection();

    if (!CanSelectVRBettingAction(EVRBettingSelection::Raise)) return;

    if (Direction > 0) PlusButtonClicked();
    else if (Direction < 0) MinusButtonClicked();
}

UWidget* UMainGameWidget::GetVRBettingSelectionWidget() const
{
    switch (VRBettingSelection)
    {
    case EVRBettingSelection::Fold: return Button_Fold;
    case EVRBettingSelection::CheckCall: return Button_CheckCall;
    case EVRBettingSelection::Raise: return Button_Raise;
    case EVRBettingSelection::AddToken: return CheckBox_133;
    default: return nullptr;
    }
}

int32 UMainGameWidget::NativePaint(const FPaintArgs& Args, const FGeometry& Geometry,
    const FSlateRect& CullingRect, FSlateWindowElementList& Elements,
    int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const
{
    const int32 TopLayer = Super::NativePaint(Args, Geometry, CullingRect, Elements, LayerId, Style, bParentEnabled);
    const UWidget* Selected = bVRBettingNavigationEnabled ? GetVRBettingSelectionWidget() : nullptr;
    if (!Selected || !CanSelectVRBettingAction(VRBettingSelection)) return TopLayer;
    const FGeometry& Target = Selected->GetCachedGeometry();
    FVector2D Size = Target.GetLocalSize();
    if (Size.IsNearlyZero()) return TopLayer;
    FVector2D Offset = FVector2D::ZeroVector;
    if (VRBettingSelection == EVRBettingSelection::AddToken)
    {
        // The checkbox's slot stretches vertically; outline its centered content.
        const FVector2D Desired = Selected->GetDesiredSize();
        const FVector2D ContentSize(FMath::Min(Size.X,Desired.X),FMath::Min(Size.Y,Desired.Y));
        if (!ContentSize.IsNearlyZero()) { Offset = (Size-ContentSize)*.5f; Size = ContentSize; }
    }
    TArray<FVector2D> Points;
    for (const FVector2D Corner : {FVector2D::ZeroVector, FVector2D(Size.X,0), Size, FVector2D(0,Size.Y), FVector2D::ZeroVector})
        Points.Add(Geometry.AbsoluteToLocal(Target.LocalToAbsolute(Offset+Corner)));
    FSlateDrawElement::MakeLines(Elements, TopLayer+1, Geometry.ToPaintGeometry(), Points,
        ESlateDrawEffect::None, VRSelectionColor, true, VRSelectionThickness);
    return TopLayer+1;
}
