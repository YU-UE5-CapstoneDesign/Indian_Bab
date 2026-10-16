#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Engine/EngineTypes.h"
#include "Engine/NetworkDelegates.h"
#include "IndianBabGameInstance.generated.h"

class UNetDriver;
class UAudioComponent;
class USoundBase;

/**
 * 프로젝트 커스텀 GameInstance.
 *
 * 주 책임: UE 엔진의 NetworkFailure / TravelFailure 콜백을 받아서
 *         UConnectivitySubsystem::ForceTriggerLost 로 합류시킨다.
 *         (인터넷 단절 외에 데디 다운/타임아웃 케이스도 동일 흐름 처리)
 *
 * DefaultEngine.ini 의 [/Script/EngineSettings.GameMapsSettings] 또는
 * [/Script/Engine.GameInstance] 에 GameInstanceClass 로 등록되어야 활성됨.
 */
UCLASS()
class INDIAN_BAB_API UIndianBabGameInstance : public UGameInstance
{
    GENERATED_BODY()

public:
    UIndianBabGameInstance();

    // 실행 중 선택한 모드를 맵 이동 후에도 유지합니다.
    bool HasSelectedPlayMode() const { return bPlayModeSelected; }
    bool IsVRPlayMode() const { return bUseVRPlayMode; }
    void SetSelectedPlayMode(bool bUseVR);
    TSubclassOf<APawn> GetPlayModePawnClass(bool bUseVR) const;

    virtual void Init() override;
    virtual void Shutdown() override;

    // 메인 메뉴 및 로비 배경음악
    UFUNCTION(BlueprintCallable, Category = "BGM")
    void PlayMainBGM();

    // 실제 게임 진행 배경음악
    UFUNCTION(BlueprintCallable, Category = "BGM")
    void PlayGameBGM();

    // 최종 승리 배경음악
    UFUNCTION(BlueprintCallable, Category = "BGM")
    void PlayVictoryBGM();

    // 기존 옵션 메뉴의 볼륨 설정 적용
    UFUNCTION(BlueprintCallable, Category = "BGM")
    void SetBGMVolume(float NewVolume);

    // 폴드 횟수에 따라 심장소리 시작
    UFUNCTION(BlueprintCallable, Category = "BGM|Fold")
    void BeginFoldAudio(int32 FoldCount);

    // 서브 리볼버 격발 완료
    // 생존 시 한숨소리 재생
    UFUNCTION(BlueprintCallable, Category = "BGM|Fold")
    void EndFoldAudio(bool bSurvived);


    // 첫 번째~세 번째 폴드에서 사용
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "BGM|Fold")
    TObjectPtr<USoundBase> SlowHeartbeatSound = nullptr;

    // 네 번째 이후 폴드에서 사용
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "BGM|Fold")
    TObjectPtr<USoundBase> FastHeartbeatSound = nullptr;

    // 생존했을 때 사용할 한숨소리
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "BGM|Fold")
    TObjectPtr<USoundBase> SighSound = nullptr;

    // 블루프린트에서 음악 에셋 지정
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "BGM")
    TObjectPtr<USoundBase> MainBGMSound = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "BGM")
    TObjectPtr<USoundBase> GameBGMSound = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "BGM")
    TObjectPtr<USoundBase> VictoryBGMSound = nullptr;

    // 서버가 ClientWasKicked RPC 로 보낸 사유를 클라 측에서 잠시 stash.
    // 직후 NetConnection close 로 OnNetworkFailure(ConnectionLost) 가 떨어지면
    // 그쪽에서 Consume 해서 generic "게임 서버에 연결할 수 없습니다…" 대신
    // 실제 킥 사유를 모달에 노출.
    void SetPendingKickReason(const FString& Reason) { PendingKickReason = Reason; }
    FString ConsumePendingKickReason()
    {
        FString Out = MoveTemp(PendingKickReason);
        PendingKickReason.Reset();
        return Out;
    }

private:
    UPROPERTY(EditDefaultsOnly, Category = "Play Mode")
    TSoftClassPtr<APawn> PCCharacterClass;

    UPROPERTY(EditDefaultsOnly, Category = "Play Mode")
    TSoftClassPtr<APawn> VRCharacterClass;

    bool bPlayModeSelected = false;
    bool bUseVRPlayMode = false;

    void OnNetworkFailure(UWorld* World, UNetDriver* NetDriver,
        ENetworkFailure::Type FailureType, const FString& ErrorString);

    void OnTravelFailure(UWorld* World,
        ETravelFailure::Type FailureType, const FString& ErrorString);

    FDelegateHandle NetworkFailureHandle;
    FDelegateHandle TravelFailureHandle;

    // ClientWasKicked override 가 채우고 OnNetworkFailure 가 비움.
    FString PendingKickReason;

    // 현재 재생 중인 음악 종류
    enum class EBGMTrack : uint8
    {
        None,
        Main,
        Game,
        Victory
    };

    // 현재 음악을 재생하는 AudioComponent
    UPROPERTY(Transient)
    TObjectPtr<UAudioComponent> ActiveBGM = nullptr;

    EBGMTrack ActiveTrack = EBGMTrack::None;

    // 현재 폴드 심장소리
    UPROPERTY(Transient)
    TObjectPtr<UAudioComponent> FoldHeartbeatAudio = nullptr;

    // 심장소리 재생 상태
    bool bFoldAudioActive = false;

    // 실제 BGM 재생 처리 함수
    void PlayBGM(USoundBase* Sound, EBGMTrack NewTrack);
};
