#pragma once

#include "CoreMinimal.h"
#include "DeathPresentationTypes.generated.h"

// Direction is where the shot came from, relative to the victim's seat.
UENUM(BlueprintType)
enum class EDeathReaction : uint8
{
    None,
    Front,
    Left,
    Right,
    Fold
};

USTRUCT()
struct FDeathPresentationState
{
    GENERATED_BODY()

    UPROPERTY()
    EDeathReaction Reaction = EDeathReaction::None;

    UPROPERTY()
    float ServerStartTime = 0.0f;
};
