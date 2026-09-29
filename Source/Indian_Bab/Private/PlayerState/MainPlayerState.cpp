#include "PlayerState/MainPlayerState.h"
#include "Game/MainGameState.h"
#include "Net/UnrealNetwork.h"

AMainPlayerState::AMainPlayerState()
{
    isAlive = 1;
    TotalTriggerCount = 0;
}

void AMainPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 변수들을 클라이언트에게 복제(Replicate)하도록 등록
    DOREPLIFETIME(AMainPlayerState, isAlive);
    DOREPLIFETIME(AMainPlayerState, isFold);
    DOREPLIFETIME(AMainPlayerState, BulletArray);
    DOREPLIFETIME(AMainPlayerState, TotalTriggerCount);
    DOREPLIFETIME(AMainPlayerState, SteamNickname);
    DOREPLIFETIME(AMainPlayerState, MyCard);
}

// 닉네임 Set/Get 함수
void AMainPlayerState::SetSteamNickname(const FString& NewNickname)
{
    SteamNickname = NewNickname;
    SetPlayerName(NewNickname);
    OnSteamNicknameChanged.Broadcast();
}

FString AMainPlayerState::GetSteamNickname() const
{
    return SteamNickname;
}

// 카드 Set/Get 함순
void AMainPlayerState::SetMyCard(const FCardData& NewCard)
{
    MyCard = NewCard;
    OnCardChanged.Broadcast(); 
}

FCardData AMainPlayerState::GetMyCard() const
{
    return MyCard;
}

// 처음 서브 리볼버 설정
void AMainPlayerState::SetInitSubRevolver()
{
    BulletArray.SetNum(8);

    for (int32 i = 0; i < BulletArray.Num(); i++)
    {
        BulletArray[i] = 0;
    }
    int32 RandomIndex = FMath::RandRange(0, BulletArray.Num() - 1);
    BulletArray[RandomIndex] = 1;
    
    TotalTriggerCount = 0;
    OnRep_TotalTriggerCount();
    SetAliveState(true);
}

void AMainPlayerState::SetAliveState(bool bNewAlive)
{
    if (isAlive == bNewAlive)
    {
        OnAliveStateChanged.Broadcast(isAlive);
        return;
    }

    isAlive = bNewAlive;
    OnRep_isAlive();
}

// 서브 리볼버 당김횟부 변화(+1)
bool AMainPlayerState::ChangeSubRevolver()
{
    isFold = 1;
    if(BulletArray[TotalTriggerCount])
    {
        SetAliveState(false);
    }

    TotalTriggerCount++;
    OnRep_TotalTriggerCount();

    return isAlive;
}

void AMainPlayerState::OnRep_TotalTriggerCount()
{
    UE_LOG(LogTemp, Warning, TEXT("[PS_%d] : %d / 8"), GetPlayerId(), TotalTriggerCount);
    OnTriggerCountChanged.Broadcast(TotalTriggerCount);
}

void AMainPlayerState::OnRep_isAlive()
{
    if(isAlive == 0)
        UE_LOG(LogTemp, Warning, TEXT("[PS_%d] : dead!"), GetPlayerId());

    OnAliveStateChanged.Broadcast(isAlive);
}

void AMainPlayerState::OnRep_isFold()
{
    if(isFold == 1)
        UE_LOG(LogTemp, Warning, TEXT("[PS_%d] : Fold!"), GetPlayerId());
}

void AMainPlayerState::OnRep_SteamNickname()
{
    UE_LOG(LogTemp, Warning, TEXT("[PS_%d] SteamNickname: %s"), GetPlayerId(), *SteamNickname);
    OnSteamNicknameChanged.Broadcast();
}

void AMainPlayerState::OnRep_MyCard()
{
    UE_LOG(LogTemp, Warning, TEXT("[PS_%d] Card updated (Client)"), GetPlayerId());
    OnCardChanged.Broadcast();
}
