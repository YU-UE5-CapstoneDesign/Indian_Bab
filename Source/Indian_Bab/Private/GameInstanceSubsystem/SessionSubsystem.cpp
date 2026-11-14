#include "GameInstanceSubsystem/SessionSubsystem.h"

#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "Online/OnlineSessionNames.h"
#include "Engine/LocalPlayer.h"
#include "Network/NetworkEndpoints.h"
#include "GameInstanceSubsystem/SteamCredentialsSubsystem.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "GameFramework/PlayerController.h"


DEFINE_LOG_CATEGORY(LogSessionSubsystem);

const FName USessionSubsystem::Key_MatchId = FName("MatchId");
const FName USessionSubsystem::Key_GameStarted = FName("GameStarted");
const FName USessionSubsystem::Key_GameTag = FName("INDIANBABID");
const int32 USessionSubsystem::GameTagMagic = 0x1B7A;  // 'IndianBab' magic — Steam pool 충돌 회피용 임의값


bool USessionSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    // 데디는 Steam OSS를 쓰지 않음 (MM이 Python에서 ISteamUserAuth 직접 호출). 데디 GameInstance에 미생성.
    if (IsRunningDedicatedServer())
    {
        return false;
    }
    return Super::ShouldCreateSubsystem(Outer);
}


void USessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    // Steam 부팅 가드(OSS 로드 확인 / Runtime의 STEAM 강제 / 로그인 검증)는 SteamCredentialsSubsystem
    Collection.InitializeDependency(USteamCredentialsSubsystem::StaticClass());
    Super::Initialize(Collection);

    // 여기 도달 시점엔 SteamCreds가 Runtime이면 STEAM 확정, Editor이면 NULL OSS 확정 상태.
    // 양쪽 다 OSS::Get()은 non-null이어야 정상 — SessionInterface stub을 NULL OSS도 제공하므로
    // 이름 분기 없이 동일 경로로 캐시. 무효 시 CreateRoom 등이 IsValid() 가드로 거부.
    IOnlineSubsystem* OSS = IOnlineSubsystem::Get();
    if (OSS == nullptr)
    {
        UE_LOG(LogSessionSubsystem, Error,
            TEXT("OSS::Get() == null"));
        return;
    }

    SessionInterface = OSS->GetSessionInterface();
    if (!SessionInterface.IsValid())
    {
        UE_LOG(LogSessionSubsystem, Error,
            TEXT("SessionInterface 무효 (OSS=%s) — CreateRoom 등 호출 시 즉시 실패 브로드캐스트."),
            *OSS->GetSubsystemName().ToString());
        return;
    }

    UE_LOG(LogSessionSubsystem, Log,
        TEXT("SessionInterface 확보 완료 (OSS=%s)."), *OSS->GetSubsystemName().ToString());
}


void USessionSubsystem::Deinitialize()
{
    FinishFindRequest();
    ResetJoinAttempt();
    FTSTicker::GetCoreTicker().RemoveTicker(CleanupTimeoutHandle);
    CleanupTimeoutHandle.Reset();
    bCleanupInProgress = false;
    if (SessionInterface.IsValid())
    {
        SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
        SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
        SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(ReturnToMainMenuDestroySessionCompleteDelegateHandle);
    }
    SessionInterface.Reset();
    Super::Deinitialize();
}


// ---------------------------------------------------------
// 방 생성
// ---------------------------------------------------------
void USessionSubsystem::CreateRoom(int32 MaxPlayers, ERoomVisibility Visibility)
{
    if (bFindingRooms || IsJoiningRoom() || bCleanupInProgress || !PendingReturnToMainMenuMapPath.IsEmpty())
    {
        OnSessionErrorEvent.Broadcast(TEXT("이전 세션 작업을 정리 중입니다. 잠시 후 다시 시도해주세요."));
        return;
    }
    if (!SessionInterface.IsValid())
    {
        OnSessionErrorEvent.Broadcast(TEXT("Steam 세션 인터페이스를 사용할 수 없습니다."));
        return;
    }

    // 이미 세션이 있다면 파괴 후 재생성 (안전장치)
    auto ExistingSession = SessionInterface->GetNamedSession(NAME_GameSession);
    if (ExistingSession != nullptr)
    {
        SessionInterface->DestroySession(NAME_GameSession);
        bIsLocalHost = false;
    }

    LastVisibility = Visibility;
    LastMaxPlayers = MaxPlayers;

    CreateSessionCompleteDelegateHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(
        FOnCreateSessionCompleteDelegate::CreateUObject(this, &USessionSubsystem::OnCreateSessionComplete));

    // 공통 설정 — Steam 로비 기반
    // 주의: Steam OSS의 BuildLobbyType은 bShouldAdvertise=false면 무조건 k_ELobbyTypePrivate 상태
    //       → Public/FriendsOnly 둘 다 bShouldAdvertise=true 유지, 구분은 PresenceFriendsOnly 플래그로
    FOnlineSessionSettings SessionSettings;
    SessionSettings.bIsLANMatch = false;
    SessionSettings.bUsesPresence = true;
    SessionSettings.bUseLobbiesIfAvailable = true;
    // 진행 중 합류는 허용 — 옛 작동 코드와 정합. 게임 시작 후 차단은 LockSessionForInGame() + dedi PreLogin 백스톱이 담당.
    SessionSettings.bAllowJoinInProgress = true;
    SessionSettings.bAllowInvites = true;
    SessionSettings.bShouldAdvertise = true;
    SessionSettings.NumPublicConnections = MaxPlayers;

    // 가시성 분기 — Public: 누구나 검색/합류 / FriendsOnly: Steam 친구만 (k_ELobbyTypeFriendsOnly 매핑)
    const bool bPublic = (Visibility == ERoomVisibility::Public);
    SessionSettings.bAllowJoinViaPresence = bPublic;
    SessionSettings.bAllowJoinViaPresenceFriendsOnly = !bPublic;

    SessionSettings.Set(Key_GameStarted, (int32)0, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
    SessionSettings.Set(Key_GameTag, GameTagMagic, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

    const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
    if (LocalPlayer)
    {
        UE_LOG(LogSessionSubsystem, Warning, TEXT("Creating Room: Visibility=%s, MaxPlayers=%d"),
            bPublic ? TEXT("Public") : TEXT("FriendsOnly"), MaxPlayers);
        SessionInterface->CreateSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, SessionSettings);
    }
}


void USessionSubsystem::OnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
    // 델리게이트 해제 (메모리 관리)
    if (SessionInterface.IsValid())
    {
        SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
    }

    if (!bWasSuccessful)
    {
        UE_LOG(LogSessionSubsystem, Error, TEXT("Failed to Create Session."));
        bIsLocalHost = false;
        OnSessionErrorEvent.Broadcast(TEXT("Steam 세션 생성에 실패했습니다."));
        return;
    }

    bIsLocalHost = true;
    UE_LOG(LogSessionSubsystem, Warning, TEXT("Steam Session OK. Requesting matchmaker..."));
    RequestMatchmakerCreateInstance();
}


// ---------------------------------------------------------
// 매치메이커 흐름 (호스트)
// ---------------------------------------------------------
void USessionSubsystem::RequestMatchmakerCreateInstance()
{
    #if !MATCHMAKER_USE_MOCK
    USteamCredentialsSubsystem* SteamCreds =
        GetGameInstance() ? GetGameInstance()->GetSubsystem<USteamCredentialsSubsystem>() : nullptr;
    if (!SteamCreds)
    {
        UE_LOG(LogSessionSubsystem, Error, TEXT("[SessionSubsystem] SteamCredentialsSubsystem 없음 — /create 중단."));
        CleanupHostSession(TEXT("Steam 인증 정보를 가져올 수 없습니다. Steam 로그인 상태를 확인해주세요."));
        return;
    }

    TWeakObjectPtr<USessionSubsystem> WeakThis(this);
    const int32 MaxPlayersToSend = LastMaxPlayers;
    SteamCreds->RequestTicket(
        [WeakThis, MaxPlayersToSend](bool bOk, FString SteamTicketHex, FString HostSteamId)
        {
            if (!WeakThis.IsValid()) return;
            if (!bOk)
            {
                UE_LOG(LogSessionSubsystem, Error, TEXT("[SessionSubsystem] Steam credentials 획득 실패 — /create 중단."));
                WeakThis->CleanupHostSession(TEXT("Steam 인증 정보를 가져올 수 없습니다. Steam 로그인 상태를 확인해주세요."));
                return;
            }

            // CreateRoom 호출 시 받은 MaxPlayers를 그대로 MM에 통과.
            // MM은 받은 값을 dedi에 -MaxPlayers=N CLI로 주입, dedi MainGameMode가 PreLogin 게이트에서 사용.
            const FString Body = FString::Printf(
                TEXT("{\"host_steam_id\":\"%s\",\"steam_session_ticket\":\"%s\",\"max_players\":%d}"),
                *HostSteamId, *SteamTicketHex, MaxPlayersToSend);

            auto Req = FHttpModule::Get().CreateRequest();
            Req->SetURL(NetworkEndpoints::MM::External::CreateMatch());
            Req->SetVerb(TEXT("POST"));
            Req->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
            Req->SetContentAsString(Body);
            // 매치메이커 다운/방화벽 차단 시 무한 대기 방지 — 15s 후 bOk=false로 콜백
            Req->SetTimeout(15.0f);
            Req->OnProcessRequestComplete().BindLambda(
                [WeakThis](FHttpRequestPtr, FHttpResponsePtr Resp, bool bHttpOk)
                {
                    if (!WeakThis.IsValid()) return;
                    WeakThis->HandleMatchmakerResponse(
                        Resp.IsValid() ? Resp->GetContentAsString() : FString(),
                        bHttpOk && Resp.IsValid() && Resp->GetResponseCode() == 200);
                });
            Req->ProcessRequest();
        });
    return;
    #else
    const FString MockResponse = TEXT(R"({"match_id":"TEST_MATCH_001","server_ip":"127.0.0.1","port":7777})");
    HandleMatchmakerResponse(MockResponse, true);
    #endif
}


void USessionSubsystem::HandleMatchmakerResponse(const FString& JsonBody, bool bOk)
{
    if (!bOk)
    {
        UE_LOG(LogSessionSubsystem, Error, TEXT("Matchmaker request failed (network/timeout/non-200). body=%s"), *JsonBody);
        CleanupHostSession(TEXT("매치메이커 서버에 연결할 수 없습니다. 잠시 후 다시 시도해주세요."));
        return;
    }

    TSharedPtr<FJsonObject> Json;
    auto Reader = TJsonReaderFactory<>::Create(JsonBody);
    if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid())
    {
        UE_LOG(LogSessionSubsystem, Error, TEXT("Matchmaker response parse failed: %s"), *JsonBody);
        CleanupHostSession(TEXT("매치메이커 응답 형식이 올바르지 않습니다."));
        return;
    }

    // 필수 필드 누락(데디 풀 고갈 등 매치메이커 에러 응답) 방어 — GetStringField는 누락 시 빈 문자열
    FString ServerIp;
    FString MatchId;
    int32 Port = 0;
    if (!Json->TryGetStringField(TEXT("server_ip"), ServerIp) ||
        !Json->TryGetNumberField(TEXT("port"), Port) ||
        !Json->TryGetStringField(TEXT("match_id"), MatchId) ||
        ServerIp.IsEmpty() || Port <= 0 || MatchId.IsEmpty())
    {
        UE_LOG(LogSessionSubsystem, Error, TEXT("Matchmaker response missing match_id/server_ip/port. body=%s"), *JsonBody);
        CleanupHostSession(TEXT("사용 가능한 게임 서버가 없습니다. 잠시 후 다시 시도해주세요."));
        return;
    }

    const FString DediURL = FString::Printf(TEXT("%s:%d"), *ServerIp, Port);

    UE_LOG(LogSessionSubsystem, Warning, TEXT("Matchmaker response → MatchId=%s Dedi=%s"), *MatchId, *DediURL);
    FinalizeHostTravel(DediURL, MatchId);
}


// ---------------------------------------------------------
// 매치메이커/데디 거부 공통 정리 (호스트·클라 통합)
// ---------------------------------------------------------
// IOnlineSession::DestroySession은 NamedSession 소유권에 따라 Steam OSS 내부에서 자동 분기:
//   - 호스트(Lobby Owner): FOnlineAsyncTaskSteamDestroyLobby → 로비 해체 + 멤버 전원 강제 이탈
//   - 클라(Lobby Member): FOnlineAsyncTaskSteamLeaveLobby → SteamMatchmaking()->LeaveLobby()
//     → 호스트가 LobbyChatUpdate 콜백 수신 → NumOpenPublicConnections 즉시 회복
// 따라서 단일 DestroySession 호출이 양쪽 시나리오 커버. 분기 코드 불필요(중복).
void USessionSubsystem::CleanupHostSession(const FString& Reason)
{
    if (JoinFlow == EJoinFlow::DestroyingPrevious || JoinFlow == EJoinFlow::Joining)
    {
        if (!bJoinTimedOut)
        {
            bJoinTimedOut = true;
            ++JoinRequestId;
            FTSTicker::GetCoreTicker().RemoveTicker(JoinTimeoutHandle);
            JoinTimeoutHandle.Reset();
            OnJoinSessionCompleteEvent.Broadcast(false);
        }
        if (!Reason.IsEmpty()) OnSessionErrorEvent.Broadcast(Reason);
        return;
    }
    if (IsJoiningRoom())
    {
        ResetJoinAttempt();
        OnJoinSessionCompleteEvent.Broadcast(false);
    }
    if (bCleanupInProgress) return;
    const bool bWasHost = bIsLocalHost;
    const bool bHasNamedSession = SessionInterface.IsValid() &&
        SessionInterface->GetNamedSession(NAME_GameSession) != nullptr;

    UE_LOG(LogSessionSubsystem, Warning,
        TEXT("CleanupHostSession: role=%s hasNamedSession=%d reason=%s"),
        bWasHost ? TEXT("HOST (destroying lobby)")
                 : (bHasNamedSession ? TEXT("CLIENT (leaving host lobby)") : TEXT("none")),
        bHasNamedSession ? 1 : 0, *Reason);

    if (bHasNamedSession)
    {
        PendingCleanupReason = Reason;
        bCleanupInProgress = true;
        CleanupTimeoutHandle = FTSTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateWeakLambda(this, [this](float)
            {
                CleanupTimeoutHandle.Reset();
                const FString Error = PendingCleanupReason.IsEmpty()
                    ? TEXT("세션 정리 응답이 지연되고 있습니다. 잠시 후 다시 시도해주세요.") : PendingCleanupReason;
                PendingCleanupReason.Empty();
                OnSessionErrorEvent.Broadcast(Error);
                return false;
            }), 25.0f);

        SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
        DestroySessionCompleteDelegateHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(
            FOnDestroySessionCompleteDelegate::CreateUObject(this, &USessionSubsystem::OnDestroySessionAfterCleanup));
        // 호스트: 로비 해체 / 클라: LeaveLobby (OSS 내부 분기)
        if (!SessionInterface->DestroySession(NAME_GameSession) && bCleanupInProgress)
            OnDestroySessionAfterCleanup(NAME_GameSession, false);
        return;
    }

    // 정리할 세션이 없는 경우(CreateSession 자체 실패 직후 / 클라가 Join 전 거부 등) 즉시 broadcast
    bIsLocalHost = false;
    if (!Reason.IsEmpty()) OnSessionErrorEvent.Broadcast(Reason);
}


void USessionSubsystem::OnDestroySessionAfterCleanup(FName SessionName, bool bWasSuccessful)
{
    if (SessionName != NAME_GameSession || !bCleanupInProgress) return;
    bCleanupInProgress = false;
    FTSTicker::GetCoreTicker().RemoveTicker(CleanupTimeoutHandle);
    CleanupTimeoutHandle.Reset();
    if (SessionInterface.IsValid())
    {
        SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
    }

    // bIsLocalHost는 cleanup 진입 시점에 이미 결정됐고 콜백 도달 시점엔 의미 없음 (역할 식별용 마지막 로그)
    UE_LOG(LogSessionSubsystem, Warning,
        TEXT("OnDestroySessionAfterCleanup: ok=%d role=%s reason=%s"),
        bWasSuccessful ? 1 : 0,
        bIsLocalHost ? TEXT("HOST (lobby destroyed)") : TEXT("CLIENT (left host lobby)"),
        *PendingCleanupReason);

    bIsLocalHost = false;
    // 클라 측 stale state 청소 — 새 방 재시도 시 잔여 영향 차단
    PendingDediURL.Empty();
    PendingMatchId.Empty();

    const FString Reason = PendingCleanupReason;
    PendingCleanupReason.Empty();

    if (!Reason.IsEmpty()) OnSessionErrorEvent.Broadcast(Reason);
}

void USessionSubsystem::ReturnToMainMenuAfterSessionCleanup(const FString& MainMenuMapPath)
{
    if (MainMenuMapPath.IsEmpty())
    {
        return;
    }

    PendingReturnToMainMenuMapPath = MainMenuMapPath;

    const bool bHasNamedSession = SessionInterface.IsValid() &&
        SessionInterface->GetNamedSession(NAME_GameSession) != nullptr;

    if (!bHasNamedSession)
    {
        TravelToPendingMainMenu();
        return;
    }

    UE_LOG(LogSessionSubsystem, Warning, TEXT("ReturnToMainMenuAfterSessionCleanup: destroying/leaving session before travel."));

    SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(ReturnToMainMenuDestroySessionCompleteDelegateHandle);
    ReturnToMainMenuDestroySessionCompleteDelegateHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(
        FOnDestroySessionCompleteDelegate::CreateUObject(this, &USessionSubsystem::OnDestroySessionAfterReturnToMainMenu));

    if (!SessionInterface->DestroySession(NAME_GameSession))
    {
        UE_LOG(LogSessionSubsystem, Warning, TEXT("ReturnToMainMenuAfterSessionCleanup: DestroySession failed to start; traveling anyway."));
        SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(ReturnToMainMenuDestroySessionCompleteDelegateHandle);
        TravelToPendingMainMenu();
    }
}

void USessionSubsystem::OnDestroySessionAfterReturnToMainMenu(FName /*SessionName*/, bool bWasSuccessful)
{
    if (SessionInterface.IsValid())
    {
        SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(ReturnToMainMenuDestroySessionCompleteDelegateHandle);
    }

    UE_LOG(LogSessionSubsystem, Warning, TEXT("OnDestroySessionAfterReturnToMainMenu: ok=%d"), bWasSuccessful ? 1 : 0);

    bIsLocalHost = false;
    PendingDediURL.Empty();
    PendingMatchId.Empty();

    TravelToPendingMainMenu();
}

void USessionSubsystem::TravelToPendingMainMenu()
{
    const FString TravelURL = PendingReturnToMainMenuMapPath;
    PendingReturnToMainMenuMapPath.Empty();

    if (TravelURL.IsEmpty())
    {
        return;
    }

    if (APlayerController* PC = GetGameInstance() ? GetGameInstance()->GetFirstLocalPlayerController() : nullptr)
    {
        PC->ClientTravel(TravelURL, ETravelType::TRAVEL_Absolute);
    }
}


bool USessionSubsystem::IsInActiveSession() const
{
    return SessionInterface.IsValid() &&
        SessionInterface->GetNamedSession(NAME_GameSession) != nullptr;
}


void USessionSubsystem::FinalizeHostTravel(const FString& DediURL, const FString& MatchId)
{
    // 1) SessionSettings에 match_id만 광고 → 조이너는 이걸로 /api/match/{id}/address 호출.
    //    dedi ip:port 평문 광고 폐지: Steam lobby data는 발견 가능한 모두에게 노출되므로
    //    네트워크 표면을 좁히기 위해 주소는 매치메이커 인증 게이트 뒤로 숨김.
    if (SessionInterface.IsValid())
    {
        if (auto* Named = SessionInterface->GetNamedSession(NAME_GameSession))
        {
            Named->SessionSettings.Set(Key_MatchId, MatchId,
                EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
            SessionInterface->UpdateSession(NAME_GameSession, Named->SessionSettings, true);
        }
    }

    // 2) 호스트 본인 데디로 이동 (호스트는 /create 응답으로 이미 ip:port 보유 — 별도 /address 불필요)
    //    ACToken은 URL에 부착하지 않음 — 데디가 UniqueId(SteamID)로 AC 검증.
    if (APlayerController* PC = GetGameInstance()->GetFirstLocalPlayerController())
    {
        PC->ClientTravel(DediURL, ETravelType::TRAVEL_Absolute);
    }
}


// ---------------------------------------------------------
// 게임 시작/종료 시 세션 가시성 토글
// ---------------------------------------------------------
void USessionSubsystem::LockSessionForInGame()
{
    if (!bIsLocalHost || !SessionInterface.IsValid())
    {
        return;
    }

    FNamedOnlineSession* Named = SessionInterface->GetNamedSession(NAME_GameSession);
    if (Named == nullptr)
    {
        UE_LOG(LogSessionSubsystem, Warning, TEXT("LockSessionForInGame: named session missing."));
        return;
    }

    // 가시성·합류경로는 유지 — 방 목록에 계속 보이도록.
    // 방 목록 위젯은 Key_GameStarted=1을 보고 클릭 비활성화한다.
    // 백스톱은 두 겹: (1) OSS — StartSession + bAllowJoinInProgress=false 조합으로 진행 중 합류 거부
    //                (2) 데디 MainGameMode::PreLogin — GamePhase != Lobby면 거부
    Named->SessionSettings.Set(Key_GameStarted, (int32)1, EOnlineDataAdvertisementType::ViaOnlineService);

    SessionInterface->UpdateSession(NAME_GameSession, Named->SessionSettings, /*bShouldRefreshOnlineData=*/true);

    // StartSession을 호출해야 bAllowJoinInProgress=false가 실제 발효 (InProgress 상태 진입)
    SessionInterface->StartSession(NAME_GameSession);

    UE_LOG(LogSessionSubsystem, Warning, TEXT("Session locked for in-game (GameStarted=1, StartSession fired)."));
}


void USessionSubsystem::UnlockSessionForLobby()
{
    if (!bIsLocalHost || !SessionInterface.IsValid())
    {
        return;
    }

    FNamedOnlineSession* Named = SessionInterface->GetNamedSession(NAME_GameSession);
    if (Named == nullptr)
    {
        UE_LOG(LogSessionSubsystem, Warning, TEXT("UnlockSessionForLobby: named session missing."));
        return;
    }

    // 진행 상태 0 복귀 + 세션을 NotStarted로 되돌려 bAllowJoinInProgress 게이트 해제
    Named->SessionSettings.Set(Key_GameStarted, (int32)0, EOnlineDataAdvertisementType::ViaOnlineService);

    SessionInterface->UpdateSession(NAME_GameSession, Named->SessionSettings, true);
    SessionInterface->EndSession(NAME_GameSession);

    UE_LOG(LogSessionSubsystem, Warning, TEXT("Session unlocked for lobby (GameStarted=0, EndSession fired)."));
}


// ---------------------------------------------------------
// 2. 방 목록 검색
// ---------------------------------------------------------

void USessionSubsystem::FinishFindRequest()
{
    FTSTicker::GetCoreTicker().RemoveTicker(FindTimeoutHandle);
    FindTimeoutHandle.Reset();
    if (SessionInterface.IsValid())
    {
        SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
    }
    FindSessionsCompleteDelegateHandle.Reset();
    bFindingRooms = false;
    bFindTimedOut = false;
}

void USessionSubsystem::OnFindTimeout()
{
    FindTimeoutHandle.Reset();
    if (!bFindingRooms || bFindTimedOut) return;
    bFindTimedOut = true;
    OnRoomsFoundEvent.Broadcast(false, TArray<FRoomListEntry>());
    // Steam은 CancelFindSessions 성공 후에도 원래 Find 완료 이벤트를 발생시킵니다.
    // 그 이벤트가 새 검색을 완료시키지 않도록 원래 콜백까지 직렬화를 유지합니다.
}

void USessionSubsystem::ResetJoinAttempt()
{
    FTSTicker::GetCoreTicker().RemoveTicker(JoinTimeoutHandle);
    JoinTimeoutHandle.Reset();
    if (SessionInterface.IsValid())
    {
        SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroyBeforeJoinDelegateHandle);
        SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
    }
    DestroyBeforeJoinDelegateHandle.Reset();
    JoinSessionCompleteDelegateHandle.Reset();
    PendingJoinResult.Reset();
    PendingMatchId.Empty();
    PendingDediURL.Empty();
    JoinFlow = EJoinFlow::Idle;
    bJoinTimedOut = false;
    ++JoinRequestId; // 늦게 도착한 티켓/HTTP 응답을 무효화합니다.
}

bool USessionSubsystem::IsCurrentAddressRequest(uint64 RequestId) const
{
    return JoinFlow == EJoinFlow::ResolvingAddress && JoinRequestId == RequestId;
}

void USessionSubsystem::FailJoin(const FString& Reason, bool bCleanupSession)
{
    ResetJoinAttempt();
    OnJoinSessionCompleteEvent.Broadcast(false);
    if (bCleanupSession) CleanupHostSession(Reason);
    else OnSessionErrorEvent.Broadcast(Reason);
}

void USessionSubsystem::ArmJoinTimeout()
{
    FTSTicker::GetCoreTicker().RemoveTicker(JoinTimeoutHandle);
    JoinTimeoutHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateWeakLambda(this, [this](float) { OnJoinTimeout(); return false; }), 25.0f);
}

void USessionSubsystem::OnJoinTimeout()
{
    JoinTimeoutHandle.Reset();
    if (JoinFlow == EJoinFlow::Idle || bJoinTimedOut) return;
    const FString Reason = TEXT("방 참가 응답 시간이 초과되었습니다. 잠시 후 다시 시도해주세요.");
    if (JoinFlow == EJoinFlow::ResolvingAddress) { FailJoin(Reason, true); return; }
    // OSS에는 Join 취소 API가 없습니다. UI는 복구하되 원래 작업의 완료 전 중복 실행을 막습니다.
    bJoinTimedOut = true;
    ++JoinRequestId;
    OnJoinSessionCompleteEvent.Broadcast(false);
    OnSessionErrorEvent.Broadcast(Reason);
}

void USessionSubsystem::OnDestroySessionBeforeJoin(FName SessionName, bool bWasSuccessful)
{
    if (SessionName != NAME_GameSession || JoinFlow != EJoinFlow::DestroyingPrevious) return;
    if (SessionInterface.IsValid())
        SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroyBeforeJoinDelegateHandle);
    DestroyBeforeJoinDelegateHandle.Reset();
    if (bJoinTimedOut) { ResetJoinAttempt(); return; }
    if (!bWasSuccessful || !SessionInterface.IsValid() || SessionInterface->GetNamedSession(NAME_GameSession))
    {
        FailJoin(TEXT("이전 방의 세션을 정리하지 못했습니다. 다시 시도해주세요."));
        return;
    }
    bIsLocalHost = false;
    StartPendingJoin();
}

void USessionSubsystem::StartPendingJoin()
{
    const ULocalPlayer* LocalPlayer = GetGameInstance() ? GetGameInstance()->GetFirstGamePlayer() : nullptr;
    if (!SessionInterface.IsValid() || !PendingJoinResult.IsSet() ||
        !LocalPlayer || !LocalPlayer->GetPreferredUniqueNetId().IsValid())
    {
        FailJoin(TEXT("참가할 방 또는 로컬 플레이어 정보가 유효하지 않습니다."));
        return;
    }
    JoinFlow = EJoinFlow::Joining;
    bIsLocalHost = false;
    JoinSessionCompleteDelegateHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(
        FOnJoinSessionCompleteDelegate::CreateUObject(this, &USessionSubsystem::OnJoinSessionComplete));
    ArmJoinTimeout();
    if (!SessionInterface->JoinSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, PendingJoinResult.GetValue())
        && JoinFlow == EJoinFlow::Joining)
        FailJoin(TEXT("방 참가 요청을 시작하지 못했습니다."));
}

void USessionSubsystem::FindRooms()
{
    if (!SessionInterface.IsValid())
    {
        UE_LOG(LogSessionSubsystem, Warning, TEXT("FindRooms: SessionInterface invalid — empty result."));
        OnRoomsFoundEvent.Broadcast(false, TArray<FRoomListEntry>());
        return;
    }

    if (bFindingRooms)
    {
        if (bFindTimedOut) OnRoomsFoundEvent.Broadcast(false, TArray<FRoomListEntry>());
        return;
    }
    if (IsJoiningRoom() || bCleanupInProgress || !PendingReturnToMainMenuMapPath.IsEmpty())
    {
        OnRoomsFoundEvent.Broadcast(false, TArray<FRoomListEntry>());
        return;
    }
    const ULocalPlayer* LocalPlayer = GetGameInstance() ? GetGameInstance()->GetFirstGamePlayer() : nullptr;
    if (!LocalPlayer || !LocalPlayer->GetPreferredUniqueNetId().IsValid())
    {
        OnRoomsFoundEvent.Broadcast(false, TArray<FRoomListEntry>());
        return;
    }

    SessionSearch = MakeShared<FOnlineSessionSearch>();
    SessionSearch->bIsLanQuery = false;
    // 작은 limit + 서버측 GameTag 필터 조합으로 480 풀을 우리 lobby로 좁힘.
    // (큰 limit은 메타데이터 fetch가 15s timeout; 작은 limit 단독은 sampling miss로 0건)
    SessionSearch->MaxSearchResults = 100;
    // SEARCH_LOBBIES — Steam OSS lobby 검색의 핵심 키.
    SessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
    // 서버측 GameTag 필터 — Steam OSS의 AddRequestLobbyListNumericalFilter 경로.
    // 클라측 SessionSettings.Get(Key_GameTag) 필터는 백스톱으로 유지(서버 필터 우회 lobby 차단).
    SessionSearch->QuerySettings.Set(Key_GameTag, GameTagMagic, EOnlineComparisonOp::Equals);

    // 이전 핸들 정리 후 재바인딩
    SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
    FindSessionsCompleteDelegateHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(
        FOnFindSessionsCompleteDelegate::CreateUObject(this, &USessionSubsystem::OnFindSessionsComplete));

    bFindingRooms = true;
    bFindTimedOut = false;
    FindTimeoutHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateWeakLambda(this, [this](float) { OnFindTimeout(); return false; }), 25.0f);
    if (!SessionInterface->FindSessions(*LocalPlayer->GetPreferredUniqueNetId(), SessionSearch.ToSharedRef()) && bFindingRooms)
    {
        FinishFindRequest();
        SessionSearch.Reset();
        OnRoomsFoundEvent.Broadcast(false, TArray<FRoomListEntry>());
    }
}

void USessionSubsystem::OnFindSessionsComplete(bool bWasSuccessful)
{
    if (!bFindingRooms) return;
    const bool bDiscard = bFindTimedOut;
    FinishFindRequest();
    if (bDiscard) { SessionSearch.Reset(); return; }
    if (SessionInterface.IsValid())
    {
        SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
    }

    TArray<FRoomListEntry> Out;

    if (!bWasSuccessful || !SessionSearch.IsValid())
    {
        UE_LOG(LogSessionSubsystem, Warning, TEXT("OnFindSessionsComplete: failed or invalid SessionSearch."));
        OnRoomsFoundEvent.Broadcast(false, Out);
        return;
    }

    const TArray<FOnlineSessionSearchResult>& Results = SessionSearch->SearchResults;

    UE_LOG(LogSessionSubsystem, Warning, TEXT("Steam returned %d lobbies:"), Results.Num());
    for (int32 i = 0; i < Results.Num(); ++i)
    {
        const FOnlineSessionSearchResult& R = Results[i];
        const FOnlineSession& S = R.Session;

        // UE_LOG(LogSessionSubsystem, Warning,
        //     TEXT("  raw[%d]: Host=%s Max=%d Open=%d"),
        //     i, *S.OwningUserName, S.SessionSettings.NumPublicConnections,
        //     S.NumOpenPublicConnections);

        // Indian_Bab 로비만 통과
        // 키 없거나 값 mismatch면 외부 게임 로비로 간주하고 skip.
        int32 TagValue = 0;
        if (!S.SessionSettings.Get(Key_GameTag, TagValue) || TagValue != GameTagMagic)
        {
            continue;
        }
        
        FRoomListEntry Entry;
        Entry.HostName = S.OwningUserName;
        Entry.MaxPlayers = S.SessionSettings.NumPublicConnections;
        Entry.CurrentPlayers = FMath::Max(0, Entry.MaxPlayers - S.NumOpenPublicConnections);

        int32 GameStartedInt = 0;
        Entry.bGameStarted = S.SessionSettings.Get(Key_GameStarted, GameStartedInt) && GameStartedInt == 1;
        Entry.bFriendsOnly = S.SessionSettings.bAllowJoinViaPresenceFriendsOnly;
        Entry.SearchResultIndex = i;

        Out.Add(Entry);
    }

    UE_LOG(LogSessionSubsystem, Warning, TEXT("OnFindSessionsComplete: %d rooms found."), Out.Num());
    for (int32 i = 0; i < Out.Num(); ++i)
    {
        const FRoomListEntry& E = Out[i];
        UE_LOG(LogSessionSubsystem, Warning,
            TEXT("  room[%d]: Host=%s Players=%d/%d GameStarted=%s FriendsOnly=%s"),
            i, *E.HostName, E.CurrentPlayers, E.MaxPlayers,
            E.bGameStarted ? TEXT("YES") : TEXT("NO"),
            E.bFriendsOnly ? TEXT("YES") : TEXT("NO"));
    }
    OnRoomsFoundEvent.Broadcast(true, Out);
}


// ---------------------------------------------------------
// 3. 인덱스 기반 참가
// ---------------------------------------------------------
void USessionSubsystem::JoinRoomByIndex(int32 SearchResultIndex)
{
    if (IsJoiningRoom())
    {
        if (bJoinTimedOut) OnSessionErrorEvent.Broadcast(TEXT("이전 참가 요청을 정리 중입니다. 잠시 후 다시 시도해주세요."));
        return;
    }
    if (bFindingRooms || bCleanupInProgress || !PendingReturnToMainMenuMapPath.IsEmpty())
    {
        OnJoinSessionCompleteEvent.Broadcast(false);
        return;
    }
    if (!SessionInterface.IsValid() || !SessionSearch.IsValid())
    {
        UE_LOG(LogSessionSubsystem, Warning, TEXT("JoinRoomByIndex: no SessionInterface/SessionSearch."));
        OnSessionErrorEvent.Broadcast(TEXT("세션 시스템이 준비되지 않았습니다. 방 검색을 다시 시도해주세요."));
        OnJoinSessionCompleteEvent.Broadcast(false);
        return;
    }

    if (!SessionSearch->SearchResults.IsValidIndex(SearchResultIndex))
    {
        UE_LOG(LogSessionSubsystem, Warning, TEXT("JoinRoomByIndex: index %d out of range (size=%d)."),
            SearchResultIndex, SessionSearch->SearchResults.Num());
        OnSessionErrorEvent.Broadcast(TEXT("선택한 방을 찾을 수 없습니다. 방 목록을 새로고침해주세요."));
        OnJoinSessionCompleteEvent.Broadcast(false);
        return;
    }

    const FOnlineSessionSearchResult& Result = SessionSearch->SearchResults[SearchResultIndex];

    // GameStarted 가드 (위젯 disable의 race 백스톱)
    int32 GameStartedInt = 0;
    if (Result.Session.SessionSettings.Get(Key_GameStarted, GameStartedInt) && GameStartedInt == 1)
    {
        UE_LOG(LogSessionSubsystem, Warning, TEXT("JoinRoomByIndex: room already in-game — denied."));
        OnSessionErrorEvent.Broadcast(TEXT("게임이 이미 진행 중인 방입니다."));
        OnJoinSessionCompleteEvent.Broadcast(false);
        return;
    }

    // match_id 추출 (호스트가 FinalizeHostTravel에서 박제). 실제 dedi 주소는
    // Join 성공 후 매치메이커 /address로 인증 조회 — lobby data 평문 누설 차단.
    PendingDediURL.Empty();
    PendingMatchId.Empty();
    Result.Session.SessionSettings.Get(Key_MatchId, PendingMatchId);
    if (PendingMatchId.IsEmpty())
    {
        UE_LOG(LogSessionSubsystem, Error, TEXT("JoinRoomByIndex: Key_MatchId missing in target session."));
        OnSessionErrorEvent.Broadcast(TEXT("방 정보가 손상되었습니다. 다른 방을 시도해주세요."));
        OnJoinSessionCompleteEvent.Broadcast(false);
        return;
    }

    const ULocalPlayer* LocalPlayer = GetGameInstance() ? GetGameInstance()->GetFirstGamePlayer() : nullptr;
    if (!LocalPlayer || !LocalPlayer->GetPreferredUniqueNetId().IsValid())
    {
        FailJoin(TEXT("로컬 플레이어 정보를 가져올 수 없습니다."));
        return;
    }
    PendingJoinResult = Result; // 검색 목록이 바뀌어도 선택한 방의 복사본을 사용합니다.
    ++JoinRequestId;
    bJoinTimedOut = false;
    if (SessionInterface->GetNamedSession(NAME_GameSession))
    {
        JoinFlow = EJoinFlow::DestroyingPrevious;
        DestroyBeforeJoinDelegateHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(
            FOnDestroySessionCompleteDelegate::CreateUObject(this, &USessionSubsystem::OnDestroySessionBeforeJoin));
        ArmJoinTimeout();
        if (!SessionInterface->DestroySession(NAME_GameSession) && JoinFlow == EJoinFlow::DestroyingPrevious)
            FailJoin(TEXT("이전 방의 세션 정리를 시작하지 못했습니다."));
        return;
    }
    StartPendingJoin();
}

void USessionSubsystem::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
    if (SessionName != NAME_GameSession || JoinFlow != EJoinFlow::Joining) return;
    if (bJoinTimedOut)
    {
        ResetJoinAttempt();
        if (Result == EOnJoinSessionCompleteResult::Success) CleanupHostSession(FString());
        return;
    }
    if (SessionInterface.IsValid())
    {
        SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
    }

    const bool bJoinOk = (Result == EOnJoinSessionCompleteResult::Success);

    if (bJoinOk)
    {
        UE_LOG(LogSessionSubsystem, Warning, TEXT("Joined Session Successfully!"));

        if (PendingMatchId.IsEmpty())
        {
            // Join은 성공해서 호스트 Lobby 멤버로 들어간 상태 — match_id만 누락.
            // 그냥 return하면 호스트 슬롯 점유한 채 leak되므로 명시적 leave 필수.
            UE_LOG(LogSessionSubsystem, Error, TEXT("Join OK but MatchId missing in SessionSettings."));
            FailJoin(TEXT("방 정보가 손상되었습니다. 다른 방을 시도해주세요."), true);
            return;
        }

        // dedi 주소를 매치메이커에 인증 조회 → 응답에서 ClientTravel.
        UE_LOG(LogSessionSubsystem, Warning, TEXT("Join OK → /address 조회 (match_id=%s)"), *PendingMatchId);
        JoinFlow = EJoinFlow::ResolvingAddress;
        ArmJoinTimeout();
        RequestDediAddress(PendingMatchId);
    }
    else
    {
        UE_LOG(LogSessionSubsystem, Error, TEXT("Join Failed. Code: %d"), (int32)Result);
        // EOnJoinSessionCompleteResult를 사용자 친화 텍스트로 매핑
        FString Reason;
        switch (Result)
        {
        case EOnJoinSessionCompleteResult::SessionIsFull:           Reason = TEXT("방이 가득 찼습니다."); break;
        case EOnJoinSessionCompleteResult::SessionDoesNotExist:     Reason = TEXT("해당 방이 더 이상 존재하지 않습니다."); break;
        case EOnJoinSessionCompleteResult::CouldNotRetrieveAddress: Reason = TEXT("방 주소를 가져올 수 없습니다."); break;
        case EOnJoinSessionCompleteResult::AlreadyInSession:        Reason = TEXT("이미 다른 방에 입장한 상태입니다."); break;
        default:                                                     Reason = TEXT("방 입장에 실패했습니다."); break;
        }
        // Join 실패 시점에는 호스트 Lobby에 합류한 적이 없음 (OSS가 자체 정리).
        // 사유 모달만 띄우면 됨 — 명시적 DestroySession 불필요.
        FailJoin(Reason);
    }
}


// ---------------------------------------------------------
// 매치메이커 흐름 (조이너)
// ---------------------------------------------------------
void USessionSubsystem::RequestDediAddress(const FString& MatchId)
{
    if (JoinFlow != EJoinFlow::ResolvingAddress) return;
    const uint64 RequestId = JoinRequestId;
    #if !MATCHMAKER_USE_MOCK
    USteamCredentialsSubsystem* SteamCreds =
        GetGameInstance() ? GetGameInstance()->GetSubsystem<USteamCredentialsSubsystem>() : nullptr;
    if (!SteamCreds)
    {
        UE_LOG(LogSessionSubsystem, Error, TEXT("[SessionSubsystem] SteamCredentialsSubsystem 없음 — /address 중단."));
        FailJoin(TEXT("Steam 인증 정보를 가져올 수 없습니다. Steam 로그인 상태를 확인해주세요."), true);
        return;
    }

    TWeakObjectPtr<USessionSubsystem> WeakThis(this);
    const FString CapturedMatchId = MatchId;
    SteamCreds->RequestTicket(
        [WeakThis, CapturedMatchId, RequestId](bool bOk, FString SteamTicketHex, FString JoinerSteamId)
        {
            if (!WeakThis.IsValid() || !WeakThis->IsCurrentAddressRequest(RequestId)) return;
            if (!bOk)
            {
                UE_LOG(LogSessionSubsystem, Error, TEXT("[SessionSubsystem] Steam credentials 획득 실패 — /address 중단."));
                WeakThis->FailJoin(TEXT("Steam 인증 정보를 가져올 수 없습니다. Steam 로그인 상태를 확인해주세요."), true);
                return;
            }

            const FString Body = FString::Printf(
                TEXT("{\"joiner_steam_id\":\"%s\",\"steam_session_ticket\":\"%s\"}"),
                *JoinerSteamId, *SteamTicketHex);

            auto Req = FHttpModule::Get().CreateRequest();
            Req->SetURL(NetworkEndpoints::MM::External::MatchAddress(CapturedMatchId));
            Req->SetVerb(TEXT("POST"));
            Req->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
            Req->SetContentAsString(Body);
            Req->SetTimeout(15.0f);
            Req->OnProcessRequestComplete().BindLambda(
                [WeakThis, RequestId](FHttpRequestPtr, FHttpResponsePtr Resp, bool bHttpOk)
                {
                    if (!WeakThis.IsValid() || !WeakThis->IsCurrentAddressRequest(RequestId)) return;
                    const int32 Code = (Resp.IsValid()) ? Resp->GetResponseCode() : 0;
                    WeakThis->HandleAddressResponse(
                        Resp.IsValid() ? Resp->GetContentAsString() : FString(),
                        Code,
                        bHttpOk && Resp.IsValid() && Code == 200);
                });
            if (!Req->ProcessRequest() && WeakThis->IsCurrentAddressRequest(RequestId))
                WeakThis->FailJoin(TEXT("서버 주소 조회 요청을 시작하지 못했습니다."), true);
        });
    return;
    #else
    // 호스트 /create 모킹과 정합 — 동일 ip/port로 응답.
    const FString MockResponse = TEXT(R"({"server_ip":"127.0.0.1","port":7777})");
    HandleAddressResponse(MockResponse, 200, true);
    #endif
}


void USessionSubsystem::HandleAddressResponse(const FString& JsonBody, int32 HttpCode, bool bOk)
{
    if (JoinFlow != EJoinFlow::ResolvingAddress) return;
    if (!bOk)
    {
        UE_LOG(LogSessionSubsystem, Error, TEXT("Address request failed (code=%d). body=%s"), HttpCode, *JsonBody);
        // Join 성공 후 합류한 호스트 Lobby 슬롯 점유 상태 — 사유에 맞춰 leave.
        FString Reason;
        switch (HttpCode)
        {
        case 401: Reason = TEXT("Steam 인증에 실패했습니다."); break;
        case 404: Reason = TEXT("해당 방이 더 이상 존재하지 않습니다."); break;
        default:  Reason = TEXT("매치메이커 응답 실패. 잠시 후 다시 시도해주세요."); break;
        }
        FailJoin(Reason, true);
        return;
    }

    TSharedPtr<FJsonObject> Json;
    auto Reader = TJsonReaderFactory<>::Create(JsonBody);
    if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid())
    {
        UE_LOG(LogSessionSubsystem, Error, TEXT("Address response parse failed: %s"), *JsonBody);
        FailJoin(TEXT("매치메이커 응답 형식이 올바르지 않습니다."), true);
        return;
    }

    FString ServerIp;
    int32 Port = 0;
    if (!Json->TryGetStringField(TEXT("server_ip"), ServerIp) ||
        !Json->TryGetNumberField(TEXT("port"), Port) ||
        ServerIp.IsEmpty() || Port <= 0)
    {
        UE_LOG(LogSessionSubsystem, Error, TEXT("Address response missing server_ip/port: %s"), *JsonBody);
        FailJoin(TEXT("방 정보가 손상되었습니다. 다른 방을 시도해주세요."), true);
        return;
    }

    PendingDediURL = FString::Printf(TEXT("%s:%d"), *ServerIp, Port);
    UE_LOG(LogSessionSubsystem, Warning, TEXT("Address OK → ClientTravel %s"), *PendingDediURL);
    APlayerController* PC = GetGameInstance() ? GetGameInstance()->GetFirstLocalPlayerController() : nullptr;
    if (!PC)
    {
        FailJoin(TEXT("서버로 이동할 플레이어 컨트롤러가 없습니다."), true);
        return;
    }
    const FString TravelURL = PendingDediURL;
    ResetJoinAttempt();
    PC->ClientTravel(TravelURL, ETravelType::TRAVEL_Absolute);
    OnJoinSessionCompleteEvent.Broadcast(true);
}
