#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "GameFramework/PlayerState.h"
#include "Actor/SeatActor.h"
#include "Game/MainGameTypes.h"
#include "MainGameState.generated.h"

DECLARE_MULTICAST_DELEGATE(FOnCurrentTurnPlayerChanged);
DECLARE_MULTICAST_DELEGATE(FOnTurnInfoChanged);

// 게임의 현재 진행 단계를 정의하는 Enum
UENUM(BlueprintType)
enum class EGamePhase : uint8
{
	Lobby UMETA(DisplayName = "로비 (대기중)"),
	Starting UMETA(DisplayName = "시작 카운트다운"),
	Playing UMETA(DisplayName = "게임 진행중"),
	Result UMETA(DisplayName = "결과창")
};

USTRUCT(BlueprintType)
struct FBetActionInfo
{
	GENERATED_BODY()

	UPROPERTY()
	EBetAction CurrentBetAction;

	UPROPERTY()
	int32 BetActionTotal;
};

USTRUCT(BlueprintType)
struct FBetLogData
{
	GENERATED_BODY()

public:

	UPROPERTY()
	FString PlayerName;

	UPROPERTY()
	EBetAction ActionType;
};

// 로비 표시 상태를 하나의 복제 속성으로 전달합니다.
USTRUCT(BlueprintType)
struct FLobbyReadyStatus
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    int32 HostPlayerId = INDEX_NONE;

    UPROPERTY(BlueprintReadOnly)
    int32 ConnectedPlayerCount = 0;

    UPROPERTY(BlueprintReadOnly)
    TArray<int32> ReadyPlayerIds;

    UPROPERTY(BlueprintReadOnly)
    bool bCanStart = false;
};

UCLASS()
class INDIAN_BAB_API AMainGameState : public AGameState
{
	GENERATED_BODY()

public:
	AMainGameState();

	FOnCurrentTurnPlayerChanged OnCurrentTurnPlayerChanged;
	FOnTurnInfoChanged OnTurnInfoChanged;

	// 상태 복제를 위한 필수 함수
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 현재 게임 단계
	UPROPERTY(ReplicatedUsing = OnRep_GamePhase, BlueprintReadOnly, Category = "Game State")
	EGamePhase CurrentGamePhase;

	// 현재 의자에 앉아 준비를 마친 플레이어 수
	UPROPERTY(ReplicatedUsing = OnRep_ReadyPlayerCount, BlueprintReadOnly, Category = "Game State")
	int32 ReadyPlayerCount;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Game State|Lobby")
    FLobbyReadyStatus LobbyReadyStatus;

	// 현재 의자에 앉아 준비를 마친 플레이어 수
	UPROPERTY(ReplicatedUsing = OnRep_AlivePlayerCount, BlueprintReadOnly, Category = "Game State")
	int32 AlivePlayerCount;

	// 현재 턴의 플레이어
	UPROPERTY(ReplicatedUsing = OnRep_CurrentTurnPlayerId, BlueprintReadOnly, Category = "Game State")
	int32 CurrentTurnPlayerId;

	UPROPERTY(Replicated, BlueprintReadOnly, Category ="Game State")
	bool bTurnActionInProgress;

	// 현재 턴의 플레이어의 인덱스(SeatChairArray 인덱스)
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Game State")
	int32 CurrentPlayerIndex;

	// 의자 배열(서버 전용 replicated 현재로선 불필요)
	UPROPERTY(BlueprintReadOnly, Category = "Game State")
	TArray<ASeatActor*> SeatChairArray;

	// 현재 누적된 메인 리볼버 당김 횟수
	UPROPERTY(ReplicatedUsing = OnRep_CurrentBulletCount, BlueprintReadOnly, Category = "Game State")
	int32 CurrentBulletCount;

	UPROPERTY(ReplicatedUsing = OnRep_MainRevolverChamberCount, BlueprintReadOnly, Category = "Game State")
	int32 MainRevolverChamberCount;

	UPROPERTY(ReplicatedUsing = OnRep_MainShotInfo, BlueprintReadOnly, Category = "Game State")
	int32 MainShotPlayerId;

	UPROPERTY(ReplicatedUsing = OnRep_MainShotInfo, BlueprintReadOnly, Category = "Game State")
	int32 MainShotTotalCount;

	// 현재 플레이어 액션
	UPROPERTY(ReplicatedUsing = OnRep_CurrentBetInfo, BlueprintReadOnly, Category = "Game State")
	FBetActionInfo CurrentBetInfo;

	// 현재 타이머가 끝나는 서버 시간
	UPROPERTY(ReplicatedUsing = OnRep_TimerInfo, BlueprintReadOnly, Category = "Timer")
	float TimerEndServerTime;

	// 타이머 전체 지속 시간
	UPROPERTY(ReplicatedUsing = OnRep_TimerInfo, BlueprintReadOnly, Category = "Timer")
	float TimerDuration;

	// 현재 타이머 활성 상태 유무
	UPROPERTY(ReplicatedUsing = OnRep_TimerInfo, BlueprintReadOnly, Category = "Timer")
	bool bTimerActive;

	//해당 부분에서 카드의 사용/미사용 여부를 저장할 예정
	UPROPERTY()
	TArray<bool> CardArray;

	UPROPERTY(ReplicatedUsing = OnRep_LatestBetLog)
	FBetLogData LatestBetLog;


	// 서버가 GamePhase를 변경할 때 호출
	void SetGamePhase(EGamePhase NewPhase);

	// 서버에서 턴이 바뀔 때 호출
	void ChangeGameTurn(int32 NewTurnPlayerId, int32 NewPlayerIndex);

	// 서버에서 최근 베팅 액션 호출
	void ChangeCurrentBetInfo(EBetAction NewAction, int32 RaiseCount = 0);

	void SetMainRevolverChamberCount(int32 NewChamberCount);

	void SetMainShotInfo(int32 NewMainShotPlayerId, int32 NewMainShotTotalCount);

	void ClearMainShotInfo();

	// 서버에서 준비 인원 호출
	void ChangeReadyPlayerCount(int NewReadyCount);

	// 다음 라운드 위한 초기화
	void SetNextRoundGameState();

	// 서버에서 타이머 정보를 세팅
	void SetTimerInfo(float Time);

	// 서버에서 타이머 정보를 초기화
	void ClearTimerInfo();

	// 현재 남은 시간을 계산
	UFUNCTION(BlueprintCallable, Category = "Timer")
	float GetRemainingTime() const;

	// UI 표시용 정수 남은 시간
	UFUNCTION(BlueprintCallable, Category = "Timer")
	int32 GetRemainingTimeCeil() const;

protected:
	// 클라이언트에서 GamePhase가 변경되었을 때 실행될 로직 (UI 업데이트 등)
	UFUNCTION()
	void OnRep_GamePhase();

	// 클라이언트에서 플레이어 ID가 변경되었을 때 실행
	UFUNCTION()
	void OnRep_CurrentTurnPlayerId();

	// 클라이언트에서 베팅 액션이 변경 or 시퀀스가 바뀌었을 때
	UFUNCTION()
	void OnRep_CurrentBetInfo();

	// 클라이언트에서 준비 인원(앉은 인원)이 변경되었을 때 실행
	UFUNCTION()
	void OnRep_ReadyPlayerCount();

	UFUNCTION()
	void OnRep_AlivePlayerCount();

	UFUNCTION()
	void OnRep_CurrentBulletCount();

	UFUNCTION()
	void OnRep_MainRevolverChamberCount();

	UFUNCTION()
	void OnRep_MainShotInfo();

	UFUNCTION()
	void OnRep_TimerInfo();


	UFUNCTION()
	void OnRep_LatestBetLog();



private:
	// 게임 단계에 따라 메인 리볼버 위젯의 탄창 수 표시 업데이트
	void UpdateMainRevolverWidget(int32 CurrentCount, int32 MaxCount);
	// 게임 단계에 따라 메인 리볼버 위젯의 표시 여부 제어
	void UpdateMainRevolverWidgetPhase(bool bIsPlaying);
};
