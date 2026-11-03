#include "GameInstanceSubsystem/IndianBabGameInstance.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/NetDriver.h"
#include "GameFramework/Pawn.h"
#include "GameInstanceSubsystem/ConnectivitySubsystem.h"
#include "GameInstanceSubsystem/SessionSubsystem.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "GameInstanceSubsystem/SettingSubsystem.h"
#include "IXRTrackingSystem.h"
#include "IHeadMountedDisplay.h"
#include "ISpectatorScreenController.h"
#include "UObject/UObjectGlobals.h"

DEFINE_LOG_CATEGORY_STATIC(LogIndianBabGI, Log, All);

namespace
{
    FString NetworkFailureToString(ENetworkFailure::Type Type)
    {
        switch (Type)
        {
        case ENetworkFailure::NetDriverAlreadyExists:    return TEXT("NetDriverAlreadyExists");
        case ENetworkFailure::NetDriverCreateFailure:    return TEXT("NetDriverCreateFailure");
        case ENetworkFailure::NetDriverListenFailure:    return TEXT("NetDriverListenFailure");
        case ENetworkFailure::ConnectionLost:            return TEXT("ConnectionLost");
        case ENetworkFailure::ConnectionTimeout:         return TEXT("ConnectionTimeout");
        case ENetworkFailure::FailureReceived:           return TEXT("FailureReceived");
        case ENetworkFailure::OutdatedClient:            return TEXT("OutdatedClient");
        case ENetworkFailure::OutdatedServer:            return TEXT("OutdatedServer");
        case ENetworkFailure::PendingConnectionFailure:  return TEXT("PendingConnectionFailure");
        case ENetworkFailure::NetGuidMismatch:           return TEXT("NetGuidMismatch");
        case ENetworkFailure::NetChecksumMismatch:       return TEXT("NetChecksumMismatch");
        default:                                          return TEXT("Unknown");
        }
    }

    FString TravelFailureToString(ETravelFailure::Type Type)
    {
        switch (Type)
        {
        case ETravelFailure::NoLevel:                    return TEXT("NoLevel");
        case ETravelFailure::LoadMapFailure:             return TEXT("LoadMapFailure");
        case ETravelFailure::InvalidURL:                 return TEXT("InvalidURL");
        case ETravelFailure::PackageMissing:             return TEXT("PackageMissing");
        case ETravelFailure::PackageVersion:             return TEXT("PackageVersion");
        case ETravelFailure::NoDownload:                 return TEXT("NoDownload");
        case ETravelFailure::TravelFailure:              return TEXT("TravelFailure");
        case ETravelFailure::CheatCommands:              return TEXT("CheatCommands");
        case ETravelFailure::PendingNetGameCreateFailure:return TEXT("PendingNetGameCreateFailure");
        case ETravelFailure::CloudSaveFailure:           return TEXT("CloudSaveFailure");
        case ETravelFailure::ServerTravelFailure:        return TEXT("ServerTravelFailure");
        case ETravelFailure::ClientTravelFailure:        return TEXT("ClientTravelFailure");
        default:                                          return TEXT("Unknown");
        }
    }
}

// 기존 캐릭터 BP를 두 모드의 기본 클래스로 재사용합니다.
UIndianBabGameInstance::UIndianBabGameInstance()
{
    // 초기 모듈 등록 중 Anim BP가 로드되지 않도록 실제 사용 시에만 불러옵니다.
    PCCharacterClass = TSoftClassPtr<APawn>(FSoftObjectPath(TEXT("/Game/Blueprint/Character/BP_LobbyCharacterBase_PC.BP_LobbyCharacterBase_PC_C")));
    VRCharacterClass = TSoftClassPtr<APawn>(FSoftObjectPath(TEXT("/Game/Blueprint/Character/BP_LobbyCharacterBase_VR.BP_LobbyCharacterBase_VR_C")));
}

// 기기 연결 여부가 아니라 사용자가 확정한 모드를 보관합니다.
void UIndianBabGameInstance::SetSelectedPlayMode(bool bUseVR)
{
    bPlayModeSelected = true;
    bUseVRPlayMode = bUseVR;
}

// 서버에서도 동일한 BP 목록으로 플레이어별 Pawn을 선택합니다.
TSubclassOf<APawn> UIndianBabGameInstance::GetPlayModePawnClass(bool bUseVR) const
{
    return bUseVR ? VRCharacterClass.LoadSynchronous() : PCCharacterClass.LoadSynchronous();
}

// =====================================
// BGM 재생 시스템
// =====================================

void UIndianBabGameInstance::PlayMainBGM()
{
    EndFoldAudio(false);
    StopMainRevolverClock();
    PlayBGM(MainBGMSound.Get(), EBGMTrack::Main);
}

void UIndianBabGameInstance::PlayGameBGM()
{
    PlayBGM(GameBGMSound.Get(), EBGMTrack::Game);
}

void UIndianBabGameInstance::PlayVictoryBGM()
{
    EndFoldAudio(false);
    StopMainRevolverClock();
    PlayBGM(VictoryBGMSound.Get(), EBGMTrack::Victory);
}

void UIndianBabGameInstance::PlayBGM(
    USoundBase* Sound,
    EBGMTrack NewTrack)
{
    UWorld* World = GetWorld();

    // 월드가 없거나 데디케이티드 서버이면 재생하지 않음
    if (!World || World->GetNetMode() == NM_DedicatedServer)
    {
        return;
    }

    if (!Sound)
    {
        UE_LOG(
            LogIndianBabGI,
            Warning,
            TEXT("[BGM] Sound asset is not assigned.")
        );
        return;
    }

    // 같은 음악이 재생 중이면 다시 시작하지 않음
    if (ActiveTrack == NewTrack &&
        IsValid(ActiveBGM) &&
        ActiveBGM->IsPlaying())
    {
        return;
    }

    // 기존 음악 페이드 아웃
    if (IsValid(ActiveBGM))
    {
        ActiveBGM->FadeOut(0.75f, 0.0f);
    }

    // 2D 음악 생성
    // 맵을 이동해도 음악이 유지되도록 설정
    UAudioComponent* NewAudio =
        UGameplayStatics::CreateSound2D(
            World,
            Sound,
            1.0f,
            1.0f,
            0.0f,
            nullptr,
            true,   // bPersistAcrossLevelTransition
            true    // bAutoDestroy
        );

    if (!IsValid(NewAudio))
    {
        ActiveBGM = nullptr;
        ActiveTrack = EBGMTrack::None;
        return;
    }

    ActiveBGM = NewAudio;
    ActiveTrack = NewTrack;

    // 저장된 마스터 볼륨 읽기
    float Volume = 1.0f;

    if (USettingSubsystem* Settings =
        GetSubsystem<USettingSubsystem>())
    {
        Volume = Settings->GetMasterVolume();
    }

    Volume = FMath::Clamp(Volume, 0.0f, 1.0f);

    // 새 음악 페이드 인
    ActiveBGM->FadeIn(0.75f, Volume);

    UE_LOG(
        LogIndianBabGI,
        Log,
        TEXT("[BGM] Playing: %s"),
        *GetNameSafe(Sound)
    );
}

void UIndianBabGameInstance::SetBGMVolume(float NewVolume)
{
    const float Volume =
        FMath::Clamp(NewVolume, 0.0f, 1.0f);

    // 메인 리볼버 시계 소리에도 볼륨 설정 적용
    if (IsValid(MainRevolverClockAudio))
    {
        MainRevolverClockAudio->AdjustVolume(0.1f, Volume);
    }

    // 폴드 중이면 게임 BGM을 계속 0으로 유지
    if (bFoldAudioActive)
    {
        if (IsValid(ActiveBGM) &&
            ActiveTrack == EBGMTrack::Game)
        {
            ActiveBGM->AdjustVolume(0.1f, 0.0f);
        }

        if (IsValid(FoldHeartbeatAudio))
        {
            FoldHeartbeatAudio->AdjustVolume(0.1f, Volume);
        }

        return;
    }

    // 평상시 BGM 음량 조절
    if (IsValid(ActiveBGM))
    {
        ActiveBGM->AdjustVolume(0.1f, Volume);
    }
}

void UIndianBabGameInstance::BeginFoldAudio(int32 FoldCount)
{
    UWorld* World = GetWorld();

    // 데디케이티드 서버에서는 오디오 재생하지 않음
    if (!World || World->GetNetMode() == NM_DedicatedServer)
    {
        return;
    }

    // 중복 실행 방지
    if (bFoldAudioActive)
    {
        return;
    }

    // 1~3번째는 slow, 4번째 이후는 fast
    USoundBase* SelectedSound =
        (FoldCount <= 3)
        ? SlowHeartbeatSound.Get()
        : FastHeartbeatSound.Get();

    if (!IsValid(SelectedSound))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[FoldAudio] Heartbeat sound is not assigned."));
        return;
    }

    // 이전 심장소리 컴포넌트 정리
    if (IsValid(FoldHeartbeatAudio))
    {
        FoldHeartbeatAudio->Stop();
        FoldHeartbeatAudio = nullptr;
    }

    // 사용자 볼륨 설정
    float Volume = 1.0f;

    if (USettingSubsystem* Settings =
        GetSubsystem<USettingSubsystem>())
    {
        Volume = Settings->GetMasterVolume();
    }

    Volume = FMath::Clamp(Volume, 0.0f, 1.0f);

    // 심장소리는 폴드한 본인에게만 재생
    // (호출은 해당 플레이어의 Client RPC에서 수행)
    UAudioComponent* NewHeartbeat =
        UGameplayStatics::CreateSound2D(
            World,
            SelectedSound,
            1.0f,
            1.0f,
            0.0f,
            nullptr,
            false,
            true
        );

    if (!IsValid(NewHeartbeat))
    {
        return;
    }

    FoldHeartbeatAudio = NewHeartbeat;
    bFoldAudioActive = true;

    // 기존 게임 BGM은 정지하지 않고 소리만 제거
    if (IsValid(ActiveBGM) &&
        ActiveTrack == EBGMTrack::Game)
    {
        ActiveBGM->AdjustVolume(0.2f, 0.0f);
    }

    // 심장소리 시작
    FoldHeartbeatAudio->FadeIn(0.2f, Volume);

    UE_LOG(LogTemp, Log,
        TEXT("[FoldAudio] FoldCount=%d, Heartbeat=%s"),
        FoldCount,
        *GetNameSafe(SelectedSound));
}

void UIndianBabGameInstance::EndFoldAudio(bool bSurvived)
{
    UWorld* World = GetWorld();

    if (!World || World->GetNetMode() == NM_DedicatedServer)
    {
        return;
    }

    const bool bWasFoldAudioActive = bFoldAudioActive;

    bFoldAudioActive = false;

    // 심장소리 종료
    if (IsValid(FoldHeartbeatAudio))
    {
        FoldHeartbeatAudio->Stop();
        FoldHeartbeatAudio = nullptr;
    }

    // 사용자 볼륨 설정
    float Volume = 1.0f;

    if (USettingSubsystem* Settings =
        GetSubsystem<USettingSubsystem>())
    {
        Volume = Settings->GetMasterVolume();
    }

    Volume = FMath::Clamp(Volume, 0.0f, 1.0f);

    // 게임 BGM 볼륨 복원
    // 기존 재생 위치는 그대로 유지
    if (bWasFoldAudioActive &&
        IsValid(ActiveBGM) &&
        ActiveTrack == EBGMTrack::Game)
    {
        ActiveBGM->AdjustVolume(0.25f, Volume);
    }

    // 사망했거나 애니메이션이 취소되었다면 종료
    if (!bSurvived)
    {
        return;
    }

    // 생존했다면 sigh1 재생
    if (IsValid(SighSound))
    {
        UGameplayStatics::PlaySound2D(
            World,
            SighSound.Get(),
            Volume
        );
    }

    UE_LOG(LogTemp, Log,
        TEXT("[FoldAudio] Ended. Survived=%s"),
        bSurvived ? TEXT("true") : TEXT("false"));
}

void UIndianBabGameInstance::Init()
{
    Super::Init();

    if (GEngine)
    {
        NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(
            this, &UIndianBabGameInstance::OnNetworkFailure);
        TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(
            this, &UIndianBabGameInstance::OnTravelFailure);

        UE_LOG(LogIndianBabGI, Log,
            TEXT("[GI] NetworkFailure/TravelFailure 핸들러 바인딩 완료."));
    }

    // 맵 로드(메인메뉴 / 데디 ClientTravel 후 인게임 / ServerTravel)마다
    // Spectator Screen Mode 재적용. HMD 초기화가 GameInstance::Init 보다
    // 나중일 수 있어 PostLoadMapWithWorld 시점으로 잡음.
    PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
        this, &UIndianBabGameInstance::OnPostLoadMap);
}

void UIndianBabGameInstance::Shutdown()
{
    StopMainRevolverClock();

    if (IsValid(FoldHeartbeatAudio))
    {
        FoldHeartbeatAudio->Stop();
        FoldHeartbeatAudio = nullptr;
    }

    bFoldAudioActive = false;

    if (IsValid(ActiveBGM))
    {
        ActiveBGM->Stop();
    }

    ActiveBGM = nullptr;
    ActiveTrack = EBGMTrack::None;

    if (GEngine)
    {
        GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
        GEngine->OnTravelFailure().Remove(TravelFailureHandle);
    }

    FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);

    Super::Shutdown();
}

void UIndianBabGameInstance::OnPostLoadMap(UWorld* /*LoadedWorld*/)
{
    ApplyVRSpectatorScreenMode();
}

void UIndianBabGameInstance::ApplyVRSpectatorScreenMode()
{
    if (IsRunningDedicatedServer())
    {
        return;
    }
    if (!GEngine || !GEngine->XRSystem.IsValid())
    {
        return;
    }

    IHeadMountedDisplay* HMD = GEngine->XRSystem->GetHMDDevice();
    if (!HMD)
    {
        return;
    }

    ISpectatorScreenController* Spectator = HMD->GetSpectatorScreenController();
    if (!Spectator)
    {
        return;
    }

    // SingleEyeLetterboxed: 한쪽 눈 화면 전체를 비율 유지하며 표시.
    // 기본값 SingleEyeCroppedToFill 은 모니터 비율에 맞춰 가장자리를 잘라내서
    // 데모 시연 시 UI/씬 가장자리가 잘림 → 데모 가시성 우선으로 letterboxed 채택.
    Spectator->SetSpectatorScreenMode(ESpectatorScreenMode::SingleEyeLetterboxed);
    UE_LOG(LogIndianBabGI, Log,
        TEXT("[GI] VR Spectator Screen Mode → SingleEyeLetterboxed"));
}

void UIndianBabGameInstance::OnNetworkFailure(UWorld* /*World*/, UNetDriver* /*NetDriver*/,
    ENetworkFailure::Type FailureType, const FString& ErrorString)
{
    const FString TypeStr = NetworkFailureToString(FailureType);
    UE_LOG(LogIndianBabGI, Error,
        TEXT("[GI] NetworkFailure: type=%s err=%s"), *TypeStr, *ErrorString);

    // 데디 PreLogin 거부 분기:
    //   - PendingConnectionFailure: ClientTravel 후 PendingNetGame 단계에서 NMT_Failure 수신 (만석/AC 미검증/게임 진행 중)
    //   - FailureReceived: 이미 접속한 상태에서 서버가 NMT_Failure 발사 (예외 케이스)
    // 두 경우 모두 ErrorString이 데디 PreLogin의 ErrorMessage 그대로
    // → 인터넷 단절(NLA/ConnectionLost/Timeout)과 구분해 모달 채널로 분리.
    const bool bIsRejection =
        (FailureType == ENetworkFailure::PendingConnectionFailure) ||
        (FailureType == ENetworkFailure::FailureReceived);

    if (bIsRejection && !ErrorString.IsEmpty())
    {
        UE_LOG(LogIndianBabGI, Warning,
            TEXT("[GI] Dedi rejection — reason=%s"), *ErrorString);

        // 호스트/클라 모두 Steam Lobby 정리 + OnSessionErrorEvent broadcast가 CleanupHostSession에서 일괄 처리됨.
        // (호스트: 매치메이커 인스턴스/Lobby 폐기, 클라: 호스트 Lobby에서 leave → 슬롯 즉시 반환)
        if (USessionSubsystem* Session = GetSubsystem<USessionSubsystem>())
        {
            Session->CleanupHostSession(ErrorString);
        }
        return;
    }

    // 활성 NamedSession 보유 = 우리가 데디로 트래블 시도(중)인 흐름.
    // ConnectionTimeout/ConnectionLost/PendingConnectionFailure(ErrorString 빈 케이스 포함)는
    // 데디 미응답/사망이지 인터넷 단절이 아니므로 세션 오류 채널로 라우팅.
    USessionSubsystem* Session = GetSubsystem<USessionSubsystem>();
    const bool bDediUnreachable =
        FailureType == ENetworkFailure::ConnectionTimeout ||
        FailureType == ENetworkFailure::ConnectionLost ||
        FailureType == ENetworkFailure::PendingConnectionFailure;

    if (Session && Session->IsInActiveSession() && bDediUnreachable)
    {
        // ClientWasKicked 가 stash 한 사유가 있으면 그걸 우선 — generic 문구 대체.
        // KickPlayer → ClientWasKicked RPC(reliable) 가 close 직전에 도착하므로
        // OnNetworkFailure 시점에 보통 채워져 있음. 비어 있으면(타이밍 미스 / 일반 단절)
        // 기존 generic fallback.
        FString KickReason = ConsumePendingKickReason();
        const FString FinalReason = KickReason.IsEmpty()
            ? FString(TEXT("게임 서버에 연결할 수 없습니다. 잠시 후 다시 시도해주세요."))
            : KickReason;
        UE_LOG(LogIndianBabGI, Warning,
            TEXT("[GI] Dedi unreachable (NetworkFailure:%s) — reason=%s"),
            *TypeStr, *FinalReason);
        Session->CleanupHostSession(FinalReason);
        return;
    }

    // 진짜 단절(NLA 사각지대 백스톱 포함) — 기존 경로
    if (UConnectivitySubsystem* Conn = GetSubsystem<UConnectivitySubsystem>())
    {
        Conn->ForceTriggerLost(FString::Printf(TEXT("NetworkFailure:%s"), *TypeStr));
    }
}

void UIndianBabGameInstance::OnTravelFailure(UWorld* /*World*/,
    ETravelFailure::Type FailureType, const FString& ErrorString)
{
    const FString TypeStr = TravelFailureToString(FailureType);
    UE_LOG(LogIndianBabGI, Error,
        TEXT("[GI] TravelFailure: type=%s err=%s"), *TypeStr, *ErrorString);

    // 활성 NamedSession 보유 = 데디 트래블 시도 중. PendingNetGameCreateFailure/TravelFailure 등
    // 어떤 사유든 "데디 응답 없음/트래블 실패"로 통일해 세션 오류 채널로 라우팅 +
    // 좀비 Steam Lobby 청소까지 CleanupHostSession이 일괄 처리.
    if (USessionSubsystem* Session = GetSubsystem<USessionSubsystem>())
    {
        if (Session->IsInActiveSession())
        {
            const FString Reason = ErrorString.IsEmpty()
                ? FString(TEXT("게임 서버에 연결할 수 없습니다. 잠시 후 다시 시도해주세요."))
                : ErrorString;
            UE_LOG(LogIndianBabGI, Warning,
                TEXT("[GI] TravelFailure during active session — routing to session error: %s"), *Reason);
            Session->CleanupHostSession(Reason);
            return;
        }
    }

    if (UConnectivitySubsystem* Conn = GetSubsystem<UConnectivitySubsystem>())
    {
        Conn->ForceTriggerLost(FString::Printf(TEXT("TravelFailure:%s"), *TypeStr));
    }
}

void UIndianBabGameInstance::StartMainRevolverClock()
{
    UWorld* World = GetWorld();

    if (!World || World->GetNetMode() == NM_DedicatedServer)
    {
        return;
    }

    if (!IsValid(MainRevolverClockSound))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[MainRevolverClock] Sound is not assigned."));
        return;
    }

    // 시계 소리 중복 재생 방지
    if (IsValid(MainRevolverClockAudio) &&
        MainRevolverClockAudio->IsPlaying())
    {
        return;
    }

    float Volume = 1.0f;

    if (USettingSubsystem* Settings =
        GetSubsystem<USettingSubsystem>())
    {
        Volume = Settings->GetMasterVolume();
    }

    Volume = FMath::Clamp(Volume, 0.0f, 1.0f);

    MainRevolverClockAudio =
        UGameplayStatics::CreateSound2D(
            World,
            MainRevolverClockSound.Get(),
            1.0f,
            1.0f,
            0.0f,
            nullptr,
            false,
            true
        );

    if (IsValid(MainRevolverClockAudio))
    {
        MainRevolverClockAudio->FadeIn(0.2f, Volume);
    }
}

void UIndianBabGameInstance::StopMainRevolverClock()
{
    if (IsValid(MainRevolverClockAudio))
    {
        MainRevolverClockAudio->FadeOut(0.2f, 0.0f);
        MainRevolverClockAudio = nullptr;
    }
}