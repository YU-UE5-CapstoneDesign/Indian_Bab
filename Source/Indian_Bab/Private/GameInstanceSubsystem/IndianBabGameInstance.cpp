#include "GameInstanceSubsystem/IndianBabGameInstance.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/NetDriver.h"
#include "GameFramework/Pawn.h"
#include "GameInstanceSubsystem/ConnectivitySubsystem.h"
#include "GameInstanceSubsystem/SessionSubsystem.h"

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
}

void UIndianBabGameInstance::Shutdown()
{
    if (GEngine)
    {
        GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
        GEngine->OnTravelFailure().Remove(TravelFailureHandle);
    }

    Super::Shutdown();
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
