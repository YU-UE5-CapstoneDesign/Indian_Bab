#include "Widget/OptionMenuWidget.h"
#include "GameInstanceSubsystem/SettingSubsystem.h"
#include "Components/Button.h"
#include "Components/Slider.h"
#include "Components/ComboBoxString.h"
#include "Kismet/GameplayStatics.h"
#include "Widget/ConfirmChangesWidget.h"
#include "Widget/MainMenuWidget.h"
#include "PlayerController/MainMenuPlayerController.h"
#include "PlayerController/MainGamePlayerController.h"
#include "Framework/Application/SlateApplication.h"


void UOptionMenuWidget::SetParentMenu(UUserWidget* InParentMenu)
{
	ParentMenu = InParentMenu;
}


void UOptionMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);

	// SettingSubsystem 캐시
	SettingSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<USettingSubsystem>() : nullptr;
	if (!SettingSubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("OptionMenuWidget: USettingSubsystem을 찾을 수 없습니다!"));
		return;
	}

	// 플레이어 컨트롤러 캐시
	PlayerControllerRef = GetOwningPlayer();
	Button_ExitGame->SetVisibility(
		Cast<AMainGamePlayerController>(PlayerControllerRef)
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed);
	Button_ExitGame->SetIsEnabled(true);

	// --- 모든 UI 이벤트에 C++ 함수 바인딩 ---
	// 슬라이더
	Slider_MasterVolume->OnValueChanged.AddDynamic(this, &UOptionMenuWidget::OnMasterVolumeChanged);
	Slider_MouseSensitivity->OnValueChanged.AddDynamic(this, &UOptionMenuWidget::OnMouseSensitivityChanged);
	Slider_ScreenBrightness->OnValueChanged.AddDynamic(this, &UOptionMenuWidget::OnScreenBrightnessChanged);
	// 콤보박스
	ComboBoxString_ScreenResolution->OnSelectionChanged.AddDynamic(this, &UOptionMenuWidget::OnScreenResolutionChanged);
	ComboBoxString_WindowMode->OnSelectionChanged.AddDynamic(this, &UOptionMenuWidget::OnWindowModeChanged);
	// 메인 버튼
	Button_Apply->OnClicked.AddDynamic(this, &UOptionMenuWidget::OnApplyClicked);
	Button_ResetAll->OnClicked.AddDynamic(this, &UOptionMenuWidget::OnResetAllClicked);
	Button_OK->OnClicked.AddDynamic(this, &UOptionMenuWidget::OnOKClicked);
	Button_Cancel->OnClicked.AddDynamic(this, &UOptionMenuWidget::OnCancelClicked);
	Button_ExitGame->OnClicked.AddDynamic(this, &UOptionMenuWidget::OnExitGameClicked);

	// 개별 리셋 버튼
	Button_ResetMasterVolume->OnClicked.AddDynamic(this, &UOptionMenuWidget::OnResetMasterVolumeClicked);
	Button_ResetMouseSensitivity->OnClicked.AddDynamic(this, &UOptionMenuWidget::OnResetMouseSensitivityClicked);
	Button_ResetScreenBrightness->OnClicked.AddDynamic(this, &UOptionMenuWidget::OnResetScreenBrightnessClicked);
	Button_ResetScreenResolution->OnClicked.AddDynamic(this, &UOptionMenuWidget::OnResetScreenResolutionClicked);
	Button_ResetWindowMode->OnClicked.AddDynamic(this, &UOptionMenuWidget::OnResetWindowModeClicked);

	// --- 콤보박스 채우기 ---
	// 해상도
	AvailableResolutionStrings = SettingSubsystem->GetSupportedScreenResolutions();
	ComboBoxString_ScreenResolution->ClearOptions();
	for (const FString& ResString : AvailableResolutionStrings)
	{
		ComboBoxString_ScreenResolution->AddOption(ResString);
	}
	// 창 모드
	ComboBoxString_WindowMode->ClearOptions();
	for (const FString& ModeString : AvailableWindowModeStrings)
	{
		ComboBoxString_WindowMode->AddOption(ModeString);
	}

	// 설정값 로드 및 UI 초기화
	InitializeSettings();
}


// 캐시 인스턴스 재오픈 대비 — BindWidget UObject들의 누적 바인딩 일괄 해제.
void UOptionMenuWidget::NativeDestruct()
{
	// 슬라이더
	if (Slider_MasterVolume)     Slider_MasterVolume->OnValueChanged.RemoveAll(this);
	if (Slider_MouseSensitivity) Slider_MouseSensitivity->OnValueChanged.RemoveAll(this);
	if (Slider_ScreenBrightness) Slider_ScreenBrightness->OnValueChanged.RemoveAll(this);

	// 콤보박스
	if (ComboBoxString_ScreenResolution) ComboBoxString_ScreenResolution->OnSelectionChanged.RemoveAll(this);
	if (ComboBoxString_WindowMode)       ComboBoxString_WindowMode->OnSelectionChanged.RemoveAll(this);

	// 메인 버튼
	if (Button_Apply)    Button_Apply->OnClicked.RemoveAll(this);
	if (Button_ResetAll) Button_ResetAll->OnClicked.RemoveAll(this);
	if (Button_OK)       Button_OK->OnClicked.RemoveAll(this);
	if (Button_Cancel)   Button_Cancel->OnClicked.RemoveAll(this);
	if (Button_ExitGame) Button_ExitGame->OnClicked.RemoveAll(this);

	// 개별 리셋 버튼
	if (Button_ResetMasterVolume)     Button_ResetMasterVolume->OnClicked.RemoveAll(this);
	if (Button_ResetMouseSensitivity) Button_ResetMouseSensitivity->OnClicked.RemoveAll(this);
	if (Button_ResetScreenBrightness) Button_ResetScreenBrightness->OnClicked.RemoveAll(this);
	if (Button_ResetScreenResolution) Button_ResetScreenResolution->OnClicked.RemoveAll(this);
	if (Button_ResetWindowMode)       Button_ResetWindowMode->OnClicked.RemoveAll(this);

	Super::NativeDestruct();
}


/**
 * ESC 키 입력을 처리하는 함수
 */
FReply UOptionMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// ESC 키가 눌렸는지 확인
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
#if WITH_EDITOR
		if (!InKeyEvent.IsShiftDown()) return FReply::Unhandled();
#endif
		UE_LOG(LogTemp, Log, TEXT("OptionMenuWidget: ESC key pressed."));

		// '확인' 버튼을 누른 것과 동일하게 동작
		// (변경 사항이 있으면 팝업을 띄우고, 없으면 그냥 닫습니다)
		HandleEscape();

		// [중요] ESC 키 입력을 '처리했음(Handled)'으로 반환
		// 이렇게 해야 입력이 PlayerController로 전파되어
		// PauseMenu가 닫히는 것을 막을 수 있음
		return FReply::Handled();
	}

	// 다른 키가 눌렸다면 '처리 안 함(Unhandled)'으로 반환
	return FReply::Unhandled();
}

void UOptionMenuWidget::HandleEscape()
{
	OnOKClicked();
}


/**
 * '배경' 클릭 시 포커스를 잃지 않도록 처리하는 함수
 */
FReply UOptionMenuWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// 기본 마우스 로직을 먼저 호출 (예: 버튼 클릭)
	FReply Reply = Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);

	// VR 모드에서 SetInputMode(UIOnly)는 WidgetInteractionComponent 시뮬 키 라우팅을 깨뜨림 — Mouse 모드만 포커스 회복 적용
	const AMainMenuPlayerController* MainMenuPC = Cast<AMainMenuPlayerController>(PlayerControllerRef);
	if (MainMenuPC && !MainMenuPC->IsMouseMenuInputMode())
	{
		return FReply::Handled();
	}

	// 팝업이 없고, 플레이어 컨트롤러가 유효하다면
	// (사용자가 이 위젯의 빈 공간(배경)을 클릭했더라도)
	// 이 위젯에게 다시 키보드 포커스를 강제로 설정
	if (PlayerControllerRef)
	{
		if (Cast<AMainGamePlayerController>(PlayerControllerRef))
		{
			// The controller already selected GameAndUI; a background click only restores focus.
			return FReply::Handled().SetUserFocus(TakeWidget());
		}

		FInputModeUIOnly InputModeData;
		InputModeData.SetWidgetToFocus(this->TakeWidget()); // 포커스를 이 위젯으로
		PlayerControllerRef->SetInputMode(InputModeData);
	}

	// FReply::Handled()를 반환
	// 이 클릭을 "처리했음(Handled)"으로 표시하여,
	// 클릭 이벤트가 뷰포트로 "떨어지는(fall-through)" 것을 막음
	return FReply::Handled();
}


// 설정값 로드 및 UI 초기화
void UOptionMenuWidget::InitializeSettings()
{
	if (!SettingSubsystem) return;

	// Subsystem에서 Saved/Default 값 로드
	SavedSettings.MasterVolume = SettingSubsystem->GetMasterVolume();
	SavedSettings.MouseSensitivity = SettingSubsystem->GetMouseSensitivity();
	SavedSettings.ScreenBrightness = SettingSubsystem->GetScreenBrightness();
	SavedSettings.ResolutionIndex = SettingSubsystem->GetResolutionIndex();
	SavedSettings.WindowModeIndex = SettingSubsystem->GetWindowModeIndex();

	DefaultSettings.MasterVolume = SettingSubsystem->GetDefaultMasterVolume();
	DefaultSettings.MouseSensitivity = SettingSubsystem->GetDefaultMouseSensitivity();
	DefaultSettings.ScreenBrightness = SettingSubsystem->GetDefaultScreenBrightness();
	DefaultSettings.ResolutionIndex = SettingSubsystem->GetDefaultResolutionIndex();
	DefaultSettings.WindowModeIndex = SettingSubsystem->GetDefaultWindowModeIndex();

	// Staged 값을 Saved 값으로 초기화
	StagedSettings = SavedSettings;

	// 로드한 Staged 값을 UI에 적용
	PopulateUIFromStagedSettings();

	// 버튼 상태(활성화/비활성화) 첫 업데이트
	UpdateUIState();
}


// Staged (데이터) -> UI (슬라이더/콤보박스)
void UOptionMenuWidget::PopulateUIFromStagedSettings()
{
	Slider_MasterVolume->SetValue(StagedSettings.MasterVolume);
	Slider_MouseSensitivity->SetValue(StagedSettings.MouseSensitivity);
	Slider_ScreenBrightness->SetValue(StagedSettings.ScreenBrightness);
	ComboBoxString_ScreenResolution->SetSelectedIndex(StagedSettings.ResolutionIndex);
	ComboBoxString_WindowMode->SetSelectedIndex(StagedSettings.WindowModeIndex);
}


// UI (슬라이더/콤보박스) -> Staged (데이터)
void UOptionMenuWidget::OnMasterVolumeChanged(float Value)
{
	StagedSettings.MasterVolume = Value;
	UpdateUIState();
}


void UOptionMenuWidget::OnMouseSensitivityChanged(float Value)
{
	StagedSettings.MouseSensitivity = Value;
	UpdateUIState();
}


void UOptionMenuWidget::OnScreenBrightnessChanged(float Value)
{
	StagedSettings.ScreenBrightness = Value;
	UpdateUIState();
}


void UOptionMenuWidget::OnScreenResolutionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	StagedSettings.ResolutionIndex = ComboBoxString_ScreenResolution->GetSelectedIndex();
	UpdateUIState();
}


void UOptionMenuWidget::OnWindowModeChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	StagedSettings.WindowModeIndex = ComboBoxString_WindowMode->GetSelectedIndex();
	UpdateUIState();
}


// 버튼 상태 갱신
void UOptionMenuWidget::UpdateUIState()
{
	if (!SettingSubsystem) return;

	// CanApply / CanReset
	const bool bCanApply = !(StagedSettings == SavedSettings);
	const bool bCanReset = !(StagedSettings == DefaultSettings);

	Button_Apply->SetIsEnabled(bCanApply);
	Button_ResetAll->SetIsEnabled(bCanReset);

	// CanReset... (개별 초기화 버튼 가시성)
	Button_ResetMasterVolume->SetVisibility(StagedSettings.MasterVolume != DefaultSettings.MasterVolume ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
	Button_ResetMouseSensitivity->SetVisibility(StagedSettings.MouseSensitivity != DefaultSettings.MouseSensitivity ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
	Button_ResetScreenBrightness->SetVisibility(StagedSettings.ScreenBrightness != DefaultSettings.ScreenBrightness ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
	Button_ResetScreenResolution->SetVisibility(StagedSettings.ResolutionIndex != DefaultSettings.ResolutionIndex ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
	Button_ResetWindowMode->SetVisibility(StagedSettings.WindowModeIndex != DefaultSettings.WindowModeIndex ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
}


// 메인 버튼 로직
void UOptionMenuWidget::OnApplyClicked()
{
	// Staged -> Saved
	SavedSettings = StagedSettings;

	// Staged -> Subsystem
	SettingSubsystem->SetMasterVolume(StagedSettings.MasterVolume);
	SettingSubsystem->SetMouseSensitivity(StagedSettings.MouseSensitivity);
	SettingSubsystem->SetScreenBrightness(StagedSettings.ScreenBrightness);
	SettingSubsystem->SetResolutionIndex(StagedSettings.ResolutionIndex);
	SettingSubsystem->SetWindowModeIndex(StagedSettings.WindowModeIndex);

	// Subsystem에 적용 및 저장 요청
	SettingSubsystem->ApplySettings();
	SettingSubsystem->SaveSettings();

	// '적용' 버튼이 비활성화되도록 UI 상태 갱신
	UpdateUIState();
}


void UOptionMenuWidget::OnResetAllClicked()
{
	// Default -> Staged
	StagedSettings = DefaultSettings;
	// Staged -> UI
	PopulateUIFromStagedSettings();
	// UI 상태 갱신
	UpdateUIState();
}


// '확인' 버튼 로직
void UOptionMenuWidget::OnOKClicked()
{
	// Staged != Saved (변경 사항이 있는가?)
	const bool bHasUnsavedChanges = !(StagedSettings == SavedSettings);

	if (bHasUnsavedChanges)
	{
		// '변경사항 저장' 팝업을 띄움
		ShowConfirmChangesPopup();
	}
	else
	{
		// 변경 사항이 없으므로 '취소'와 동일하게 그냥 닫음
		CloseMenu(false); // false = 저장 안 함
	}
}


void UOptionMenuWidget::OnCancelClicked()
{
	// 변경 사항 무시하고 닫기
	CloseMenu(false);
}

void UOptionMenuWidget::OnExitGameClicked()
{
	if (AMainGamePlayerController* MainGamePC = Cast<AMainGamePlayerController>(PlayerControllerRef))
	{
		MainGamePC->ShowExitConfirmation();
	}
}


// CloseMenu 로직
void UOptionMenuWidget::CloseMenu(bool bSaveChanges)
{
	if (bSaveChanges)
	{
		// '적용' 버튼이 활성화된 경우 (Staged != Saved)
		if (!(StagedSettings == SavedSettings))
		{
			OnApplyClicked(); // 저장 로직 실행
		}
	}

	// The in-game option menu lives inside the world-space VR master menu.
	// Closing it must resume gameplay instead of returning to the lobby main-menu page.
	if (AMainGamePlayerController* MainGamePC = Cast<AMainGamePlayerController>(PlayerControllerRef))
	{
		if (MainGamePC->CloseVRMenu())
		{
			return;
		}
		RemoveFromParent();
		MainGamePC->ResumeAfterOptionsMenu();
		return;
	}

	if (UMainMenuWidget* MainMenu = Cast<UMainMenuWidget>(ParentMenu))
	{
		if (!IsInViewport())
		{
			MainMenu->ShowMainMenuRoot();
			return;
		}
	}

	// 부모 메뉴가 있다면 다시 보이게 함
	if (ParentMenu)
	{
		// 부모 위젯(PauseMenu)으로 돌아가기 위해 GameAndUI 모드로 '복원' 및 포커싱 재설정
		if (PlayerControllerRef)
		{
			FInputModeGameAndUI InputModeData;
			InputModeData.SetWidgetToFocus(ParentMenu->TakeWidget());
			InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::LockInFullscreen);
			PlayerControllerRef->SetInputMode(InputModeData);
		}
	}
	// 자신을 뷰포트에서 제거
	RemoveFromParent();
}


// 개별 리셋 버튼 로직
void UOptionMenuWidget::OnResetMasterVolumeClicked()
{
	StagedSettings.MasterVolume = DefaultSettings.MasterVolume;
	PopulateUIFromStagedSettings();
	UpdateUIState();
}


void UOptionMenuWidget::OnResetMouseSensitivityClicked()
{
	StagedSettings.MouseSensitivity = DefaultSettings.MouseSensitivity;
	PopulateUIFromStagedSettings();
	UpdateUIState();
}


void UOptionMenuWidget::OnResetScreenBrightnessClicked()
{
	StagedSettings.ScreenBrightness = DefaultSettings.ScreenBrightness;
	PopulateUIFromStagedSettings();
	UpdateUIState();
}


void UOptionMenuWidget::OnResetScreenResolutionClicked()
{
	StagedSettings.ResolutionIndex = DefaultSettings.ResolutionIndex;
	PopulateUIFromStagedSettings();
	UpdateUIState();
}


void UOptionMenuWidget::OnResetWindowModeClicked()
{
	StagedSettings.WindowModeIndex = DefaultSettings.WindowModeIndex;
	PopulateUIFromStagedSettings();
	UpdateUIState();
}


// 팝업 관련 함수들

// '변경사항 저장' 팝업을 염
/*
void UOptionMenuWidget::ShowConfirmChangesPopup()
{
	if (!ConfirmChangesPopupClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("OptionMenuWidget: ConfirmChangesPopupClass가 설정되지 않았습니다! (BP에서 설정 필요). '확인'을 '적용'으로 간주합니다."));
		// 팝업이 없으면, 그냥 저장하고 닫음
		CloseMenu(true); // true = 저장 함
		return;
	}

	if (PlayerControllerRef)
	{
		// 팝업 위젯을 생성 (Owner = this)
		UConfirmChangesWidget* ConfirmPopup = CreateWidget<UConfirmChangesWidget>(this, ConfirmChangesPopupClass);
		if (ConfirmPopup)
		{
			ConfirmPopup->SetParentMenu(this); // 자신을 부모 메뉴로 설정
			ConfirmPopup->AddToViewport();

			// 팝업이 열릴 때 입력을 'UIOnly'로 변경하고 팝업에 포커스
			FInputModeUIOnly InputModeData;
			InputModeData.SetWidgetToFocus(ConfirmPopup->TakeWidget());
			InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::LockInFullscreen);
			PlayerControllerRef->SetInputMode(InputModeData);
		}
	}
}
*/

// WBP_ConfirmChanges 팝업의 '예' 버튼에서 호출
void UOptionMenuWidget::OnConfirmChangesYes()
{
	// 변경 사항을 저장하고 닫음
	CloseMenu(true); // true = 저장 함
}


// WBP_ConfirmChanges 팝업의 '아니오' 버튼에서 호출
void UOptionMenuWidget::OnConfirmChangesNo()
{
	// 변경 사항을 저장하지 않고 닫음
	CloseMenu(false); // false = 저장 안 함
}
