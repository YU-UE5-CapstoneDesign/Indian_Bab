#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "Game/MainGameTypes.h"
#include "CardController/CardData.h"
#include "MainGameMode.generated.h"

class AMainGamePlayerController;
class ASeatActor;
class ALobbyCharacter;
class ALobbyVRCharacter;
class AMainGameState;
class AMainPlayerState;
class ACardManager;
class ARevolver;

UCLASS()
class INDIAN_BAB_API AMainGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	AMainGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;

	// 동기 합류 조건(phase·만석·incompatible_net_id)만 검사. 안티치트 의존 0.
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;

	// 안티치트 게이트 — UniqueId(SteamID)로 AC /internal/anc/prelogin 비동기 호출.
	// PreLogin이 동기 함수라 HTTP 응답을 기다릴 수 없어 비동기 콜백 버전 사용.
	// AC 미운영(서버 다운) 시 fail-closed로 거부 — 단독 운영하려면 AC 호출 자체를
	// 컴파일/런타임 분기로 토글하는 게 옳음(현재 미적용, 후속 작업).
	virtual void PreLoginAsync(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, const FOnPreLoginCompleteDelegate& OnComplete) override;

	// 플레이어가 서버에 접속 완료했을 때 호출됨.
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void RestartPlayer(AController* NewPlayer) override;

	// 플레이어가 데디 NetConnection을 끊었을 때 — 정상 이탈/강제종료/timeout 공통 SSoT
	virtual void Logout(AController* Exiting) override;

	// 준비 완료 버튼을 눌렀을 때 호출
	void HandlePlayerReady(APlayerController* ReadyPlayer);

	// 플레이어가 의자에 앉았을 때 호출됨
	void PlayerSeated(APlayerController* SeatedPlayer, ASeatActor* SeatedChair);

	// 접속한 플레이어를 빈 의자에 자동 배치
	void AssignInitialSeatToPlayer(APlayerController* NewPlayer);

	// 베팅 액션 관리
	void HandleBetAction(AMainGamePlayerController* RequestPC, EBetAction Action, int32 RaiseCount);

	// 폴드 베팅 액션
	void HandleFoldAction(AMainGamePlayerController* RequestPC);

	// 메인 리볼버 격발 액션
	void HandleMainRevolverShotAction(AMainGamePlayerController* RequestPC);

	void HandleMainRevolverGrabbed(ALobbyCharacter* Character);

	// 자기 머리에 겨냥했을 때
	void HandleFoldMontageFinished(ALobbyCharacter* Character);

	// 메인 리볼버 가져오는 애니메이션 끝났을 때
	void HandleMainMontageFinished(ALobbyCharacter* Character);

	// 자기 머리에 쏜 이후
	void HandlePutBackGunMontageFinished(ALobbyCharacter* Character, EGunHoldReason Reason);

protected:
	// 기존 착석 기반 게임 시작 체크
	void CheckGameStart();

	// 게임 루프 시작점
	void StartMainGame();

	// 카드 매니저 획득
	TObjectPtr<ACardManager> GetCardManager();

	// 플레이어 선택
	void PickPlayer(int32 CurrentPlayerIndex);

	// 플레이어 랜덤 선택
	void PickRandomPlayer();

	// 결과 기반 선택
	void PickByResult();

	// 카드 분배
	void DistributeCard();

	// 턴 넘기는 타이머
	void StartTurnTimer(float Time);

	// 턴 제한시간을 넘겼을 때
	void OnTurnTimerExpired();

	// 격발 페이즈 관리
	void ManageShotPhase();

	// 격발 페이즈 종료 후 정리
	void FinishMainShotPhase();

	// 메인 리볼버 격발 시간 타이머
	void StartMainshotTimer(float Time);

	// 메인 리볼버 격발 시간 넘겼을 때
	void OnMainShotTimerExpired();

	void OnMainRevolverGrabTimerExpired();

	// 메인 리볼버 격발 실행
	void ExecuteMainShot(bool bAutoFire);

	// 활성 인원 업데이트
	int32 UpdateActivePlayer(AMainGameState* GS);

	// 결과 확인 및 승리 플레이어 PS 리턴
	void CheckPlayerCard();

	// 다음 행동 체크 함수
	void CheckNext();

	// 다음 플레이어 PS Get
	TObjectPtr<AMainPlayerState> GetNextPlayerState(int32 CurrentPlayerIndex);

	// 다음 턴
	void NextTurn(AMainPlayerState* NextPS);

	// 다음 라운드
	void NextRound();

	// 폴드 인원 초기화
	void ResetFoldState();

	void EndGame(AMainPlayerState* WinnerPS);

	AMainPlayerState* GetLastAlivePlayer();

	void ShowResultWidgets(FString WinnerName, int32 WinnerPlayerId);

private:
	// 빈 의자 찾기
	ASeatActor* FindEmptySeat();

	// 모든 플레이어가 Ready를 눌렀을 때 게임 시작 준비
	void StartGameAfterAllReady();

	int32 GetRequiredReadyPlayerCount() const;
	void GetSeatedPlayers(TArray<APlayerController*>& OutPlayers) const;

	bool IsCurrentPlayerCountInGameRange() const;

private:
	// MM dedi_manager.spawn이 주입한 -MatchId=<uuid>. InitGame에서 캐싱.
	FString CachedMatchId;

	// 방 생성 호스트 SteamID. MM dedi_manager.spawn이 주입한 -HostSteamId=<id>를 InitGame에서 캐싱.
	// 호스트 이탈 시 clear_host 발사 후 비움(재발사 방지 멱등).
	FString CachedHostSteamId;

	// 인스턴스 만석 기준. MM dedi_manager.spawn이 주입한 -MaxPlayers=N을 InitGame에서 캐싱.
	int32 CachedMaxPlayers = 0;

	// 토큰/SteamID 매핑 자료구조는 보관하지 않음 — 게임모드는 SteamID만 다룸.
	// Logout의 Exiting 인자에서 SteamID 추출 → AC가 SteamID로 verify_session 조회·리셋.
	// (CLAUDE.md "단일 활성 토큰" 정책상 SteamID → row 1:1 매칭)

	void NotifyACLeave(const FString& SteamId);
	void NotifyMatchClose();
	void NotifyMatchClearHost();

	FTimerHandle TimerHandle;

	// 준비 완료한 플레이어 목록
	UPROPERTY()
	TArray<TObjectPtr<APlayerController>> ReadyPlayers;

	// 게임 시작을 위한 최소/최대 플레이어 수
	UPROPERTY(EditDefaultsOnly, Category = "Game Start")
	int32 MinPlayerCountToStart = 2;

	UPROPERTY(EditDefaultsOnly, Category = "Game Start")
	int32 MaxPlayerCountToStart = 4;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game|Turn", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", UIMin = "0.1"))
	float TurnTimeLimitSeconds = 20.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game|Result", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
	float ResultWidgetDelaySeconds = 1.5f;

	// 게임 시작 중복 호출 방지
	bool bGameStartRequested = false;

	// 베팅 기준점 플레이어
	int32 CheckPlayer = -1;

	// 카드 관리
	UPROPERTY()
	TObjectPtr<ACardManager> MainCardManager;

	// 분배된 카드 배열
	UPROPERTY()
	TArray<FCardData> DealtCards;

	// 승리 플레이어 PS
	UPROPERTY()
	TObjectPtr<AMainPlayerState> CurrentWinnerPS;

	// 메인 리볼버
	UPROPERTY()
	TObjectPtr<ARevolver> MainRevolver;

	// 활성 인원 중에서 가장 큰 값을 가진 플레이어
	TObjectPtr<AMainPlayerState> MaxCardPlayer();

	ARevolver* GetMainRevolver();

	bool bCheckPlayerFolded = false;

	bool bMainRevolverPutBackInProgress = false;

	bool bGameEnded = false;

	void StartMainRevolverPutBack();

	// 메인 리볼버 탄창 칸 수
	int32 MainRevolverChamberCount = 8;

	int32 MaxMainRevolverChamberCount = 8;

	// 앞으로 몇 번 당기면 실탄이 나가는지
	int32 MainLiveShotOffset = -1;

	// 실탄 위치 초기화
	void InitMainRevolverLiveBulletIfNeeded();

	// 실탄 위치 재배치
	void RandomizeMainRevolverLiveBullet();

	// 방아쇠 당기는 함수
	bool PullMainRevolverTrigger();

	// 메인 리볼버 라인트레이스 거리
	UPROPERTY(EditDefaultsOnly, Category = "Main Revolver")
	float MainShotTraceDistance = 5000.0f;

	// 현재는 카메라 기준, 나중에는 총구 소켓 기준으로 바꿀 함수
	bool GetMainShotTraceStartEnd(AMainGamePlayerController* ShooterPC, FVector& OutStart, FVector& OutEnd);

	// 조준한 대상 판정
	AMainPlayerState* GetMainShotTargetByAim(AMainGamePlayerController* ShooterPC, FHitResult& OutHit);
};
