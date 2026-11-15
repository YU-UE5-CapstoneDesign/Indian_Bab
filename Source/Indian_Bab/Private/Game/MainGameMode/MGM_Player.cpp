#include "Game/MainGameMode.h"
#include "Game/MainGameState.h"
#include "Actor/SeatActor.h"
#include "PlayerState/MainPlayerState.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "CardController/CardManager.h"

#if WITH_SERVER_CODE

// 살아 있는 플레이어 목록 생성
bool AMainGameMode::BuildRoundParticipants(TArray<FMainRoundParticipant>& OutParticipants) const
{
    OutParticipants.Reset();

    const AMainGameState* GS = GetGameState<AMainGameState>();
    if (!GS) return false;

    TSet<AMainPlayerState*> Seen;
    for (int32 Index = 0; Index < GS->SeatChairArray.Num(); Index++)
    {
        ASeatActor* Seat = GS->SeatChairArray[Index];
        ACharacter* Character = IsValid(Seat) ? Cast<ACharacter>(Seat->GetOccupant()) : nullptr;
        AMainPlayerState* PS = IsValid(Character) ? Character->GetPlayerState<AMainPlayerState>() : nullptr;
        if (!IsValid(PS) || !PS->isAlive) continue;

        APlayerController* PC = Cast<APlayerController>(PS->GetOwner());
        if (!IsValid(PC) || PC->GetPawn() != Character || !GS->PlayerArray.Contains(PS)) return false;

        if (Seen.Contains(PS)) return false;
        Seen.Add(PS);
        FMainRoundParticipant& Participant = OutParticipants.AddDefaulted_GetRef();
        Participant.PlayerState = PS;
        Participant.SeatIndex = Index;
    }

    // 누락된 생존자를 사망자로 간주하여 승자를 잘못 판정하지 않습니다.
    for (APlayerState* PlayerState : GS->PlayerArray)
    {
        AMainPlayerState* PS = Cast<AMainPlayerState>(PlayerState);
        if (IsValid(PS) && PS->isAlive && !Seen.Contains(PS)) return false;
    }
    return true;
}

// 라운드 시작 플레이어 선택
int32 AMainGameMode::SelectRoundStarter(const TArray<FMainRoundParticipant>& Participants) const
{
    const AMainGameState* GS = GetGameState<AMainGameState>();
    if (!GS || Participants.IsEmpty() || !IsValid(MainCardManager)) return INDEX_NONE;
    if (GS->CurrentPlayerIndex == INDEX_NONE) return FMath::RandRange(0, Participants.Num() - 1);

    // 폴드 초기화와 새 카드 지급 전에 이전 라운드의 결과를 보존합니다.
    int32 BestIndex = INDEX_NONE;
    for (int32 Index = 0; Index < Participants.Num(); ++Index)
    {
        AMainPlayerState* PS = Participants[Index].PlayerState;
        if (PS->isFold || PS->GetMyCard().Value <= 0) continue;
        AMainPlayerState* BestPS = BestIndex != INDEX_NONE ? Participants[BestIndex].PlayerState : nullptr;
        if (!BestPS || MainCardManager->IsCardHigher(PS->GetMyCard(), PS->GetCardComparisonValue(),
            BestPS->GetMyCard(), BestPS->GetCardComparisonValue())) BestIndex = Index;
    }
    if (BestIndex != INDEX_NONE) return BestIndex;
    // 공개 단계에서 카드가 지워졌다면 확정된 승자의 턴 ID를 사용합니다.
    for (int32 Index = 0; Index < Participants.Num(); ++Index)
    {
        if (Participants[Index].PlayerState->GetPlayerId() == GS->CurrentTurnPlayerId
            && !Participants[Index].PlayerState->isFold) return Index;
    }
    return FMath::RandRange(0, Participants.Num() - 1);
}

int32 AMainGameMode::CountAliveSeatedPlayers() const
{
	const AMainGameState* GS = GetGameState<AMainGameState>();
	if (!GS) return 0;

	int32 Count = 0;
	for (ASeatActor* Seat : GS->SeatChairArray)
	{
		ACharacter* Character = IsValid(Seat) ? Cast<ACharacter>(Seat->GetOccupant()) : nullptr;
		AMainPlayerState* PS = IsValid(Character) ? Character->GetPlayerState<AMainPlayerState>() : nullptr;
		if (IsValid(PS) && PS->isAlive) ++Count;
	}
	return Count;
}

// 활성 인원 업데이트
int32 AMainGameMode::UpdateActivePlayer(AMainGameState* GS)
{
    if (!GS) return 0;

	int32 ActivePlayer = 0;
	for(ASeatActor* Seat : GS->SeatChairArray)
	{
		if(!Seat || !Seat->GetOccupant()) continue;

		ACharacter* OccupantCharacter = Cast<ACharacter>(Seat->GetOccupant());
		if (!OccupantCharacter) continue;

		AMainPlayerState* PS = OccupantCharacter -> GetPlayerState<AMainPlayerState>();
		if(!PS) continue;
		if(!PS -> isAlive) continue;
		if(PS -> isFold) continue;

		ActivePlayer++;
	}
	return ActivePlayer;
}

// 다음 플레이어 Get
// MainPlayerState* 리턴 및 GS의 CurrentPlayerIndex 업데이트
TObjectPtr<AMainPlayerState> AMainGameMode::GetNextPlayerState(int32 CurrentPlayerIndex)
{
	if (!HasAuthority()) return nullptr;

	AMainGameState* GS = GetGameState<AMainGameState>();
    if (!GS) return nullptr;

	const int32 NumSeats = GS->SeatChairArray.Num();
    if (NumSeats <= 0) return nullptr;

	int32 NextPlayerIndex = CurrentPlayerIndex;
	for(int i = 0; i < NumSeats; i++)
	{
		NextPlayerIndex = (NextPlayerIndex + 1) % NumSeats;
		
		ASeatActor* NextChair = GS -> SeatChairArray[NextPlayerIndex];
		if(!NextChair || !NextChair -> GetOccupant()) continue;

		ACharacter* OccupantCharacter = Cast<ACharacter>(NextChair->GetOccupant());
		if (!OccupantCharacter) continue;

		AMainPlayerState* NextPS = OccupantCharacter->GetPlayerState<AMainPlayerState>();
		if (!NextPS) continue;
		if (!NextPS->isAlive) continue;
		if (NextPS->isFold) continue;

		GS -> CurrentPlayerIndex = NextPlayerIndex;
		return NextPS;
	}
	return nullptr;
}

#endif // WITH_SERVER_CODE
