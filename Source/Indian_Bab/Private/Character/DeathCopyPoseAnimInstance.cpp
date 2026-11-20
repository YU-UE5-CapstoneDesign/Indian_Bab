#include "Character/DeathCopyPoseAnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "AnimNodes/AnimNode_CopyPoseFromMesh.h"

namespace
{
    struct FDeathCopyPoseProxy final : FAnimInstanceProxy
    {
        explicit FDeathCopyPoseProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance)
        {
            CopyPose.bUseAttachedParent = true;
            CopyPose.bCopyCurves = true;
        }
        virtual FAnimNode_Base* GetCustomRootNode() override { return &CopyPose; }
        virtual void GetCustomNodes(TArray<FAnimNode_Base*>& OutNodes) override { OutNodes.Add(&CopyPose); }
        FAnimNode_CopyPoseFromMesh CopyPose;
    };
}

FAnimInstanceProxy* UDeathCopyPoseAnimInstance::CreateAnimInstanceProxy()
{
    return new FDeathCopyPoseProxy(this);
}

void UDeathCopyPoseAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
    delete static_cast<FDeathCopyPoseProxy*>(InProxy);
}
