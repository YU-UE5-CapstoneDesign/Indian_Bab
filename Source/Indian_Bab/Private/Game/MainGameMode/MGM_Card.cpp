

#include "Game/MainGameMode.h"
#include "Game/MainGameState.h"
#include "Kismet/GameplayStatics.h"
#include "Character/LobbyCharacter.h"
#include "Character/LobbyVRCharacter.h"
#include "Actor/SeatActor.h"
#include "CardController/CardManager.h"
#include "PlayerState/MainPlayerState.h"
#include "PlayerController/MainGamePlayerController.h"
#include "GameFramework/Character.h"

#if WITH_SERVER_CODE

// 카드 매니저 획득
TObjectPtr<ACardManager> AMainGameMode::GetCardManager()
{
    if (!MainCardManager)
    {
        MainCardManager = Cast<ACardManager>(
            UGameplayStatics::GetActorOfClass(GetWorld(), ACardManager::StaticClass())
        );
    }
    return MainCardManager;
}

// 카드 분배
void AMainGameMode::DistributeCard()
{
	AMainGameState* GS = GetGameState<AMainGameState>();
	if (!GS) return;

	if (MainCardManager->CurrentDeck.Num() < GS->AlivePlayerCount)
	{
		UE_LOG(LogTemp, Warning, TEXT("Not enough cards. Reset deck before distribution: Alive=%d, Remaining=%d"),
			GS->AlivePlayerCount, MainCardManager->CurrentDeck.Num());
		MainCardManager->InitializeDeck();
	}

	DealtCards = MainCardManager->DealCards(GS->AlivePlayerCount);
	if (DealtCards.Num() != GS->AlivePlayerCount)
	{
		UE_LOG(LogTemp, Warning, TEXT("카드 배분 실패: Alive=%d, Dealt=%d"),
			GS->AlivePlayerCount, DealtCards.Num());
		return;
	}

	int32 CardIndex = 0;
	for(ASeatActor* Seat : GS->SeatChairArray)
	{
		if(!Seat || !Seat->GetOccupant()) continue;

		ACharacter* OccupantCharacter = Cast<ACharacter>(Seat->GetOccupant());
		if (!OccupantCharacter) continue;

		AMainPlayerState* PS = OccupantCharacter -> GetPlayerState<AMainPlayerState>();
		if(!PS) continue;
		if(!PS -> isAlive) continue;

		PS -> SetMyCard(DealtCards[CardIndex++]);
		UE_LOG(LogTemp, Warning, TEXT("PS[%d] : PS_Card(%s)"), PS->GetPlayerId(), *PS->GetMyCard().ToDisplayString());
	}
	return;
}

// 게임 결과 확인
// CurrentWinnerPS 업데이트
// 카드를 비교해 승자를 정하고 메인 총 단계와 PC 잡기 연출을 시작합니다.
void AMainGameMode::CheckPlayerCard()
{
	AMainGameState* GS = GetGameState<AMainGameState>();
    if (!GS) return;

    CurrentWinnerPS = MaxCardPlayer();
	if(!CurrentWinnerPS) return;

    UE_LOG(LogTemp, Warning, TEXT("[GM] Winner : %d[%s]"), CurrentWinnerPS -> GetPlayerId(), *CurrentWinnerPS->GetMyCard().ToDisplayString());
	
	AMainGamePlayerController* PC = Cast<AMainGamePlayerController>(CurrentWinnerPS->GetOwner());
	if (!PC) return;

	ALobbyCharacter* WinnerCharacter = Cast<ALobbyCharacter>(PC->GetPawn());
	if (!WinnerCharacter)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GM] Winner pawn is not ALobbyCharacter"));
		return;
	}

	ARevolver* Revolver = GetMainRevolver();
	if (!Revolver) 
	{
		UE_LOG(LogTemp, Warning, TEXT("[GM] MainRevolver is NULL"));
		return;
	}
	UE_LOG(LogTemp, Warning, TEXT("[GM] MainRevolver is found"));

	GS->SetGamePhase(EGamePhase::Result);
	GS->SetMainShotInfo(CurrentWinnerPS->GetPlayerId(), GS->CurrentBulletCount);

	WinnerCharacter->SetActiveRevolver(Revolver);
	WinnerCharacter->BeginManualMainRevolverPhase();

	if (ALobbyVRCharacter* WinnerVRCharacter = Cast<ALobbyVRCharacter>(WinnerCharacter))
	{
		WinnerVRCharacter->Client_HideMainGameWidget();
	}

	ManageShotPhase();
	if (!Cast<ALobbyVRCharacter>(WinnerCharacter) && GS->CurrentBulletCount > 0)
	{
		PC->Client_SetPCMainShotMode(true);
		WinnerCharacter->Multicast_BeginPCMainRevolver(Revolver);
	}
}

// 활성 인원 중에서 가장 큰 값을 가진 플레이어
TObjectPtr<AMainPlayerState> AMainGameMode::MaxCardPlayer()
{
    AMainGameState* GS = GetGameState<AMainGameState>();
    if (!GS) return nullptr;

    AMainPlayerState* MaxPS = nullptr;
    FCardData MaxCard;

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

        FCardData CurrentCard = PS->GetMyCard();

        if (!bFound || MainCardManager -> IsCardHigher(CurrentCard, MaxCard))
        {
            MaxCard = CurrentCard;
            MaxPS = PS;
            bFound = true;
        }
	}

    return MaxPS;
}

#endif // WITH_SERVER_CODE
