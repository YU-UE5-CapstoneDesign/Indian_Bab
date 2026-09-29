#include "Widget/TurnInfoWidget.h"
#include "Components/TextBlock.h"
#include "Game/MainGameState.h"
#include "GameFramework/PlayerController.h"
#include "PlayerState/MainPlayerState.h"

void UTurnInfoWidget::InitializeForPlayer(APlayerController* InOwningPlayer)
{
	if (InOwningPlayer)
	{
		SetOwningPlayer(InOwningPlayer);
	}

	BindGameState();
	RefreshFromGameState();
}

void UTurnInfoWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BindGameState();
	RefreshFromGameState();
}

void UTurnInfoWidget::NativeDestruct()
{
	UnbindGameState();

	Super::NativeDestruct();
}

void UTurnInfoWidget::SetTurnText(const FText& InTurnText)
{
	if (!Text_Turn) return;

	Text_Turn->SetText(InTurnText);
}

void UTurnInfoWidget::SetMainRevolverCount(int32 CurrentCount, int32 MaxCount)
{
	if (!Text_MainRevolver) return;

	Text_MainRevolver->SetVisibility(ESlateVisibility::Visible);
	Text_MainRevolver->SetText(FText::FromString(FString::Printf(TEXT("MainRevolver(%d/%d)"), CurrentCount, MaxCount)));
}

void UTurnInfoWidget::BindGameState()
{
	AMainGameState* GameState = GetWorld() ? GetWorld()->GetGameState<AMainGameState>() : nullptr;
	if (BoundGameState == GameState)
	{
		return;
	}

	UnbindGameState();

	BoundGameState = GameState;
	if (BoundGameState)
	{
		BoundGameState->OnTurnInfoChanged.AddUObject(this, &UTurnInfoWidget::RefreshFromGameState);
	}
}

void UTurnInfoWidget::UnbindGameState()
{
	if (BoundGameState)
	{
		BoundGameState->OnTurnInfoChanged.RemoveAll(this);
		BoundGameState = nullptr;
	}
}

void UTurnInfoWidget::RefreshFromGameState()
{
	if (!BoundGameState)
	{
		BindGameState();
	}

	if (!BoundGameState)
	{
		SetVisibility(ESlateVisibility::Hidden);
		return;
	}

	if (BoundGameState->CurrentGamePhase == EGamePhase::Playing)
	{
		const APlayerController* OwningPlayer = GetOwningPlayer();
		const APlayerState* OwningPlayerState = OwningPlayer ? OwningPlayer->PlayerState : nullptr;
		if (!OwningPlayerState || OwningPlayerState->GetPlayerId() != BoundGameState->CurrentTurnPlayerId)
		{
			SetVisibility(ESlateVisibility::Hidden);
			return;
		}

		SetTurnText(FText::FromString(TEXT("Your Turn")));
		if (Text_MainRevolver)
		{
			Text_MainRevolver->SetVisibility(ESlateVisibility::Collapsed);
		}
		SetVisibility(ESlateVisibility::Visible);
		return;
	}

	const bool bIsMainShotPhase = BoundGameState->CurrentGamePhase == EGamePhase::Result
		&& BoundGameState->MainShotPlayerId >= 0
		&& BoundGameState->MainShotTotalCount > 0;
	if (bIsMainShotPhase)
	{
		const FString PlayerName = GetPlayerDisplayNameById(BoundGameState->MainShotPlayerId);
		SetTurnText(FText::FromString(FString::Printf(TEXT("%s\uc758 Shot"), *PlayerName)));
		if (Text_MainRevolver)
		{
			Text_MainRevolver->SetVisibility(ESlateVisibility::Visible);
		}
		SetMainRevolverCount(BoundGameState->CurrentBulletCount, BoundGameState->MainShotTotalCount);
		SetVisibility(ESlateVisibility::Visible);
		return;
	}

	SetVisibility(ESlateVisibility::Hidden);
}

FString UTurnInfoWidget::GetPlayerDisplayNameById(int32 PlayerId) const
{
	if (!BoundGameState)
	{
		return TEXT("Unknown");
	}

	for (APlayerState* PlayerState : BoundGameState->PlayerArray)
	{
		if (!PlayerState || PlayerState->GetPlayerId() != PlayerId)
		{
			continue;
		}

		if (const AMainPlayerState* MainPlayerState = Cast<AMainPlayerState>(PlayerState))
		{
			const FString SteamNickname = MainPlayerState->GetSteamNickname();
			if (!SteamNickname.IsEmpty())
			{
				return SteamNickname;
			}
		}

		const FString PlayerName = PlayerState->GetPlayerName();
		return PlayerName.IsEmpty() ? FString::Printf(TEXT("Player %d"), PlayerId) : PlayerName;
	}

	return FString::Printf(TEXT("Player %d"), PlayerId);
}
