#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Widget/BetProgressWidget.h"
#include "MainGameWidget.generated.h"

class UButton;
class UCheckBox;
class UDeckLeftWidget;
class UTextBlock;
class UEditableTextBox;
class UBetProgressWidget;
class UTurnInfoWidget;
class AMainGamePlayerController;
class AMainPlayerState;
class UEliminationNoticeWidget;

UENUM(BlueprintType)
enum class EVRBettingSelection : uint8
{
    None, Fold, CheckCall, Raise, AddToken
};

UCLASS()
class INDIAN_BAB_API UMainGameWidget : public UUserWidget
{
	GENERATED_BODY()
protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;
    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
        const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
        int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UEliminationNoticeWidget> EliminationNotice;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> Button_Ready;

    // 자기 턴에만 표시하는 기존 WBP 텍스트입니다.
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTurnInfoWidget> WBP_TurnInfoWidget;

    UFUNCTION()
    void OnReadyClicked();

    // PC와 VR 모두 같은 턴 조건으로 베팅 버튼을 갱신합니다.
    bool IsOwningPlayerTurn() const;
    bool CanUseBettingButtons() const;
    int32 GetMaxRaiseCount() const;
    void RefreshRaiseSelection();
    void RefreshBettingButtons();

    bool bPCReadyMode = false;
    bool bPCReadySubmitted = false;


	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Txt_BetLog;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableTextBox> SubRevolverCount;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCheckBox> CheckBox_133;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_AddTokenCount;

	UPROPERTY(EditAnywhere)
	int32 BetNum = 1;

	// Optional while existing Blueprint timer calls are retained for asset compatibility.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Time;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> BetCount;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Plus_Button;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Minus_Button;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Raise;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_CheckCall;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Fold;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBetProgressWidget> WBP_BetProgress;

	UFUNCTION(BlueprintCallable)
	void OperateTimer();

	// 플레이어 컨트롤러 참조 변수
	TObjectPtr<AMainGamePlayerController> MainGamePC;

	// 플레이어 스테이트 참조 변수
	UPROPERTY()
	TObjectPtr<AMainPlayerState> MainPS;

	// 레이즈 버튼 클릭 시
	UFUNCTION()
	void OnButtonRaise();

	// 체크/콜 버튼 클릭 시
	UFUNCTION()
	void OnButtonCheckCall();

	// 폴드 버튼 클릭 시
	UFUNCTION()
	void OnButtonFold();

	UFUNCTION()
	void UpdateSubRevolverCount(int32 Count);

	UFUNCTION()
	void OnAddTokenCheckStateChanged(bool bIsChecked);

	bool CanUseAddToken() const;
	void RefreshAddTokenUI();
	bool bUpdatingAddTokenUI = false;
	bool bAddTokenChangedBySlate = false;

public:
    void SetVRBettingNavigationEnabled(bool bEnabled);
    void MoveVRBettingSelection(int32 Direction);
    void ConfirmVRBettingSelection();
    void AdjustVRRaise(int32 Direction);

    UFUNCTION(BlueprintPure, Category = "VR|Betting")
    EVRBettingSelection GetVRBettingSelection() const { return VRBettingSelection; }

protected:
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "VR|Betting")
    FLinearColor VRSelectionColor = FLinearColor(1.0f, 0.7f, 0.05f, 1.0f);
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "VR|Betting", meta = (ClampMin = "1.0"))
    float VRSelectionThickness = 3.0f;

    UFUNCTION(BlueprintImplementableEvent, Category = "VR|Betting")
    void OnVRBettingSelectionChanged(EVRBettingSelection Selection);

public:
	// 플레이어 컨트롤러 및 플레이어 스테이트 등록
	void InitWidget();
	void SetPCReadyMode(bool bWaitingForReady);
	void SetEliminationNotice(bool bVisible, bool bSpectating);

	int32 GetBetNum() const;

	void UpdateCenterBetLog(const FString& Message);
	bool HandleVRClickAtWidgetLocation(const FVector2D& WidgetLocalHitLocation);

private:
    friend class FVRBettingNavigationTest;
    bool CanSelectVRBettingAction(EVRBettingSelection Selection) const;
    UWidget* GetVRBettingSelectionWidget() const;
    void RefreshVRBettingSelection();
    void SetVRBettingSelection(EVRBettingSelection Selection);
    bool bVRBettingNavigationEnabled = false;
    bool bVRWasOwningPlayerTurn = false;
    EVRBettingSelection VRBettingSelection = EVRBettingSelection::None;
    uint64 LastVRConfirmationFrame = MAX_uint64;
    TOptional<bool> PendingVRTokenSelection;
    double VRTokenRequestTime = 0.0;

	bool IsButtonUnderWidgetLocation(const UButton* Button, const FVector2D& WidgetLocalHitLocation) const;
	bool IsCheckBoxUnderWidgetLocation(const UCheckBox* CheckBox, const FVector2D& WidgetLocalHitLocation) const;

	UFUNCTION()
	void MinusButtonClicked();

	UFUNCTION()
	void PlusButtonClicked();

	FTimerHandle BetLogTimerHandle;

	void ClearBetLog();
};
