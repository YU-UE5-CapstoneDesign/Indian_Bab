#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Character/LobbyPCCharacter.h"
#include "Character/LobbyVRCharacter.h"
#include "Character/DeathCopyPoseAnimInstance.h"
#include "PlayerState/MainPlayerState.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/BufferArchive.h"
#include "ShaderCompiler.h"
#include "AssetCompilingManager.h"
#include "UObject/Script.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeathPresentationTest, "IndianBab.Character.SeatedDeath",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDeathPresentationTest::RunTest(const FString& Parameters)
{
    if (!FParse::Param(FCommandLine::Get(), TEXT("DeathMotionIsolated")))
    {
        AddWarning(TEXT("Skipped: use a disposable project with -DeathMotionIsolated."));
        return true;
    }
    FEditorScriptExecutionGuard ScriptGuard;
    UGameInstance* GI = NewObject<UGameInstance>(GEngine);
    GI->InitializeStandalone();
    UWorld* World = GI->GetWorld();
    ON_SCOPE_EXIT
    {
        GI->Shutdown();
        World->DestroyWorld(false);
        GEngine->DestroyWorldContext(World);
    };
    APlayerController* PC = World->SpawnActor<APlayerController>();
    AMainPlayerState* PS = World->SpawnActor<AMainPlayerState>();
    PC->PlayerState = PS;
    PS->SetOwner(PC);
    ADirectionalLight* Light = World->SpawnActor<ADirectionalLight>();
    Light->SetActorRotation(FRotator(-35.0f, 140.0f, 0.0f));
    Light->GetLightComponent()->SetIntensity(6.0f);
    ADirectionalLight* Fill = World->SpawnActor<ADirectionalLight>();
    Fill->SetActorRotation(FRotator(-25.0f, -40.0f, 0.0f));
    Fill->GetLightComponent()->SetIntensity(2.0f);

    FString Directory;
    const bool bRender = FParse::Value(FCommandLine::Get(), TEXT("DeathRenderDir="), Directory);
    USceneCaptureComponent2D* Capture = nullptr;
    UTextureRenderTarget2D* Target = nullptr;
    if (bRender)
    {
        IFileManager::Get().MakeDirectory(*Directory, true);
        AActor* Camera = World->SpawnActor<AActor>();
        Capture = NewObject<USceneCaptureComponent2D>(Camera);
        Camera->SetRootComponent(Capture);
        Capture->RegisterComponent();
        Capture->bCaptureEveryFrame = false;
        Capture->bCaptureOnMovement = false;
        Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
        Capture->FOVAngle = 45.0f;
        Capture->ShowFlags.SetTemporalAA(false);
        Capture->PostProcessSettings.bOverride_AutoExposureMethod = true;
        Capture->PostProcessSettings.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
        Capture->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
        Capture->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = false;
        Capture->PostProcessSettings.bOverride_AutoExposureBias = true;
        Capture->PostProcessSettings.AutoExposureBias = 1.0f;
        Target = NewObject<UTextureRenderTarget2D>();
        Target->InitCustomFormat(1280, 720, PF_B8G8R8A8, false);
        Capture->TextureTarget = Target;
    }
    auto Refresh = [](ALobbyCharacter* Character)
    {
        for (USkeletalMeshComponent* Body : {Character->GetMesh(), Character->FirstPersonMetaHumanBody, Character->ThirdPersonMetaHumanBody,
            Character->FirstPersonMetaHumanTorso, Character->ThirdPersonMetaHumanTorso, Character->ThirdPersonMetaHumanFace})
            if (Body && Body->GetAnimInstance())
            {
                Body->TickAnimation(0.0f, false);
                Body->RefreshBoneTransforms();
            }
    };
    auto Render = [&](ALobbyCharacter* Character, const FString& Name)
    {
        if (!bRender) return;
        if (Name.EndsWith(TEXT("_Start")))
        {
            const FVector Center = (Character->ThirdPersonMetaHumanBody->GetSocketLocation(TEXT("head"))
                + Character->ThirdPersonMetaHumanBody->GetSocketLocation(TEXT("foot_l"))) * 0.5f;
            Capture->SetWorldLocation(Center + FVector(300.0f, -400.0f, 130.0f));
            Capture->SetWorldRotation((Center - Capture->GetComponentLocation()).Rotation());
        }
        // Skinning render buffers are cached per engine frame, even offscreen.
        for (int32 Frame = 0; Frame < 3; ++Frame)
        {
            ++GFrameCounter;
            World->Tick(LEVELTICK_All, 1.0f / 30.0f);
            Refresh(Character);
            World->SendAllEndOfFrameUpdates();
            Capture->CaptureScene();
        }
        FBufferArchive Png;
        TestTrue(TEXT("Export death preview"), FImageUtils::ExportRenderTarget2DAsPNG(Target, Png));
        TestTrue(TEXT("Save death preview"), FFileHelper::SaveArrayToFile(Png, *FPaths::Combine(Directory, Name + TEXT(".png"))));
    };
    for (const TCHAR* ClassName : {TEXT("PC"), TEXT("VR")})
    {
        const FString BPPath = FString::Printf(TEXT("/Game/Blueprint/Character/BP_LobbyCharacterBase_%s.BP_LobbyCharacterBase_%s_C"), ClassName, ClassName);
        UClass* Class = LoadClass<ALobbyCharacter>(nullptr, *BPPath);
        if (!TestNotNull(TEXT("Production character BP"), Class)) return false;
        ALobbyCharacter* Character = World->SpawnActor<ALobbyCharacter>(Class, FVector(0.0f, 0.0f, 88.0f), FRotator::ZeroRotator);
        if (bRender)
        {
            Capture->HiddenComponents.Add(Character->GetMesh());
            Capture->HiddenComponents.Add(Character->FirstPersonMetaHumanBody);
            Capture->HiddenComponents.Add(Character->FirstPersonMetaHumanTorso);
        }
        PC->Possess(Character);
        Character->bIsSitting = true;
        Character->bIsSittingEnded = true;
        Character->DispatchBeginPlay();
        Character->OnRep_IsSitting();
        if (FParse::Param(FCommandLine::Get(), TEXT("DeathForceLODZero")))
        {
            TInlineComponentArray<USkeletalMeshComponent*> Meshes(Character);
            for (USkeletalMeshComponent* Mesh : Meshes) Mesh->SetForcedLOD(1);
        }
        if (bRender)
        {
            FAssetCompilingManager::Get().FinishAllCompilation();
            if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
        }
        if (!TestTrue(TEXT("PlayerState owns the possessed pawn"), PS->GetPawn() == Character)) return false;
        TestEqual(TEXT("Front hit"), Character->GetDeathReactionFromSource(Character->GetActorLocation()+FVector(100,0,0)), EDeathReaction::Front);
        TestEqual(TEXT("Left hit"), Character->GetDeathReactionFromSource(Character->GetActorLocation()+FVector(0,-100,0)), EDeathReaction::Left);
        TestEqual(TEXT("Right hit"), Character->GetDeathReactionFromSource(Character->GetActorLocation()+FVector(0,100,0)), EDeathReaction::Right);
        Character->SetActorRotation(FRotator(0,90,0));
        TestEqual(TEXT("Direction follows the victim's seat rotation"), Character->GetDeathReactionFromSource(Character->GetActorLocation()+FVector(100,0,0)), EDeathReaction::Left);
        Character->SetActorRotation(FRotator::ZeroRotator);
        if (bRender)
        {
            UAnimSequence* DeathSequence = Character->FrontDeathAnimation;
            Character->FrontDeathAnimation = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Blueprint/Character/Animations/Sit/Anim_Sit.Anim_Sit"));
            PS->SetAliveState(false, EDeathReaction::Front);
            Character->Tick(0.0f);
            Refresh(Character);
            Render(Character, FString(ClassName)+TEXT("_Reference_Start"));
            PS->SetAliveState(true);
            Character->Tick(0.0f);
            Character->FrontDeathAnimation = DeathSequence;
        }
        const UClass* InitialAnimClass = Character->GetMesh()->GetAnimClass();
        const USkeletalMesh* InitialFPBody = Character->FirstPersonMetaHumanBody->GetSkeletalMeshAsset();
        const USkeletalMesh* InitialTPBody = Character->ThirdPersonMetaHumanBody->GetSkeletalMeshAsset();
        const USkeletalMesh* InitialTorso = Character->ThirdPersonMetaHumanTorso->GetSkeletalMeshAsset();
        USkinnedMeshComponent* InitialFaceLeader = Character->ThirdPersonMetaHumanFace->LeaderPoseComponent.Get();
        for (EDeathReaction Reaction : {EDeathReaction::Front, EDeathReaction::Left, EDeathReaction::Right, EDeathReaction::Fold})
        {
            // Each life may start with a different live rig pose.
            const FTransform CameraBefore = Character->CameraComponent->GetComponentTransform();
            const FTransform CameraRelativeBefore = Character->CameraComponent->GetRelativeTransform();
            USceneComponent* CameraParentBefore = Character->CameraComponent->GetAttachParent();
            const FString Label = StaticEnum<EDeathReaction>()->GetNameStringByValue(int64(Reaction));
            PS->SetAliveState(false, Reaction);
            TestTrue(TEXT("Death disables alive state"), !PS->isAlive);
            TestEqual(TEXT("Server selects the requested reaction"), Character->DeathState.Reaction, Reaction);
            TestFalse(TEXT("Death does not change animation instances inside the state callback"), Character->bDeathVisualApplied);
            Character->Tick(0.0f);
            UAnimSequence* Sequence = Character->GetDeathAnimation();
            if (!TestNotNull(TEXT("Death sequence assigned in production BP"), Sequence)) return false;
            TestFalse(TEXT("Seated death does not move the actor with root motion"), Sequence->bEnableRootMotion);
            if (!TestNotNull(TEXT("Body switches from the live rig to death animation"), Character->GetMesh()->GetSingleNodeInstance())) return false;
            TestTrue(TEXT("Clothing copies the death pose with its own bones"), Character->ThirdPersonMetaHumanTorso->GetAnimInstance()->IsA<UDeathCopyPoseAnimInstance>());
            TestTrue(TEXT("Visible body copies the death pose with its own bones"), Character->ThirdPersonMetaHumanBody->GetAnimInstance()->IsA<UDeathCopyPoseAnimInstance>());
            TestTrue(TEXT("Face copies the dead body"), Character->ThirdPersonMetaHumanFace->GetAnimInstance()->IsA<UDeathCopyPoseAnimInstance>());
            Refresh(Character);
            const FVector InitialHead = Character->GetMesh()->GetSocketTransform(TEXT("head"), RTS_Component).GetLocation();
            const FVector InitialPelvis = Character->GetMesh()->GetSocketTransform(TEXT("pelvis"), RTS_Component).GetLocation();
            const FVector InitialFoot = Character->GetMesh()->GetSocketTransform(TEXT("foot_l"), RTS_Component).GetLocation();
            const FVector InitialVisibleHead = Character->ThirdPersonMetaHumanBody->GetSocketTransform(TEXT("head"), RTS_Component).GetLocation();
            if (Reaction == EDeathReaction::Front) Render(Character, FString(ClassName)+TEXT("_Start"));
            Character->DeathState.ServerStartTime -= 0.8f;
            Character->UpdateDeathAnimationTime();
            Refresh(Character);
            Render(Character, FString(ClassName)+TEXT("_")+Label+TEXT("_Mid"));
            Character->DeathState.ServerStartTime -= 100.0f;
            Character->UpdateDeathAnimationTime();
            Refresh(Character);
            UAnimSingleNodeInstance* Anim = Character->GetMesh()->GetSingleNodeInstance();
            TestEqual(TEXT("Late presentation goes directly to the final pose"), Anim->GetCurrentTime(), Sequence->GetPlayLength());
            const FVector FinalHead = Character->GetMesh()->GetSocketTransform(TEXT("head"), RTS_Component).GetLocation();
            TestTrue(TEXT("Death visibly changes the head position"), FVector::Dist(FinalHead, InitialHead) > 20.0f);
            TestTrue(TEXT("Visible body also changes its head position"), FVector::Dist(Character->ThirdPersonMetaHumanBody->GetSocketTransform(TEXT("head"), RTS_Component).GetLocation(), InitialVisibleHead) > 20.0f);
            TestTrue(TEXT("Seated pelvis stays fixed"), Character->GetMesh()->GetSocketTransform(TEXT("pelvis"), RTS_Component).GetLocation().Equals(InitialPelvis, 0.2f));
            TestTrue(TEXT("Seated foot stays fixed"), Character->GetMesh()->GetSocketTransform(TEXT("foot_l"), RTS_Component).GetLocation().Equals(InitialFoot, 0.2f));
            TestTrue(TEXT("FP and TP bodies share the death pose"), Character->FirstPersonMetaHumanBody->GetSocketTransform(TEXT("head"), RTS_Component).GetLocation().Equals(
                Character->ThirdPersonMetaHumanBody->GetSocketTransform(TEXT("head"), RTS_Component).GetLocation(), 0.5f));
            TestTrue(TEXT("Local camera remains at its spectator position"), Character->CameraComponent->GetComponentTransform().Equals(CameraBefore, 0.2f));
            PS->SetAliveState(false, EDeathReaction::Front);
            TestEqual(TEXT("Duplicate death notification keeps the original direction"), Character->DeathState.Reaction, Reaction);
            Character->Multicast_PlayGrabGunMontage(EGunHoldReason::Fold);
            Character->Multicast_PutBackGunMontage(EGunHoldReason::Fold);
            if (ALobbyVRCharacter* VR = Cast<ALobbyVRCharacter>(Character)) VR->Multicast_CompleteVRMainRevolverGrab();
            if (ALobbyPCCharacter* Desktop = Cast<ALobbyPCCharacter>(Character)) Desktop->Multicast_CompletePCMainRevolverGrab();
            TestTrue(TEXT("Late grab callbacks cannot replace the death animation"), Character->GetMesh()->GetSingleNodeInstance() == Anim);
            Character->Tick(0.016f);
            Refresh(Character);
            TestTrue(TEXT("Final pose remains after character tick"), Character->GetMesh()->GetSocketTransform(TEXT("head"), RTS_Component).GetLocation().Equals(FinalHead, 0.2f));
            Render(Character, FString(ClassName)+TEXT("_")+Label+TEXT("_End"));
            PS->SetAliveState(true);
            Character->Tick(0.0f);
            TestFalse(TEXT("New game clears the death presentation"), Character->IsDeathPresentationActive());
            TestTrue(TEXT("New game restores the live animation class"), Character->GetMesh()->GetAnimClass() == InitialAnimClass);
            TestTrue(TEXT("New game restores the original FP body"), Character->FirstPersonMetaHumanBody->GetSkeletalMeshAsset() == InitialFPBody);
            TestTrue(TEXT("New game restores the original TP body"), Character->ThirdPersonMetaHumanBody->GetSkeletalMeshAsset() == InitialTPBody);
            TestTrue(TEXT("New game restores the original clothing"), Character->ThirdPersonMetaHumanTorso->GetSkeletalMeshAsset() == InitialTorso);
            TestTrue(TEXT("New game restores the face rig"), Character->ThirdPersonMetaHumanFace->LeaderPoseComponent.Get() == InitialFaceLeader);
            TestTrue(TEXT("New game restores the camera attachment"), Character->CameraComponent->GetAttachParent() == CameraParentBefore);
            TestTrue(TEXT("New game restores the camera offset"), Character->CameraComponent->GetRelativeTransform().Equals(CameraRelativeBefore, 0.1f));
        }
        // Regression: use a real montage-end event dispatched by PostAnimEvaluation.
        // The former immediate mesh refresh recursively entered that evaluation.
        UAnimInstance* LiveAnim = Character->GetMesh()->GetAnimInstance();
        UAnimMontage* FoldMontage = Character->AimMyselfMontage;
        if (!TestNotNull(TEXT("Production fold montage"), FoldMontage)) return false;
        bool bFoldEnded = false;
        PS->BulletArray = {true};
        PS->TotalTriggerCount = 0;
        if (!TestTrue(TEXT("Start production fold montage"), LiveAnim->Montage_Play(FoldMontage) > 0.0f)) return false;
        FOnMontageEnded FoldEnded;
        FoldEnded.BindLambda([&](UAnimMontage*, bool bInterrupted)
        {
            bFoldEnded = true;
            TestFalse(TEXT("Fold montage finishes naturally"), bInterrupted);
            TestTrue(TEXT("Fold callback executes during animation post-evaluation"), Character->GetMesh()->IsPostEvaluatingAnimation());
            TestFalse(TEXT("A live fold bullet eliminates the player inside the montage callback"), PS->ChangeSubRevolver());
            TestFalse(TEXT("Alive state changes immediately in the callback"), PS->isAlive);
            TestTrue(TEXT("The evaluating AnimInstance stays intact until the next actor tick"), Character->GetMesh()->GetAnimInstance() == LiveAnim);
            TestFalse(TEXT("Visual death is deferred outside the montage callback"), Character->bDeathVisualApplied);
        });
        LiveAnim->Montage_SetEndDelegate(FoldEnded, FoldMontage);
        const int32 MaxSteps = FMath::CeilToInt((FoldMontage->GetPlayLength() + 2.0f) / 0.05f);
        for (int32 Step = 0; Step < MaxSteps && !bFoldEnded; ++Step)
        {
            ++GFrameCounter;
            Character->GetMesh()->TickAnimation(0.05f, false);
            Character->GetMesh()->RefreshBoneTransforms();
        }
        if (!TestTrue(TEXT("Production fold montage end callback was dispatched"), bFoldEnded)) return false;
        TestEqual(TEXT("Montage callback selects Fold death"), Character->DeathState.Reaction, EDeathReaction::Fold);
        Character->Tick(0.0f);
        TestTrue(TEXT("The next actor tick applies the deferred fold death"), Character->bDeathVisualApplied);
        TestNotNull(TEXT("Fold death starts after animation post-evaluation finishes"), Character->GetMesh()->GetSingleNodeInstance());
        PS->SetAliveState(true);
        Character->Tick(0.0f);

        // A new alive state before the queued death is applied must cancel its visuals.
        PS->BulletArray = {true};
        PS->TotalTriggerCount = 0;
        TestFalse(TEXT("A live sub-revolver bullet eliminates the folding player"), PS->ChangeSubRevolver());
        TestEqual(TEXT("The actual fold death path selects Fold"), Character->DeathState.Reaction, EDeathReaction::Fold);
        PS->SetAliveState(true);
        Character->Tick(0.0f);
        TestFalse(TEXT("Revival before the pending tick does not apply a stale death"), Character->bDeathVisualApplied);
        TestTrue(TEXT("Cancelled death keeps the live animation class"), Character->GetMesh()->GetAnimClass() == InitialAnimClass);
        PS->SetFoldState(false);
        // A fresh replication notification uses the same final pose without replaying.
        Character->DeathState.Reaction = EDeathReaction::Left;
        Character->DeathState.ServerStartTime = -100.0f;
        Character->OnRep_DeathState();
        Character->Tick(0.0f);
        TestEqual(TEXT("Replicated late death is positioned at the end"), Character->GetMesh()->GetSingleNodeInstance()->GetCurrentTime(), Character->LeftDeathAnimation->GetPlayLength());
        Character->SetDeathPresentation(EDeathReaction::None);
        Character->Tick(0.0f);
        PC->UnPossess();
        Character->Destroy();
    }
    return true;
}
#endif
