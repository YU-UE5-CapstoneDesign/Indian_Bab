#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Engine/StaticMesh.h"
#include "CardData.generated.h"

UENUM(BlueprintType)
enum class EJokerType : uint8
{
    None  UMETA(DisplayName = "None"),
    Black UMETA(DisplayName = "Black Joker"),
    Color UMETA(DisplayName = "Color Joker")
};

// 데이터 테이블의 각 행(Row)으로 사용할 카드 정보 구조체
USTRUCT(BlueprintType)
struct FCardData : public FTableRowBase
{
    GENERATED_BODY()

    // 카드 숫자 (일반 1~13, 조커 14, 0은 빈 카드)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card Info")
    int32 Value;

    // 카드 무늬 (Spade, Heart, Club, Diamond, Joker)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card Info")
    FString Suit;

    // 두 조커가 같은 숫자(14)를 사용해도 종류를 안전하게 구분합니다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card Info")
    EJokerType JokerType;

    // 카드별 3D 메시 에셋 (SM_Card_Spade_1 등)
    // TSoftObjectPtr: 게임 시작 시 53장을 한꺼번에 메모리에 올리지 않고 필요할 때만 로딩
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card Visual")
    TSoftObjectPtr<UStaticMesh> CardMesh;

    // 기본 생성자: 초기값 설정
    FCardData(): Value(0), Suit(TEXT("")), JokerType(EJokerType::None), CardMesh(nullptr) {}

    bool IsJoker() const
    {
        // JokerType이 아직 입력되지 않은 기존 데이터 테이블도 계속 인식합니다.
        return JokerType != EJokerType::None || Suit.Equals(TEXT("Joker"), ESearchCase::IgnoreCase);
    }

    int32 GetJokerRank() const
    {
        switch (JokerType)
        {
        case EJokerType::Black:
            return 0;
        case EJokerType::Color:
            return 1;
        default:
            // 기존 데이터는 흑백=14, 컬러=15로 저장되어 있습니다.
            if (IsJoker())
            {
                if (Value == 14) return 0;
                if (Value == 15) return 1;
            }
            return INDEX_NONE;
        }
    }

    FString GetDisplayRank() const
    {
        if (IsJoker())
        {
            if (JokerType == EJokerType::Black)
            {
                return TEXT("Black Joker");
            }

            if (JokerType == EJokerType::Color)
            {
                return TEXT("Color Joker");
            }

            return TEXT("Joker");
        }

        switch (Value)
        {
        case 1:
            return TEXT("A");
        case 11:
            return TEXT("J");
        case 12:
            return TEXT("Q");
        case 13:
            return TEXT("K");
        default:
            return FString::FromInt(Value);
        }
    }

    FString ToDisplayString() const
    {
        if (IsJoker())
        {
            return GetDisplayRank();
        }

        return FString::Printf(TEXT("%s %s"), *GetDisplayRank(), *Suit);
    }
};


