#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "DeathCopyPoseAnimInstance.generated.h"

// Copies the attached parent's death pose while retaining this mesh's extra bones.
UCLASS(Transient, NotBlueprintable)
class INDIAN_BAB_API UDeathCopyPoseAnimInstance : public UAnimInstance
{
    GENERATED_BODY()
protected:
    virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
    virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;
};
