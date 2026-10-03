#include "PlayerState/MainPlayerState.h"
#include "Game/MainGameState.h"
#include "Net/UnrealNetwork.h"

AMainPlayerState::AMainPlayerState()
{
    isAlive = 1;
    isFold = false;
    TotalTriggerCount = 0;
    AddTokenUsedCount = 0;
    bAddTokenSelected = false;
    bAddTokenAppliedThisRound = false;
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
    DOREPLIFETIME_CONDITION(AMainPlayerState, AddTokenUsedCount, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(AMainPlayerState, bAddTokenSelected, COND_OwnerOnly);
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

bool AMainPlayerState::SetAddTokenSelected(bool bSelected)
{
    if (bSelected && (!isAlive || isFold || AddTokenUsedCount >= MaxAddTokenUses)) return false;
    if (bAddTokenSelected == bSelected) return true;

    bAddTokenSelected = bSelected;
    OnRep_AddTokenState();
    ForceNetUpdate();
    return true;
}

bool AMainPlayerState::CommitAddTokenForComparison()
{
    // 같은 최종 비교가 중복 호출 방지
    if (bAddTokenAppliedThisRound)
    {
        bAddTokenSelected = false;
        return false;
    }

    const bool bHasComparableCard = MyCard.Value > 0 || MyCard.IsJoker();
    bAddTokenAppliedThisRound = bAddTokenSelected && isAlive && !isFold && bHasComparableCard && AddTokenUsedCount < MaxAddTokenUses;

    if (bAddTokenAppliedThisRound) ++AddTokenUsedCount;

    bAddTokenSelected = false;
    OnRep_AddTokenState();
    ForceNetUpdate();

    // 실제 판정에 사용한다는 뜻
    return bAddTokenAppliedThisRound;
}

void AMainPlayerState::ResetAddToken()
{
    bAddTokenSelected = false;
    bAddTokenAppliedThisRound = false;
    OnRep_AddTokenState();
    ForceNetUpdate();
}

int32 AMainPlayerState::GetRemainingAddTokenCount() const
{
    return FMath::Max(0, MaxAddTokenUses - AddTokenUsedCount);
}

int32 AMainPlayerState::GetCardComparisonValue() const
{
    // 토큰 사용 x
    if (!bAddTokenAppliedThisRound) return MyCard.Value;

    // 토큰을 사용한 조커는 종류와 관계없이 일반 카드보다 낮게 처리합니다.
    if (MyCard.IsJoker() || MyCard.Value <= 0) return 0;

    // 일반 카드는 +3 후 13을 넘으면 1부터 다시 시작합니다.
    return ((MyCard.Value - 1 + 3) % 13) + 1;
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

void AMainPlayerState::OnRep_AddTokenState()
{
    OnAddTokenStateChanged.Broadcast();
}
