#include "PCCrosshairWidget.h"
#include "Components/Image.h"

void UPCCrosshairWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if (UI_Crosshair)
        UI_Crosshair->SetVisibility(ESlateVisibility::HitTestInvisible);
    SetCrosshairVisible(false);
}

void UPCCrosshairWidget::SetCrosshairVisible(bool bVisible)
{
    const ESlateVisibility DesiredVisibility = bVisible && UI_Crosshair
        ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
    if (GetVisibility() != DesiredVisibility)
        SetVisibility(DesiredVisibility);
}
