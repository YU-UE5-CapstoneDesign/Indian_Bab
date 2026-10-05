#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OptionMenuWidget.generated.h"


class UButton;
class USlider;
class UComboBoxString;
class USettingSubsystem;
class UPlayerController;
class UConfirmChangesWidget;


/**
 * C++에서 설정을 임시(Staged)로 관리하기 위한 구조체
 * 15개의 개별 변수 대신 이 구조체 3개(Staged, Saved, Default)를 사용
 */
USTRUCT(BlueprintType)
struct FDisplaySettings
{
	GENERATED_BODY()

	UPROPERTY() float MasterVolume = 1.f;
	UPROPERTY() float MouseSensitivity = 1.f;
	UPROPERTY() float ScreenBrightness = 1.f;
	UPROPERTY() int32 ResolutionIndex = 0;
	UPROPERTY() int32 WindowModeIndex = 0;

	// 두 설정이 동일한지 비교하기 위한 연산자
	bool operator==(const FDisplaySettings& Other) const
	{
		return FMath::IsNearlyEqual(MasterVolume, Other.MasterVolume) &&
			FMath::IsNearlyEqual(MouseSensitivity, Other.MouseSensitivity) &&
			FMath::IsNearlyEqual(ScreenBrightness, Other.ScreenBrightness) &&
			ResolutionIndex == Other.ResolutionIndex &&
			WindowModeIndex == Other.WindowModeIndex;
	}
};

/**
 * 옵션 메뉴 (WBP_OptionMenu)의 C++ 기반 클래스
 */
UCLASS()
class INDIAN_BAB_API UOptionMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * 이 위젯을 생성한 부모 위젯(MainMenu, PauseMenu)을 설정
	 * '확인'/'취소' 시 부모 위젯을 다시 보이게 하기 위해 필수
	 */
	void SetParentMenu(UUserWidget* InParentMenu);

	UFUNCTION(BlueprintCallable, Category = "Option Menu")
	UUserWidget* GetParentMenu() const { return ParentMenu; }

	// [추가] WBP_ConfirmChanges 팝업의 '예' 버튼에서 호출
	UFUNCTION(BlueprintCallable, Category = "Option Menu")
	void OnConfirmChangesYes();

	// [추가] WBP_ConfirmChanges 팝업의 '아니오' 버튼에서 호출
	UFUNCTION(BlueprintCallable, Category = "Option Menu")
	void OnConfirmChangesNo();

	// 키보드 포커스가 다른 위젯으로 옮겨가도 기존 확인 동작으로 옵션을 닫습니다.
	void HandleEscape();

protected:
	// '변경사항 저장' 팝업을 염 (블루프린트에서 VR용으로 띄우기 위해 이벤트로 변경)
	UFUNCTION(BlueprintImplementableEvent, Category = "Option Menu")
	void ShowConfirmChangesPopup();

	// 위젯 생성 시 (EventConstruct) 호출
	virtual void NativeConstruct() override;

	// 캐시 인스턴스 재오픈 시 NativeConstruct 재진입 → 중복 바인딩 ensure 방지용 정리.
	virtual void NativeDestruct() override;

	/**
	 * 이 위젯이 포커스를 가지고 있을 때 키 입력을 받음 (ESC 키 처리용)
	 */
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	/**
	 * [추가] 이 위젯에서 마우스 버튼이 눌렸을 때 호출 (포커스 유지용)
	 * 이 위젯의 '배경'을 클릭해도 포커스를 잃지 않도록 함
	 */
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	//--- BP 위젯 변수 바인딩 ---

	UPROPERTY(meta = (BindWidget)) TObjectPtr<USlider> Slider_MasterVolume;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<USlider> Slider_MouseSensitivity;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<USlider> Slider_ScreenBrightness;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UComboBoxString> ComboBoxString_ScreenResolution;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UComboBoxString> ComboBoxString_WindowMode;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> Button_Apply;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> Button_ResetAll;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> Button_OK;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> Button_Cancel;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> Button_ExitGame;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> Button_ResetMasterVolume;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> Button_ResetMouseSensitivity;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> Button_ResetScreenBrightness;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> Button_ResetScreenResolution;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> Button_ResetWindowMode;

	//--- C++ 이벤트 핸들러 ---
	// 슬라이더 및 콤보박스 값 변경
	UFUNCTION() void OnMasterVolumeChanged(float Value);
	UFUNCTION() void OnMouseSensitivityChanged(float Value);
	UFUNCTION() void OnScreenBrightnessChanged(float Value);
	UFUNCTION() void OnScreenResolutionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION() void OnWindowModeChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	// 메인 버튼 클릭
	UFUNCTION() void OnApplyClicked();
	UFUNCTION() void OnResetAllClicked();
	UFUNCTION() void OnOKClicked();
	UFUNCTION() void OnCancelClicked();
	UFUNCTION() void OnExitGameClicked();

	// 개별 초기화 버튼 클릭
	UFUNCTION() void OnResetMasterVolumeClicked();
	UFUNCTION() void OnResetMouseSensitivityClicked();
	UFUNCTION() void OnResetScreenBrightnessClicked();
	UFUNCTION() void OnResetScreenResolutionClicked();
	UFUNCTION() void OnResetWindowModeClicked();

	//--- C++ 핵심 로직 함수 ---

	// SettingSubsystem에서 값을 로드하고 UI를 초기화
	void InitializeSettings();

	// StagedSettings 값을 실제 UI(슬라이더, 콤보박스)에 적용
	void PopulateUIFromStagedSettings();

	/** * Staged/Saved/Default 값을 비교하여 모든 버튼('적용', '초기화' 등)의
	 * 활성화(Enabled) 및 가시성(Visibility) 상태를 업데이트
	 */
	void UpdateUIState();

	// bSaveChanges=true면 '적용'을, false면 '취소' 로직을 수행하고 메뉴를 닫음
	void CloseMenu(bool bSaveChanges);

	

	//--- 상태 변수 ---
	UPROPERTY() TObjectPtr<USettingSubsystem> SettingSubsystem;
	UPROPERTY() TObjectPtr<UUserWidget> ParentMenu;
	UPROPERTY() TObjectPtr<APlayerController> PlayerControllerRef;

	// UI에 표시된 (아직 저장되지 않은) 임시 값
	FDisplaySettings StagedSettings;
	// 현재 디스크에 저장된 값
	FDisplaySettings SavedSettings;
	// 게임의 하드코딩된 기본값
	FDisplaySettings DefaultSettings;

	// 콤보박스 채우기용 데이터
	TArray<FString> AvailableResolutionStrings;
	TArray<FString> AvailableWindowModeStrings = { TEXT("전체 화면"), TEXT("테두리 없는 창모드"), TEXT("창 모드") };

	// '확인' 시 변경사항이 있을 때 띄울 확인 팝업 클래스
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UConfirmChangesWidget> ConfirmChangesPopupClass;
};
