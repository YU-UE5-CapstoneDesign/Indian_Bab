

#include "Game/MainGameMode.h"
#include "Game/MainGameState.h"
#include "Kismet/GameplayStatics.h"
#include "Character/LobbyCharacter.h"
#include "Character/LobbyPCCharacter.h"
#include "Character/LobbyVRCharacter.h"
#include "Actor/SeatActor.h"
#include "Actor/Revolver.h"
#include "CardController/CardManager.h"
#include "PlayerState/MainPlayerState.h"
#include "PlayerController/MainGamePlayerController.h"
#include "Game/CardRevealTypes.h"
#include "GameFramework/Character.h"

#if WITH_SERVER_CODE

// 카드 매니저 획득
TObjectPtr<ACardManager> AMainGameMode::GetCardManager()
{
    if (!IsValid(MainCardManager))
    {
        MainCardManager = Cast<ACardManager>(
            UGameplayStatics::GetActorOfClass(GetWorld(), ACardManager::StaticClass())
        );
    }
    return MainCardManager;
}

// 카드 분배
bool AMainGameMode::TryDistributeCards(const TArray<FMainRoundParticipant>& Participants)
{
    MainCardManager = GetCardManager();
    if (!IsValid(MainCardManager) || !IsValid(MainCardManager->CardDataTable) || Participants.IsEmpty()) return false;
    for (const FMainRoundParticipant& Participant : Participants)
    {
        if (!IsValid(Participant.PlayerState) || !Participant.PlayerState->isAlive) return false;
    }
    const int32 CardCount = Participants.Num();
    if (MainCardManager->CurrentDeck.Num() < CardCount) MainCardManager->InitializeDeck();
    if (MainCardManager->CurrentDeck.Num() < CardCount) return false;
    TArray<FCardData> PreparedCards = MainCardManager->DealCards(CardCount);
    if (PreparedCards.Num() != CardCount) return false;

    // 준비는 동기 처리입니다. 전체 카드가 확보된 뒤에만 플레이어 상태를 갱신합니다.
    ResetFoldState();
    DealtCards = MoveTemp(PreparedCards);
    for (int32 Index = 0; Index < Participants.Num(); ++Index)
    {
        Participants[Index].PlayerState->SetMyCard(DealtCards[Index]);
    }
    return true;
}

// 카드 비교 및 공개
void AMainGameMode::CheckPlayerCard()
{
	AMainGameState* GS = GetGameState<AMainGameState>();
    if (!GS) return;
	if (GS->CurrentGamePhase != EGamePhase::Playing || CurrentWinnerPS) return;

	// 최종 비교에 실제로 참가하는 플레이어만 예약한 토큰을 소모합니다.
	// 폴드했거나 사망한 플레이어의 예약은 취소되고 사용 횟수는 유지됩니다.
	for (ASeatActor* Seat : GS->SeatChairArray)
	{
		ACharacter* OccupantCharacter = Cast<ACharacter>(Seat->GetOccupant());
		if(!OccupantCharacter) continue;

		AMainPlayerState* PS = OccupantCharacter ->GetPlayerState<AMainPlayerState>();
		if(!PS) continue;

		PS->CommitAddTokenForComparison();
	}

    CurrentWinnerPS = MaxCardPlayer();
	if(!CurrentWinnerPS) return;

	TArray<FCardRevealEntry> RevealEntries;
	for (ASeatActor* Seat : GS->SeatChairArray)
	{
		ALobbyCharacter* Character = Seat ? Cast<ALobbyCharacter>(Seat->GetOccupant()) : nullptr;
		AMainPlayerState* PS = Character ? Character->GetPlayerState<AMainPlayerState>() : nullptr;
		if (!PS || !PS->isAlive || PS->isFold) continue;

		FCardRevealEntry& Entry = RevealEntries.AddDefaulted_GetRef();
		Entry.Card = PS->GetMyCard();
		Entry.PlayerName = PS->GetSteamNickname();
		if (Entry.PlayerName.IsEmpty()) Entry.PlayerName = PS->GetPlayerName();
		if (Entry.PlayerName.IsEmpty()) Entry.PlayerName = FString::Printf(TEXT("Player %d"), PS->GetPlayerId());
		Entry.ComparisonValue = PS->GetCardComparisonValue();
		Entry.bAddTokenSelected = PS->IsAddTokenSelected();
		Entry.bWinner = PS == CurrentWinnerPS;
	}

	// 카드가 지워지기 전에 확정값을 각 클라이언트에 전달합니다.
	GS->bTurnActionInProgress = true;
	GS->ClearTimerInfo();
	GS->SetGamePhase(EGamePhase::CardReveal);

	// 승자 판정이 끝났으므로 이번 라운드 카드 제거
    for (APlayerState* PlayerState : GS->PlayerArray)
    {
        if (AMainPlayerState* MPS = Cast<AMainPlayerState>(PlayerState))
        {
            MPS->SetMyCard(FCardData());
        }
    }
	
	if (CardRevealDurationSeconds <= 0.0f || RevealEntries.IsEmpty())
	{
		FinishCardReveal();
		return;
	}
	for (APlayerState* PlayerState : GS->PlayerArray)
	{
		AMainPlayerState* PS = Cast<AMainPlayerState>(PlayerState);
		if (AMainGamePlayerController* PC = PS ? Cast<AMainGamePlayerController>(PS->GetOwner()) : nullptr)
		{
			PC->Client_BeginCardReveal(RevealEntries, CardRevealDurationSeconds);
		}
	}
	GetWorldTimerManager().SetTimer(CardRevealTimerHandle, this, &AMainGameMode::FinishCardReveal,
		CardRevealDurationSeconds, false);
}

void AMainGameMode::FinishCardReveal()
{
	if (!HasAuthority()) return;
	AMainGameState* GS = GetGameState<AMainGameState>();
	if (!GS || GS->CurrentGamePhase != EGamePhase::CardReveal) return;
	GetWorldTimerManager().ClearTimer(CardRevealTimerHandle);
	for (APlayerState* PlayerState : GS->PlayerArray)
	{
		AMainPlayerState* PS = Cast<AMainPlayerState>(PlayerState);
		if (AMainGamePlayerController* PC = PS ? Cast<AMainGamePlayerController>(PS->GetOwner()) : nullptr)
		{
			PC->Client_EndCardReveal();
		}
	}
	AMainGamePlayerController* PC = IsValid(CurrentWinnerPS)
		? Cast<AMainGamePlayerController>(CurrentWinnerPS->GetOwner()) : nullptr;
	ALobbyCharacter* WinnerCharacter = PC ? Cast<ALobbyCharacter>(PC->GetPawn()) : nullptr;
	ARevolver* Revolver = GetMainRevolver();
	if (!IsValid(WinnerCharacter) || !IsValid(Revolver))
	{
		CurrentWinnerPS = nullptr;
		if (GS->AlivePlayerCount <= 1) EndGame(GetLastAlivePlayer());
		else NextRound();
		return;
	}

	// 승자를 메인 리볼버 사수이자 다음 라운드 선 플레이어로 지정
	// 미리 게임 턴 바꿔서 색깔 변경하기 위해서
	for (int32 i = 0; i < GS->SeatChairArray.Num(); ++i)
	{
		ASeatActor* Seat = GS->SeatChairArray[i];
		if (!Seat || !Seat->GetOccupant()) continue;

		ACharacter* Character = Cast<ACharacter>(Seat->GetOccupant());
		if (!Character) continue;

		AMainPlayerState* PS = Character->GetPlayerState<AMainPlayerState>();

		if (PS == CurrentWinnerPS)
		{
			CheckPlayer = PS->GetPlayerId();
			GS->ChangeGameTurn(PS->GetPlayerId(), i);
			break;
		}
	}
	
	UE_LOG(LogTemp, Warning, TEXT("[GM] MainRevolver is found"));

	GS->SetGamePhase(EGamePhase::Result);
	GS->SetMainShotInfo(CurrentWinnerPS->GetPlayerId(), GS->CurrentBulletCount);

	WinnerCharacter->SetActiveRevolver(Revolver);
	WinnerCharacter->BeginManualMainRevolverPhase(!Cast<ALobbyPCCharacter>(WinnerCharacter));

	// vr/pc 집는 모드 분할
	if(GS->CurrentBulletCount > 0)
	{
		if (ALobbyVRCharacter* WinnerVRCharacter = Cast<ALobbyVRCharacter>(WinnerCharacter))
		{
			WinnerVRCharacter->Client_HideMainGameWidget();
		}
		else if(Cast<ALobbyPCCharacter>(WinnerCharacter))
		{
			PC->Client_SetPCMainShotMode(true);
		}
	}
	ManageShotPhase();
}

// 활성 인원 중에서 가장 큰 값을 가진 플레이어
TObjectPtr<AMainPlayerState> AMainGameMode::MaxCardPlayer()
{
    AMainGameState* GS = GetGameState<AMainGameState>();
    if (!GS) return nullptr;

    AMainPlayerState* MaxPS = nullptr;
    FCardData MaxCard;
    int32 MaxComparisonValue = 0;

    bool bFound = false;

    for(ASeatActor* Seat : GS->SeatChairArray)
	{
		if(!Seat || !Seat->GetOccupant()) continue;

		ACharacter* OccupantCharacter = Cast<ACharacter>(Seat->GetOccupant());
		if (!OccupantCharacter) continue;

		AMainPlayerState* PS = OccupantCharacter -> GetPlayerState<AMainPlayerState>();
		if(!PS) continue;
		if(!PS -> isAlive) continue;
        if(PS -> isFold) continue;

        const FCardData CurrentCard = PS->GetMyCard();
        const int32 CurrentComparisonValue = PS->GetCardComparisonValue();
        const bool bHigher = !bFound || MainCardManager->IsCardHigher(CurrentCard, CurrentComparisonValue, MaxCard, MaxComparisonValue);

        if (bHigher)
        {
            MaxCard = CurrentCard;
            MaxComparisonValue = CurrentComparisonValue;
            MaxPS = PS;
            bFound = true;
        }
	}

    return MaxPS;
}

#endif // WITH_SERVER_CODE
