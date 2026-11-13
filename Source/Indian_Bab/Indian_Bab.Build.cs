// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Indian_Bab : ModuleRules
{
    public Indian_Bab(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[] {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "EnhancedInput",
            "AIModule",
            "StateTreeModule",
            "GameplayStateTreeModule",
            "HTTP",
            "HTTPServer",
            "Json",
            "JsonUtilities",
            "UMG",
            "Slate",
            "SlateCore",
            "ApplicationCore",
            "RHI",
            "OnlineSubsystemUtils",
            "OnlineSubsystem",
            "HairStrandsCore",
            "Niagara",
            "HeadMountedDisplay",
            "XRBase"
        });

        if (Target.Type != TargetType.Server)
        {
            DynamicallyLoadedModuleNames.Add("OnlineSubsystemSteam");
        }

        PrivateDependencyModuleNames.AddRange(new string[] { });

        // Steamworks SDK: SteamCredentials가 GetAuthSessionTicket 콜백 동기화를 위해 직접 호출.
        // 엔진 동봉 SDK 사용
        AddEngineThirdPartyPrivateStaticDependencies(Target, "Steamworks");

        //   bcrypt.lib: AntiCheatSubsystem SHA-256 (클라 부팅 verify)
        //   ole32.lib : ConnectivitySubsystem INetworkListManager COM (클라 NLM 폴링)
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            PublicSystemLibraries.Add("bcrypt.lib");
            PublicSystemLibraries.Add("ole32.lib");
        }

        // 전역 빌드 플래그 — Dev/Debug에서 켜고 끄는 mock 토글.
        // Shipping은 강제 0(실통신)으로 잠궈 사고 방지.
        int MatchmakerUseMock = 0;
        int AntiCheatUseMock  = 1;

        if (Target.Configuration == UnrealTargetConfiguration.Shipping)
        {
            MatchmakerUseMock = 0;
            AntiCheatUseMock  = 0;
        }

        PublicDefinitions.Add("MATCHMAKER_USE_MOCK=" + MatchmakerUseMock);
        PublicDefinitions.Add("ANTICHEAT_USE_MOCK=" + AntiCheatUseMock);

        // ─────────────────────────────────────────────────────────────
        // MM/AC 엔드포인트 — 외부/내부 2채널 SSoT.
        //   External: 클라/워치독 → Nginx(443, TLS) → MM/AC. 공인 도메인으로 진입.
        //   Internal: 데디 S2S(loopback). Nginx `/internal/*` deny 정책상 외부 절대 금지.
        // 헤더(Public/Network/NetworkEndpoints.h)가 이 매크로를 읽어 host/port/tls/BaseURL 노출.
        // BaseURL composer는 표준 포트(80/443)면 ":port" 생략(Host 헤더 server_name 매칭 안전).
        // 운영 도메인/IP/포트/스킴 변경은 여기서 수정 후 재빌드.
        // ─────────────────────────────────────────────────────────────
        string ExternalHost = "indianbab.freeddns.org";
        int    ExternalPort = 443;
        int    ExternalTls  = 1;

        string InternalHost = "127.0.0.1";
        int    MMInternalPort = 8000;
        int    ACInternalPort = 9000;

        PublicDefinitions.Add("INDIANBAB_MM_EXTERNAL_HOST=\""  + ExternalHost + "\"");
        PublicDefinitions.Add("INDIANBAB_MM_EXTERNAL_PORT="    + ExternalPort);
        PublicDefinitions.Add("INDIANBAB_MM_EXTERNAL_USE_TLS=" + ExternalTls);
        PublicDefinitions.Add("INDIANBAB_MM_INTERNAL_HOST=\""  + InternalHost + "\"");
        PublicDefinitions.Add("INDIANBAB_MM_INTERNAL_PORT="    + MMInternalPort);
        PublicDefinitions.Add("INDIANBAB_MM_INTERNAL_USE_TLS=0");

        PublicDefinitions.Add("INDIANBAB_AC_EXTERNAL_HOST=\""  + ExternalHost + "\"");
        PublicDefinitions.Add("INDIANBAB_AC_EXTERNAL_PORT="    + ExternalPort);
        PublicDefinitions.Add("INDIANBAB_AC_EXTERNAL_USE_TLS=" + ExternalTls);
        PublicDefinitions.Add("INDIANBAB_AC_INTERNAL_HOST=\""  + InternalHost + "\"");
        PublicDefinitions.Add("INDIANBAB_AC_INTERNAL_PORT="    + ACInternalPort);
        PublicDefinitions.Add("INDIANBAB_AC_INTERNAL_USE_TLS=0");

        PublicIncludePaths.AddRange(new string[] {
            "Indian_Bab",
            "Indian_Bab/TP_FirstPerson",
            "Indian_Bab/TP_FirstPerson/Variant_Horror",
            "Indian_Bab/TP_FirstPerson/Variant_Horror/UI",
            "Indian_Bab/TP_FirstPerson/Variant_Shooter",
            "Indian_Bab/TP_FirstPerson/Variant_Shooter/AI",
            "Indian_Bab/TP_FirstPerson/Variant_Shooter/UI",
            "Indian_Bab/TP_FirstPerson/Variant_Shooter/Weapons"
        });

        // Uncomment if you are using Slate UI
        // PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

        // Uncomment if you are using online features
        // PrivateDependencyModuleNames.Add("OnlineSubsystem");

        // To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
    }
}
