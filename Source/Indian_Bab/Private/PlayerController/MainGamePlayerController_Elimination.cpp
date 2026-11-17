#include "PlayerController/MainGamePlayerController.h"
#include "Character/LobbyVRCharacter.h"
#include "PlayerState/MainPlayerState.h"
#include "Widget/MainGameWidget.h"
#include "Components/WidgetComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"

bool AMainGamePlayerController::CanPerformGameActions() const
{
    const AMainPlayerState* PS = GetPlayerState<AMainPlayerState>();
    return PS && PS->isAlive && !bFinalResultsShown;
}

void AMainGamePlayerController::OnRep_Pawn()
{
    Super::OnRep_Pawn();
    RefreshEliminationNotice();
}

void AMainGamePlayerController::RefreshEliminationNotice()
{
    if (!IsLocalController()) return;
    const AMainPlayerState* PS = GetPlayerState<AMainPlayerState>();
    if (!PS) return;
    if (PS->isAlive || bFinalResultsShown)
    {
        ClearEliminationNotice();
        return;
    }
    if (!bEliminationNoticeStarted)
    {
        bEliminationNoticeStarted = true;
        bShowingSpectatorNotice = false;
    }
    // 관전 전환은 버튼으로만 요청하고, 중복 알림은 현재 선택을 유지합니다.
    UpdateEliminationPresentation();
}

void AMainGamePlayerController::EnterEliminationSpectatorMode()
{
    const AMainPlayerState* PS = GetPlayerState<AMainPlayerState>();
    if (!IsLocalController() || !PS || PS->isAlive || bFinalResultsShown
        || !bEliminationNoticeStarted || bShowingSpectatorNotice) return;
    bShowingSpectatorNotice = true;
    UpdateEliminationPresentation();
}

void AMainGamePlayerController::UpdateEliminationPresentation()
{
    if (!IsLocalController() || !GetLocalPlayer()) return;
    const AMainPlayerState* PS = GetPlayerState<AMainPlayerState>();
    if (!PS || PS->isAlive || bFinalResultsShown) return;
    ALobbyVRCharacter* VR = Cast<ALobbyVRCharacter>(GetPawn());
    if (bUseVRPlayMode && !VR) return;
    UMainGameWidget* GameWidget = MainGameWidgetInstance;
    if (VR)
    {
        GameWidget = nullptr;
        TArray<UWidgetComponent*> Components;
        VR->GetComponents(Components);
        for (UWidgetComponent* Component : Components)
        {
            if (UMainGameWidget* Candidate = Cast<UMainGameWidget>(Component->GetUserWidgetObject()))
            {
                GameWidget = Candidate;
                break;
            }
        }
    }
    if (!GameWidget) return;
    if (IsValid(EliminationGameWidget) && EliminationGameWidget != GameWidget)
        EliminationGameWidget->SetEliminationNotice(false, false);
    EliminationGameWidget = GameWidget;
    GameWidget->SetEliminationNotice(true, bShowingSpectatorNotice);
}

void AMainGamePlayerController::ClearEliminationNotice()
{
    if (IsValid(EliminationGameWidget)) EliminationGameWidget->SetEliminationNotice(false, false);
    EliminationGameWidget = nullptr;
    bEliminationNoticeStarted = false;
    bShowingSpectatorNotice = false;
}

void AMainGamePlayerController::DismissEliminationNoticeForResults()
{
    if (!IsLocalController()) return;
    bFinalResultsShown = true;
    ClearEliminationNotice();
}
