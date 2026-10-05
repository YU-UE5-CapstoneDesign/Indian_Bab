#include "Game/MainGameMode.h"
#include "Game/MainGameState.h"
#include "Actor/SeatActor.h"
#include "PlayerState/MainPlayerState.h"
#include "GameFramework/Character.h"

#if WITH_SERVER_CODE

// 플레이어 선택
void AMainGameMode::PickPlayer(int32 CurrentPlayerIndex)
{
	if (!HasAuthority()) return;

	AMainGameState* GS = GetGameState<AMainGameState>();
	if(!GS) return;
	if(GS -> CurrentGamePhase != EGamePhase::Playing) return;
	
	// 이제 막 게임 시작한 케이스
	if(CurrentPlayerIndex == -1 )
	{
		PickRandomPlayer();
	}
	// Result인 경우 결과 확인 페이즈인 케이스
	if(CurrentPlayerIndex != -1)
	{
		// 결과 판별 아직 안 만들어서 임시 구현
		PickByResult();
	}
	return;
}

// 플레이어 랜덤 선택
void AMainGameMode::PickRandomPlayer()
{
	if (!HasAuthority()) return;

	AMainGameState* GS = GetGameState<AMainGameState>();
	if (!GS) return;

	TArray<int32> EligibleSeatIndices;
	for (int32 Index = 0; Index < GS->SeatChairArray.Num(); ++Index)
	{
		ASeatActor* Seat = GS->SeatChairArray[Index];
		ACharacter* Character = IsValid(Seat) ? Cast<ACharacter>(Seat->GetOccupant()) : nullptr;
		AMainPlayerState* PS = IsValid(Character) ? Character->GetPlayerState<AMainPlayerState>() : nullptr;
		if (IsValid(PS) && PS->isAlive)
		{
			EligibleSeatIndices.Add(Index);
		}
	}
	if (EligibleSeatIndices.IsEmpty()) return;

	const int32 SeatIndex = EligibleSeatIndices[FMath::RandRange(0, EligibleSeatIndices.Num() - 1)];
	ACharacter* Character = Cast<ACharacter>(GS->SeatChairArray[SeatIndex]->GetOccupant());
	AMainPlayerState* PS = Character->GetPlayerState<AMainPlayerState>();
	CheckPlayer = PS->GetPlayerId();
	GS->ChangeGameTurn(CheckPlayer, SeatIndex);
}

void AMainGameMode::PickByResult()
{
    if (!HasAuthority()) return;

    AMainGameState* GS = GetGameState<AMainGameState>();
    if (!GS) return;

    AMainPlayerState* WinPS = MaxCardPlayer();
    if (!WinPS) return;
    
    for (int32 i = 0; i < GS->SeatChairArray.Num(); i++)
    {
        ASeatActor* Seat = GS->SeatChairArray[i];
        if(!Seat || !Seat->GetOccupant()) continue;

        ACharacter* OccupantCharacter = Cast<ACharacter>(Seat->GetOccupant());
        if(!OccupantCharacter) continue;

        AMainPlayerState* PS = OccupantCharacter->GetPlayerState<AMainPlayerState>();
        if(!PS) continue;
        if(!PS -> isAlive) continue;
        if(PS -> isFold) continue;

        if (PS == WinPS)
        {
            CheckPlayer = PS->GetPlayerId();
            GS->ChangeGameTurn(PS->GetPlayerId(), i);

            UE_LOG(LogTemp, Warning, TEXT("[GM] PickByResult -> Player %d selected as next starter"), PS->GetPlayerId());
            return;
        }
    }
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
