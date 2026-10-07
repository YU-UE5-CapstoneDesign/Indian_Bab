

#include "Game/MainGameMode.h"
#include "Game/MainGameState.h"
#include "Kismet/GameplayStatics.h"
#include "Character/LobbyCharacter.h"
#include "Character/LobbyPCCharacter.h"
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

	// 승자 판정이 끝났으므로 이번 라운드 카드 제거
    for (APlayerState* PlayerState : GS->PlayerArray)
    {
        if (AMainPlayerState* MPS = Cast<AMainPlayerState>(PlayerState))
        {
            MPS->SetMyCard(FCardData());
        }
    }
	
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
