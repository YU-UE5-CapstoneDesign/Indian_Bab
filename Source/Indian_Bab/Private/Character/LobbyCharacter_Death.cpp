#include "Character/LobbyCharacter.h"
#include "Character/DeathCopyPoseAnimInstance.h"
#include "Actor/Revolver.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"

EDeathReaction ALobbyCharacter::GetDeathReactionFromSource(const FVector& SourceLocation) const
{
    const FVector Local = GetActorTransform().InverseTransformVectorNoScale(SourceLocation - GetActorLocation());
    if (FMath::Abs(Local.Y) > FMath::Abs(Local.X))
        return Local.Y < 0.0f ? EDeathReaction::Left : EDeathReaction::Right;
    return EDeathReaction::Front;
}

void ALobbyCharacter::SetDeathPresentation(EDeathReaction Reaction)
{
    if (!HasAuthority() || DeathState.Reaction == Reaction) return;
    // A second notification must not replay or change an already selected death.
    if (IsDeathPresentationActive() && Reaction != EDeathReaction::None) return;
    DeathState.Reaction = Reaction;
    const AGameStateBase* GS = GetWorld()->GetGameState();
    DeathState.ServerStartTime = GS ? GS->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
    OnRep_DeathState();
    ForceNetUpdate();
}

UAnimSequence* ALobbyCharacter::GetDeathAnimation() const
{
    switch (DeathState.Reaction)
    {
    case EDeathReaction::Left: return LeftDeathAnimation;
    case EDeathReaction::Right: return RightDeathAnimation;
    case EDeathReaction::Fold: return FoldDeathAnimation;
    case EDeathReaction::Front: return FrontDeathAnimation;
    default: return nullptr;
    }
}

void ALobbyCharacter::PrepareDeathPresentation()
{
}

void ALobbyCharacter::OnRep_DeathState()
{
    // Fold death can be decided inside PostAnimEvaluation's montage-end callback.
    // Keep state changes immediate, but change animation instances in the actor tick.
    bDeathPresentationPending = true;
}

void ALobbyCharacter::ApplyDeathPresentation()
{
    if (!IsDeathPresentationActive())
    {
        ResetDeathPresentation();
        return;
    }
    if (bDeathVisualApplied) return;
    bDeathVisualApplied = true;
    bDeathSavedControllerYaw = bUseControllerRotationYaw;
    bUseControllerRotationYaw = false;
    GetCharacterMovement()->StopMovementImmediately();
    GetCharacterMovement()->DisableMovement();
    bShowMainShotAimLine = false;
    bMainRevolverGrabbed = false;
    bMainRevolverGrabInProgress = false;
    bIsPuttingBackGun = false;
    GunHoldReason = EGunHoldReason::None;

    // Restore a VR grab's temporary meshes/camera before replacing the live rig.
    PrepareDeathPresentation();
    ClearHeldRevolverMeshes();
    if (ActiveRevolver && ActiveRevolver == DeskRevolver)
    {
        DeskRevolver->SetActorHiddenInGame(false);
        if (DeskRevolver->CollisionSphere)
            DeskRevolver->CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    }
    if (CameraComponent)
    {
        bDeathSavedFirstPersonFOV = CameraComponent->bEnableFirstPersonFieldOfView;
        bDeathSavedFirstPersonScale = CameraComponent->bEnableFirstPersonScale;
        CameraComponent->bEnableFirstPersonFieldOfView = false;
        CameraComponent->bEnableFirstPersonScale = false;
        // Preserve the spectator's position. VR continues to use its HMD origin.
        if (IsLocallyControlled() && !CameraComponent->bLockToHmd)
        {
            DeathCameraParent = CameraComponent->GetAttachParent();
            DeathCameraSocket = CameraComponent->GetAttachSocketName();
            DeathCameraTransform = CameraComponent->GetRelativeTransform();
            CameraComponent->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
            bDeathCameraDetached = true;
        }
    }
    if (GetNetMode() == NM_DedicatedServer) return;
    UAnimSequence* Sequence = GetDeathAnimation();
    if (!Sequence)
    {
        UE_LOG(LogTemp, Warning, TEXT("[%s] Death animation is not assigned (%d)."), *GetName(), int32(DeathState.Reaction));
        return;
    }
    auto SaveMesh = [this](USkeletalMeshComponent* Component)
    {
        if (!Component || !Component->GetSkeletalMeshAsset()) return;
        FDeathMeshState& State = DeathMeshStates.AddDefaulted_GetRef();
        State.Component = Component;
        State.Leader = Component->LeaderPoseComponent;
        State.AnimClass = Component->GetAnimClass();
        State.Mode = Component->GetAnimationMode();
        State.bPauseAnims = Component->bPauseAnims;
        State.bDisablePostProcess = Component->GetDisablePostProcessBlueprint();
        State.VisibilityTickOption = uint8(Component->VisibilityBasedAnimTickOption);
        Component->SetDisablePostProcessBlueprint(true);
        Component->bPauseAnims = false;
        Component->SetLeaderPoseComponent(nullptr, true);
    };
    USkeletalMeshComponent* Bodies[] = {GetMesh()};
    for (USkeletalMeshComponent* Body : Bodies)
    {
        if (!Body || !Body->GetSkeletalMeshAsset()) continue;
        SaveMesh(Body);
        // Keep the mesh's deformation correctives; only the live AnimBP is replaced.
        Body->SetDisablePostProcessBlueprint(false);
        if (UAnimInstance* Anim = Body->GetAnimInstance()) Anim->Montage_Stop(0.0f);
        Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
        Body->PlayAnimation(Sequence, false);
        if (UAnimSingleNodeInstance* Anim = Body->GetSingleNodeInstance()) Anim->SetPlaying(false);
    }
    // Copy local poses rather than a shared skinning buffer: these meshes have extra bones.
    auto FollowBody = [&](USkeletalMeshComponent* Follower, USkeletalMeshComponent* Body)
    {
        if (!Follower || !Body || !Follower->GetSkeletalMeshAsset() || !Body->GetSkeletalMeshAsset()) return;
        SaveMesh(Follower);
        Follower->SetAnimInstanceClass(UDeathCopyPoseAnimInstance::StaticClass());
        Follower->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
        if (Follower == FirstPersonMetaHumanBody || Follower == ThirdPersonMetaHumanBody)
            Follower->SetDisablePostProcessBlueprint(false);
    };
    FollowBody(FirstPersonMetaHumanBody, GetMesh());
    FollowBody(ThirdPersonMetaHumanBody, GetMesh());
    FollowBody(FirstPersonMetaHumanTorso, GetMesh());
    FollowBody(ThirdPersonMetaHumanTorso, GetMesh());
    FollowBody(ThirdPersonMetaHumanFace, GetMesh());
    UpdateDeathAnimationTime();
}

void ALobbyCharacter::UpdateDeathAnimationTime()
{
    UAnimSequence* Sequence = GetDeathAnimation();
    if (!bDeathVisualApplied || !Sequence || GetNetMode() == NM_DedicatedServer) return;
    const AGameStateBase* GS = GetWorld()->GetGameState();
    const float Now = GS ? GS->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
    const float Position = FMath::Clamp(Now - DeathState.ServerStartTime, 0.0f, Sequence->GetPlayLength());
    // Seeking without notifies also corrects an initially unsynchronized late join.
    for (USkeletalMeshComponent* Body : {GetMesh()})
        if (Body && Body->GetSingleNodeInstance()) Body->SetPosition(Position, false);
}

void ALobbyCharacter::ResetDeathPresentation()
{
    if (!bDeathVisualApplied) return;
    for (const FDeathMeshState& State : DeathMeshStates)
        if (USkeletalMeshComponent* Component = State.Component.Get())
        {
            Component->SetLeaderPoseComponent(nullptr, true);
            Component->SetAnimationMode(EAnimationMode::Type(State.Mode));
            Component->SetAnimInstanceClass(State.AnimClass);
            Component->SetDisablePostProcessBlueprint(State.bDisablePostProcess);
            Component->bPauseAnims = State.bPauseAnims;
            Component->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption(State.VisibilityTickOption);
        }
    for (const FDeathMeshState& State : DeathMeshStates)
        if (USkeletalMeshComponent* Component = State.Component.Get())
            Component->SetLeaderPoseComponent(State.Leader.Get(), true);
    DeathMeshStates.Reset();
    if (CameraComponent)
    {
        if (bDeathCameraDetached && DeathCameraParent.IsValid())
        {
            CameraComponent->AttachToComponent(DeathCameraParent.Get(), FAttachmentTransformRules::KeepRelativeTransform, DeathCameraSocket);
            CameraComponent->SetRelativeTransform(DeathCameraTransform);
        }
        CameraComponent->bEnableFirstPersonFieldOfView = bDeathSavedFirstPersonFOV;
        CameraComponent->bEnableFirstPersonScale = bDeathSavedFirstPersonScale;
    }
    bDeathCameraDetached = false;
    bUseControllerRotationYaw = bDeathSavedControllerYaw;
    GetCharacterMovement()->SetMovementMode(bIsSitting ? MOVE_None : MOVE_Walking);
    bDeathVisualApplied = false;
}
