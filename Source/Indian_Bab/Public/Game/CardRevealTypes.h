#pragma once

#include "CoreMinimal.h"
#include "CardController/CardData.h"
#include "CardRevealTypes.generated.h"

// 결과 공개 연출에 필요한 이번 라운드의 확정값입니다.
USTRUCT(BlueprintType)
struct FCardRevealEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FCardData Card;

	UPROPERTY(BlueprintReadOnly)
	FString PlayerName;

	UPROPERTY(BlueprintReadOnly)
	int32 ComparisonValue = 0;

	UPROPERTY(BlueprintReadOnly)
	bool bAddTokenSelected = false;

	UPROPERTY(BlueprintReadOnly)
	bool bWinner = false;
};
