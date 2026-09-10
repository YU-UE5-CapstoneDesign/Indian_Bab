#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h" 
#include "MainGamePlayerController.generated.h"

class UMainGameWidget;
class UDeckLeftWidget;
class UInputMappingContext;
class UInputAction;

UCLASS()
class INDIAN_BAB_API AMainGamePlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:
	AMainGamePlayerController();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // 데디 AGameSession::KickPlayer(reason) → ClientWasKicked RPC 수신부.
    // 기본 구현은 no-op 이라 reason 이 폐기되며 직후 ConnectionLost 가
    // generic 문구로 덮어쓴다. 여기서 GameInstance 에 stash → OnNetworkFailure 가 소비.
    virtual void ClientWasKicked_Implementation(const FText& KickReason) override;

    void ApplyLobbyMappingContext();

    void ApplyMainGameMappingContext();

	virtual void SetupInputComponent() override;

    // 서버 호출
    UFUNCTION(BlueprintCallable)
    void RequestRaise(int32 RaiseCount);

    UFUNCTION(BlueprintCallable)
    void RequestCheckCall();

    UFUNCTION(BlueprintCallable)
    void RequestFold();

	/** Close the world-space VR menu and return to gameplay. */
	UFUNCTION(BlueprintCallable, Category = "VR UI")
	bool CloseVRMenu();

    UFUNCTION(Client, Reliable)
    void ClientOnSeated();

    UFUNCTION(Client, Reliable)
    void Client_FinishPCReady();

    UFUNCTION(Server, Reliable)
    void Server_RequestReady();

    int GetPlayerIdSafe();

private:
    // 서버로 보내는 RPC
    UFUNCTION(Server, Reliable)
    void Server_RequestBetAction(EBetAction Action, int32 RaiseCount);

    UFUNCTION(Server, Reliable)
    void Server_SetSteamNickname(const FString& NewNickname);

    UFUNCTION(Server, Reliable)
    void Server_RequestMainRevolverShot();

	// 입력 모드 전환 함수
    void EnterUIMode();     // 커서 보이기
    void EnterCameraMode(); // 커서 숨기고 회전

    // 내 스팀 닉네임 읽기
    FString GetMySteamNickname() const;

    // PS에 닉네임 보내는 함수
    void TrySendSteamNickname();

    // UI 생성/관리
    void CreateMainGameWidget();
    void CreateDeckLeftWidget();

	// 입력 바인딩 함수
    void OnMainGameLook(const FInputActionValue& Value);

    void OnMainGameRMBPressed(const FInputActionValue& Value);

    void OnMainGameRMBReleased(const FInputActionValue& Value);

    void OnMainGameCheckCall(const FInputActionValue& Value);

    void OnMainGameFold(const FInputActionValue& Value);

    void OnMainGameRaise(const FInputActionValue& Value);

    void OnLobbyMove(const FInputActionValue& Value);

    void OnLobbyLook(const FInputActionValue& Value);

    void OnMainGameTabPressed(const FInputActionValue& Value);

    void OnFire(const FInputActionValue& Value);

    void OnRightTriggerClickStarted(const FInputActionValue& Value);

    void OnRightTriggerClickReleased(const FInputActionValue& Value);

    void OnLeftTriggerClickStarted(const FInputActionValue& Value);

    void OnLeftTriggerClickReleased(const FInputActionValue& Value);

	void OnMenuToggleLeft(const FInputActionValue& Value);

    void OnMainGameInteract(const FInputActionValue& Value);

    void OnDebugRightTriggerPressed();

    void OnDebugRightTriggerReleased();

    // 플레이어 스테이트 변화 발생 시 실행(위젯에서 플레이어 스테이트 등록 실패 시 재등록)
    virtual void OnRep_PlayerState() override;

    // 감도
    float LookSensitivity = 1.0f;



    // 카메라 모드/UI 모드 관리
    bool bRMBHeld = false;

    // PS에 스팀 닉네임이 보내졌는지 확인
    bool bSteamNicknameSent = false;

   	// UI 위젯 클래스와 인스턴스 참조 변수
    UPROPERTY(EditDefaultsOnly, Category="UI")
    TSubclassOf<UMainGameWidget> MainGameWidgetClass;

    UPROPERTY()
    TObjectPtr<UMainGameWidget> MainGameWidgetInstance;

    UPROPERTY(EditDefaultsOnly, Category="UI")
    TSubclassOf<UDeckLeftWidget> DeckLeftWidgetClass;

    UPROPERTY()
    TObjectPtr<UDeckLeftWidget> DeckLeftWidgetInstance;

    // Input 관리
    UPROPERTY(EditDefaultsOnly, Category="Input")
    TObjectPtr<UInputMappingContext> MainGameMappingContext;

    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputMappingContext> LobbyMappingContext;

    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputAction> IA_MainGameRMB;

    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputAction> IA_MainGameLook;

    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputAction> IA_MainGameCheckCall;

    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputAction> IA_MainGameFold;

    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputAction> IA_MainGameRaise;

    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputAction> IA_LobbyMove;

    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputAction> IA_LobbyLook;

    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputAction> IA_MainGameTab;



    //--- 연결성(인터넷/데디 끊김) ---
    // 끊김 시 모달로 띄울 위젯 클래스 (BP에서 WBP_OfflineNotice 지정)
    UPROPERTY(EditDefaultsOnly, Category = "Connectivity")
    TSubclassOf<UUserWidget> OfflineWidgetClass;

    UPROPERTY()
    TObjectPtr<UUserWidget> OfflineWidgetInstance;

    void HandleConnectivityLost();
    void HandleConnectivityRestored();

    FDelegateHandle LostHandle;
    FDelegateHandle RestoredHandle;
    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputAction> IA_Fire;

    UPROPERTY(EditDefaultsOnly, Category = "Input|VR")
    TObjectPtr<UInputAction> IA_RightTriggerClick;

    UPROPERTY(EditDefaultsOnly, Category = "Input|VR")
    TObjectPtr<UInputAction> IA_LeftTriggerClick;

    UPROPERTY(EditDefaultsOnly, Category = "Input|VR")
    TObjectPtr<UInputAction> IA_MainGameInteract;

	UPROPERTY(EditDefaultsOnly, Category = "Input|VR")
	TObjectPtr<UInputAction> IA_MenuToggleLeft;
};
