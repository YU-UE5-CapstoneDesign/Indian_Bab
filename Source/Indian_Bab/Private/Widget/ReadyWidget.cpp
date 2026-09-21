#include "Widget/ReadyWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Game/MainGameState.h"
#include "PlayerController/MainGamePlayerController.h"

void UReadyWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    if (Button_Ready)
    {
        Button_Ready->OnClicked.RemoveAll(this);
        Button_Ready->OnClicked.AddDynamic(this, &UReadyWidget::OnReadyButtonClicked);
    }
    RefreshReadyState();
}

void UReadyWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    RefreshReadyState();
}

void UReadyWidget::NativeDestruct()
{
    if (Button_Ready) Button_Ready->OnClicked.RemoveAll(this);
    Super::NativeDestruct();
}

void UReadyWidget::RefreshReadyState()
{
    const AMainGameState* GS = GetWorld() ? GetWorld()->GetGameState<AMainGameState>() : nullptr;
    const APlayerController* PC = GetOwningPlayer();
    const APlayerState* PS = PC ? PC->PlayerState : nullptr;
    const bool bLobby = GS && GS->CurrentGamePhase == EGamePhase::Lobby;
    const bool bHost = GS && PS && GS->LobbyReadyStatus.HostPlayerId == PS->GetPlayerId();
    const bool bReady = GS && PS && GS->LobbyReadyStatus.ReadyPlayerIds.Contains(PS->GetPlayerId());

    if (Text_ReadyPlayer)
    {
        const FText Count = FText::FromString(FString::Printf(TEXT("(%d/%d)"),
            GS ? GS->LobbyReadyStatus.ReadyPlayerIds.Num() : 0,
            GS ? GS->LobbyReadyStatus.ConnectedPlayerCount : 0));
        if (!Text_ReadyPlayer->GetText().EqualTo(Count)) Text_ReadyPlayer->SetText(Count);
    }
    if (Text_ReadyState)
    {
        const FText Label = FText::FromString(bHost ? TEXT("Start") : TEXT("Ready"));
        if (!Text_ReadyState->GetText().EqualTo(Label)) Text_ReadyState->SetText(Label);
    }
    if (Button_Ready)
    {
        const bool bEnabled = bLobby && PC && PC->IsLocalController() && PS
            && GS->LobbyReadyStatus.HostPlayerId != INDEX_NONE
            && (bHost ? GS->LobbyReadyStatus.bCanStart : !bReady);
        if (Button_Ready->GetIsEnabled() != bEnabled) Button_Ready->SetIsEnabled(bEnabled);
    }
}

void UReadyWidget::OnReadyButtonClicked()
{
    ConfirmReady();
}

void UReadyWidget::ConfirmReady()
{
    // VR direct clicks must check the same replicated readiness as desktop clicks.
    RefreshReadyState();
    if (!Button_Ready || !Button_Ready->GetIsEnabled()) return;
    AMainGamePlayerController* PC = Cast<AMainGamePlayerController>(GetOwningPlayer());
    const AMainGameState* GS = GetWorld() ? GetWorld()->GetGameState<AMainGameState>() : nullptr;
    if (!PC || !PC->IsLocalController() || !PC->PlayerState || !GS) return;

    if (GS->LobbyReadyStatus.HostPlayerId == PC->PlayerState->GetPlayerId())
        PC->Server_RequestStart();
    else
        PC->Server_RequestReady();
    // Keep the widget open to display readiness and host changes.
}
