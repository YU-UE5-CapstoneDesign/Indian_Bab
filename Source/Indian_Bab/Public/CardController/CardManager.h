#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CardController/CardData.h"
#include "CardManager.generated.h"

UCLASS()
class INDIAN_BAB_API ACardManager : public AActor
{
    GENERATED_BODY()

public:
    ACardManager();

    // 에디터에서 직접 할당할 데이터 테이블
    // 이 테이블에 53장의 카드 정보(숫자, 무늬, 메시)가 들어있음
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card Logic")
    UDataTable* CardDataTable;

    // 현재 게임에서 실제로 사용 중인 덱 (데이터 테이블의 복사본)
    // 카드를 뽑을 때마다 이 배열에서 제거됨
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Card Logic")
    TArray<FCardData> CurrentDeck;

    // 데이터 테이블에서 카드를 읽어와 덱을 구성하고 셔플하는 함수
    // 매 라운드 시작 시 호출
    UFUNCTION(BlueprintCallable, Category = "Card Logic")
    void InitializeDeck();

    // 덱 맨 위에서 카드 한 장을 뽑고 덱에서 제거하는 함수
    UFUNCTION(BlueprintCallable, Category = "Card Logic")
    FCardData DrawCard();

    // 플레이어 수만큼 카드를 한 장씩 뽑아 배열로 반환하는 함수
    // 반환된 배열의 인덱스 = 플레이어 순서
    UFUNCTION(BlueprintCallable, Category = "Card Logic")
    TArray<FCardData> DealCards(int32 PlayerCount);

    // 문양에 따른 순위
    int32 GetSuitRank(const FString& Suit);

    // 어떤 카드가 더 큰 지 비교
    bool IsCardHigher(const FCardData& A, int32 AComparisonValue, const FCardData& B, int32 BComparisonValue);

protected:
    // 게임 시작 시 자동으로 덱 초기화
    virtual void BeginPlay() override;
};
