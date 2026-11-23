#include "Widget/GameTimerWidget.h"
#include "Components/TextBlock.h"
#include "Game/MainGameState.h"
#include "Engine/World.h"

void UGameTimerWidget::NativeConstruct()
{
    Super::NativeConstruct();
    LastDisplayedSeconds = INDEX_NONE;
    RefreshTimer();
    if (UWorld* World = GetWorld())
        World->GetTimerManager().SetTimer(RefreshHandle, this, &UGameTimerWidget::RefreshTimer, 0.1f, true);
}

void UGameTimerWidget::NativeDestruct()
{
    if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(RefreshHandle);
    Super::NativeDestruct();
}

void UGameTimerWidget::RefreshTimer()
{
    const AMainGameState* GS = GetWorld() ? GetWorld()->GetGameState<AMainGameState>() : nullptr;
    const bool bActive = GS && GS->bTimerActive;
    const ESlateVisibility Desired = bActive ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
    if (GetVisibility() != Desired) SetVisibility(Desired);
    const int32 Seconds = bActive ? GS->GetRemainingTimeCeil() : INDEX_NONE;
    if (Time && LastDisplayedSeconds != Seconds)
    {
        Time->SetText(Seconds == INDEX_NONE ? FText::GetEmpty() : FText::AsNumber(Seconds));
        LastDisplayedSeconds = Seconds;
    }
}
