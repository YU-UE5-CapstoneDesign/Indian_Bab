#include "Widget/EliminationNoticeWidget.h"
#include "Components/WidgetSwitcher.h"
#include "Components/Button.h"
#include "PlayerController/MainGamePlayerController.h"

void UEliminationNoticeWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if (Button_Spectate)
    {
        Button_Spectate->OnClicked.RemoveDynamic(this, &UEliminationNoticeWidget::OnSpectateClicked);
        Button_Spectate->OnClicked.AddDynamic(this, &UEliminationNoticeWidget::OnSpectateClicked);
    }
}

void UEliminationNoticeWidget::NativeDestruct()
{
    if (Button_Spectate) Button_Spectate->OnClicked.RemoveDynamic(this, &UEliminationNoticeWidget::OnSpectateClicked);
    Super::NativeDestruct();
}

void UEliminationNoticeWidget::OnSpectateClicked()
{
    if (AMainGamePlayerController* PC = Cast<AMainGamePlayerController>(GetOwningPlayer()))
        PC->EnterEliminationSpectatorMode();
}

bool UEliminationNoticeWidget::HandleSpectateClickAtAbsoluteLocation(const FVector2D& AbsoluteLocation)
{
    if (bShowingSpectator || !IsVisible() || !Button_Spectate
        || !Button_Spectate->GetIsEnabled() || !Button_Spectate->IsVisible()
        || !Button_Spectate->GetCachedGeometry().IsUnderLocation(AbsoluteLocation)) return false;
    OnSpectateClicked();
    return true;
}

void UEliminationNoticeWidget::NativePreConstruct()
{
    Super::NativePreConstruct();
    SetNotice(bShowingSpectator);
}

void UEliminationNoticeWidget::SetNotice(bool bSpectating)
{
    bShowingSpectator = bSpectating;
    if (NoticeStates) NoticeStates->SetActiveWidgetIndex(bSpectating ? 1 : 0);
    if (Button_Spectate) Button_Spectate->SetIsEnabled(!bSpectating);
}
