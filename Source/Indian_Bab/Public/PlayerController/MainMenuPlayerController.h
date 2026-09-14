#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "MainMenuPlayerController.generated.h"


class UInputMappingContext;
class UMainMenuWidget;
class UUserWidget;
class UInputAction;
struct FInputActionValue;
class FReply;
class SWidget;

UENUM(BlueprintType)
enum class EMainMenuInputMode : uint8
{
	VR UMETA(DisplayName = "VR"),
	Mouse UMETA(DisplayName = "Mouse")
};

UCLASS()
class INDIAN_BAB_API AMainMenuPlayerController : public APlayerController
{
	GENERATED_BODY()
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void OpenMainMenu();

	/** * 생성할 메인 메뉴 위젯의 블루프린트 클래스
	 * TSubclassOf는 특정 클래스(여기서는 UUserWidget)의 자식 클래스만 할당할 수 있도록 제한
	 */
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UMainMenuWidget> MainMenuWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	EMainMenuInputMode MenuInputMode = EMainMenuInputMode::VR;

	// 생성된 메인 메뉴 위젯의 인스턴스를 저장할 변수
	UPROPERTY(VisibleInstanceOnly, Category = "UI")
	TObjectPtr<UMainMenuWidget> MainMenuWidgetInstance;

	//--- 연결성(인터넷 끊김) ---

	// 끊김 시 모달로 띄울 위젯 클래스 (BP에서 WBP_OfflineNotice 지정)
	UPROPERTY(EditDefaultsOnly, Category = "Connectivity")
	TSubclassOf<UUserWidget> OfflineWidgetClass;

	UPROPERTY()
	TObjectPtr<UUserWidget> OfflineWidgetInstance;

public:
	// [Shim] 기존 BP 호환용 — 내부적으로 MainMenuWidget::RefocusSelf로 위임.
	// SessionErrorWidget 모달 등 기존 BP가 본 함수를 호출하고 있어 잔류.
	// 신규 BP는 MainMenuWidget::RefocusSelf 를 직접 호출 권장.
	UFUNCTION(BlueprintCallable, Category = "UI")
	void RefocusMainMenu();

	void ApplyMenuInputMode(UUserWidget* FocusWidget = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Input")
	void SetMenuInputMode(EMainMenuInputMode NewInputMode);

	UFUNCTION(BlueprintPure, Category = "Input")
	bool IsMouseMenuInputMode() const { return MenuInputMode == EMainMenuInputMode::Mouse; }

	virtual void SetupInputComponent() override;

private:
    // 메뉴를 열기 전에 로컬 플레이 모드를 확정합니다.
    void BeginPlayModeSelection();
    void FinishPlayModeSelection(bool bUseVR);
    void RemovePlayModePrompt();
    FReply ChooseVRMode();
    FReply ChoosePCMode();
    TSharedPtr<SWidget> PlayModePrompt;

	void ApplyMainMenuMappingContext();
	void RemoveMainMenuMappingContext();

	void HandleConnectivityLost();
	void HandleConnectivityRestored();

	FDelegateHandle LostHandle;
	FDelegateHandle RestoredHandle;

	UPROPERTY(EditDefaultsOnly, Category="Input")
    TObjectPtr<UInputMappingContext> MainMenuMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputAction> IA_RightTriggerClick;

    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputAction> IA_LeftTriggerClick;

	void OnLeftTriggerClickStarted(const FInputActionValue& Value);
	void OnLeftTriggerClickReleased(const FInputActionValue& Value);

	void OnRightTriggerClickStarted(const FInputActionValue& Value);
	void OnRightTriggerClickReleased(const FInputActionValue& Value);
};
