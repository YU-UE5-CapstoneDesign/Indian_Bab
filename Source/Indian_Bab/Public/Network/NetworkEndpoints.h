#pragma once

#include "CoreMinimal.h"

/**
 * MM(매치메이커)·AC(안티치트) 엔드포인트 단일 소스.
 *
 * 외부/내부 2채널:
 *   External — 클라/워치독 → Nginx(443, TLS) → MM/AC. 공인 도메인 진입.
 *   Internal — 데디 S2S(loopback). Nginx `/internal/` 경로 deny 정책상 외부 호출 금지.
 *
 * Host/Port/UseTls는 빌드타임 하드코딩 — 값은 Indian_Bab.Build.cs의
 *   INDIANBAB_{MM,AC}_{EXTERNAL,INTERNAL}_{HOST,PORT,USE_TLS}
 * PublicDefinitions에서 주입됨. 운영 변경 시 Build.cs 한 곳 수정 후 재빌드.
 *
 * BaseURL은 host/port/tls의 파생값 — 헤더에서 조립. 표준 포트(80/443)는
 * URL에 ":port" 생략(Host 헤더 server_name 매칭 안전, Nginx vhost 라우팅 깨짐 방지).
 * 경로는 모두 여기서 관리. 외부(클라/데디) 호출처는 URL 조립 책임 없음.
 *
 * 내부 S2S 경로(Internal 네임스페이스)는 WITH_SERVER_CODE 가드로
 * 클라 EXE(Type=Client)에는 컴파일조차 되지 않음.
 *
 * 워치독 WS는 클라 측 = AC External 그대로. 외부 도메인 wss://host/ws/{steam_id}.
 */

#if !defined(INDIANBAB_MM_EXTERNAL_HOST) || !defined(INDIANBAB_MM_EXTERNAL_PORT) || !defined(INDIANBAB_MM_EXTERNAL_USE_TLS)
    #error "INDIANBAB_MM_EXTERNAL_{HOST,PORT,USE_TLS} must be defined by Indian_Bab.Build.cs"
#endif
#if !defined(INDIANBAB_MM_INTERNAL_HOST) || !defined(INDIANBAB_MM_INTERNAL_PORT) || !defined(INDIANBAB_MM_INTERNAL_USE_TLS)
    #error "INDIANBAB_MM_INTERNAL_{HOST,PORT,USE_TLS} must be defined by Indian_Bab.Build.cs"
#endif
#if !defined(INDIANBAB_AC_EXTERNAL_HOST) || !defined(INDIANBAB_AC_EXTERNAL_PORT) || !defined(INDIANBAB_AC_EXTERNAL_USE_TLS)
    #error "INDIANBAB_AC_EXTERNAL_{HOST,PORT,USE_TLS} must be defined by Indian_Bab.Build.cs"
#endif
#if !defined(INDIANBAB_AC_INTERNAL_HOST) || !defined(INDIANBAB_AC_INTERNAL_PORT) || !defined(INDIANBAB_AC_INTERNAL_USE_TLS)
    #error "INDIANBAB_AC_INTERNAL_{HOST,PORT,USE_TLS} must be defined by Indian_Bab.Build.cs"
#endif

namespace NetworkEndpoints
{
namespace Detail
{
    inline FString ComposeBaseURL(const FString& Host, uint16 Port, bool bUseTls)
    {
        const bool bDefaultPort = (bUseTls && Port == 443) || (!bUseTls && Port == 80);
        if (bDefaultPort)
        {
            return FString::Printf(TEXT("%s://%s"),
                bUseTls ? TEXT("https") : TEXT("http"), *Host);
        }
        return FString::Printf(TEXT("%s://%s:%u"),
            bUseTls ? TEXT("https") : TEXT("http"), *Host, static_cast<uint32>(Port));
    }
}

namespace MM
{
    // 외부 공개 (클라 → Nginx → :8000)
    namespace External
    {
        inline const FString& Host()  { static const FString V = TEXT(INDIANBAB_MM_EXTERNAL_HOST); return V; }
        inline uint16         Port()  { return static_cast<uint16>(INDIANBAB_MM_EXTERNAL_PORT); }
        inline bool           UseTls(){ return INDIANBAB_MM_EXTERNAL_USE_TLS != 0; }
        inline const FString& BaseURL()
        {
            static const FString V = Detail::ComposeBaseURL(Host(), Port(), UseTls());
            return V;
        }

        inline FString CreateMatch()
        {
            return BaseURL() + TEXT("/api/match/create");
        }
        inline FString MatchAddress(const FString& MatchId)
        {
            return BaseURL() + FString::Printf(TEXT("/api/match/%s/address"), *MatchId);
        }
    }

#if WITH_SERVER_CODE
    // 내부 S2S (loopback, Nginx deny — 데디→MM).
    // 클라 EXE(Type=Client, WITH_SERVER_CODE=0)에는 경로 리터럴조차 박지 않음.
    namespace Internal
    {
        inline const FString& Host()  { static const FString V = TEXT(INDIANBAB_MM_INTERNAL_HOST); return V; }
        inline uint16         Port()  { return static_cast<uint16>(INDIANBAB_MM_INTERNAL_PORT); }
        inline bool           UseTls(){ return INDIANBAB_MM_INTERNAL_USE_TLS != 0; }
        inline const FString& BaseURL()
        {
            static const FString V = Detail::ComposeBaseURL(Host(), Port(), UseTls());
            return V;
        }

        inline FString Close(const FString& MatchId)
        {
            return BaseURL() + FString::Printf(TEXT("/internal/match/%s/close"), *MatchId);
        }
        inline FString ClearHost(const FString& MatchId)
        {
            return BaseURL() + FString::Printf(TEXT("/internal/match/%s/clear_host"), *MatchId);
        }
    }
#endif
}

namespace AC
{
    // 외부 공개 (클라/워치독 → Nginx → :9000)
    namespace External
    {
        inline const FString& Host()  { static const FString V = TEXT(INDIANBAB_AC_EXTERNAL_HOST); return V; }
        inline uint16         Port()  { return static_cast<uint16>(INDIANBAB_AC_EXTERNAL_PORT); }
        inline bool           UseTls(){ return INDIANBAB_AC_EXTERNAL_USE_TLS != 0; }
        inline const FString& BaseURL()
        {
            static const FString V = Detail::ComposeBaseURL(Host(), Port(), UseTls());
            return V;
        }

        inline FString Verify()
        {
            return BaseURL() + TEXT("/api/verify");
        }
    }

    // 워치독 WS 접속용 — AC External 그대로(Nginx wss → /ws/{steam_id} 프록시).
    inline const FString& WsHost()  { return External::Host(); }
    inline uint16         WsPort()  { return External::Port(); }
    inline bool           WsUseTls(){ return External::UseTls(); }

#if WITH_SERVER_CODE
    // 내부 S2S (loopback — 데디→AC). 클라 EXE 비노출.
    namespace Internal
    {
        inline const FString& Host()  { static const FString V = TEXT(INDIANBAB_AC_INTERNAL_HOST); return V; }
        inline uint16         Port()  { return static_cast<uint16>(INDIANBAB_AC_INTERNAL_PORT); }
        inline bool           UseTls(){ return INDIANBAB_AC_INTERNAL_USE_TLS != 0; }
        inline const FString& BaseURL()
        {
            static const FString V = Detail::ComposeBaseURL(Host(), Port(), UseTls());
            return V;
        }

        inline FString PreLogin()
        {
            return BaseURL() + TEXT("/internal/anc/prelogin");
        }
        inline FString Leave()
        {
            return BaseURL() + TEXT("/internal/anc/leave");
        }
    }
#endif
}

#if WITH_SERVER_CODE
// 데디가 *binding* 하는 엔드포인트 (수신 측). 나가는 URL 조립이 아니라 path 리터럴만
// 노출 — 데디는 자기 자신의 게임 포트 기반으로 listen 포트(+kHttpPortOffset)를 파생.
//
// AC 측 dedi_client.py 의 path/offset 과 짝. 변경 시 두 곳 동시 수정 (Python 공유 불가).
// 포트 분리 정책: 게임 UDP 포트(7777~7876)는 외부 공개, kick HTTP 는 +10000(17777~17876)
// 대역 loopback only — 운영 방화벽에서 17xxx 차단이 SSoT.
namespace Dedi
{
    namespace Internal
    {
        inline constexpr uint16 kHttpPortOffset = 10000;

        // 데디가 BindRoute 에 등록할 라우트 패턴. UE5 HttpRouter path param 문법 `:name`.
        inline const TCHAR* KickPlayerRoutePattern()
        {
            return TEXT("/internal/dedi/:steam_id/kick_player");
        }

        // 핸들러가 PathParams 에서 SteamID 를 꺼낼 때 사용할 키.
        inline const TCHAR* KickPlayerSteamIdParam()
        {
            return TEXT("steam_id");
        }

        // 외부에서 호출 URL 조립 시(테스트/디버깅용 — 실제 호출처는 AC Python).
        inline FString KickPlayerPath(const FString& SteamId)
        {
            return FString::Printf(TEXT("/internal/dedi/%s/kick_player"), *SteamId);
        }
    }
}
#endif
}
