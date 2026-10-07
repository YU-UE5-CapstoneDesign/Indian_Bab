#include "CardController/CardManager.h"
#include "CardController/CardData.h"

// 생성자: Tick이 필요 없으므로 꺼둠
ACardManager::ACardManager()
{
    PrimaryActorTick.bCanEverTick = false;
}

// 게임 시작 시 자동으로 덱 초기화 실행
void ACardManager::BeginPlay()
{
    Super::BeginPlay();

    //InitializeDeck();
}

void ACardManager::InitializeDeck()
{
    // 데이터 테이블이 에디터에서 할당되지 않은 경우 경고 후 종료
    if (!CardDataTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("CardDataTable이 할당되지 않았습니다!"));
        return;
    }

    // 이전 라운드에 남아있던 덱 데이터를 비움
    CurrentDeck.Empty();

    // 데이터 테이블의 모든 행을 포인터 배열로 가져옴
    TArray<FCardData*> AllRows;
    CardDataTable->GetAllRows<FCardData>(TEXT("Context_CardInit"), AllRows);
    UE_LOG(LogTemp, Warning, TEXT("AllRows.Num() = %d"), AllRows.Num());

    // 포인터 배열을 실제 구조체 배열로 복사
    for (FCardData* Row : AllRows)
    {
        if (Row) // 포인터가 유효한지 확인
        {
            CurrentDeck.Add(*Row); // 구조체 값 복사
        }
    }
    UE_LOG(LogTemp, Warning, TEXT("덱 초기화 완료: CurrentDeck.Num() = %d"), CurrentDeck.Num());

    // Fisher-Yates 셔플: 완전한 무작위를 보장하는 알고리즘
    // 뒤에서부터 순회하며 현재 위치와 무작위 위치의 카드를 교환
    const int32 NumCards = CurrentDeck.Num();
    for (int32 i = NumCards - 1; i > 0; --i)
    {
        // 0 ~ i 사이의 무작위 인덱스 선택
        int32 RandomIndex = FMath::RandRange(0, i);

        // 같은 위치면 교환할 필요 없음
        if (i != RandomIndex)
        {
            CurrentDeck.Swap(i, RandomIndex);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("덱 초기화 완료: 총 %d장"), CurrentDeck.Num());
}

FCardData ACardManager::DrawCard()
{
    // 덱에 카드가 남아있는 경우
    if (CurrentDeck.Num() > 0)
    {
        FCardData Picked = CurrentDeck[0]; // 맨 위 카드 복사
        CurrentDeck.RemoveAt(0);           // 덱에서 제거
        return Picked;                     // 복사한 카드 반환
    }

    // 덱이 비어있으면 경고 후 빈 카드 반환
    UE_LOG(LogTemp, Warning, TEXT("덱에 카드가 없습니다!"));
    return FCardData();
}

TArray<FCardData> ACardManager::DealCards(int32 PlayerCount)
{
    TArray<FCardData> DealtCards; // 배분할 카드를 담을 배열

    // 플레이어 수가 유효한지 확인
    if (PlayerCount <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("플레이어 수가 올바르지 않습니다: %d"), PlayerCount);
        return DealtCards;
    }

    // 덱에 플레이어 수만큼 카드가 남아있는지 확인
    if (CurrentDeck.Num() < PlayerCount)
    {
        UE_LOG(LogTemp, Warning, TEXT("덱에 카드가 부족합니다! 필요: %d, 남은 수: %d"), PlayerCount, CurrentDeck.Num());
        return DealtCards;
    }

    // 플레이어 수만큼 카드를 한 장씩 뽑아 배열에 추가
    // DealtCards[0] = 첫 번째 플레이어 카드, DealtCards[1] = 두 번째 플레이어 카드 ...
    for (int32 i = 0; i < PlayerCount; i++)
    {
        DealtCards.Add(DrawCard());
    }

    UE_LOG(LogTemp, Log, TEXT("%d명에게 카드 배분 완료"), PlayerCount);
    return DealtCards;
}

int32 ACardManager::GetSuitRank(const FString& Suit)
{
    if (Suit == "Spade")   return 4;
    if (Suit == "Diamond") return 3;
    if (Suit == "Heart")   return 2;
    if (Suit == "Clover")  return 1;
    return 0;
}


bool ACardManager::IsCardHigher(const FCardData& A, int32 AComparisonValue, const FCardData& B, int32 BComparisonValue)
{
    if (AComparisonValue != BComparisonValue)
    {
        return AComparisonValue > BComparisonValue;
    }

    // 두 조커의 숫자는 모두 14지만 Color가 Black보다 높습니다.
    // 덧셈 토큰으로 둘 다 최저값이 된 경우에도 이 순서는 유지합니다.
    if (A.IsJoker() && B.IsJoker())
    {
        return A.GetJokerRank() > B.GetJokerRank();
    }

    return GetSuitRank(A.Suit) > GetSuitRank(B.Suit);
}