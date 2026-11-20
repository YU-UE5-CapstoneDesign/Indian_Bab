#include "PlayerController/MainGamePlayerController.h"
#include "Character/LobbyVRCharacter.h"
#include "Components/WidgetComponent.h"
#include "Game/MainGameState.h"
#include "PlayerState/MainPlayerState.h"
#include "Widget/OptionMenuWidget.h"

bool AMainGamePlayerController::CanUseVRBettingNavigation() const
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
    return bEligible;
}
