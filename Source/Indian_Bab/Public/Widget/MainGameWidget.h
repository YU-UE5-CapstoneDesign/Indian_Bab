#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Widget/BetProgressWidget.h"
#include "MainGameWidget.generated.h"

class UButton;
class UDeckLeftWidget;
class UTextBlock;
class UEditableTextBox;
class UBetProgressWidget;
class AMainGamePlayerController;
class AMainPlayerState;

UCLASS()
class INDIAN_BAB_API UMainGameWidget : public UUserWidget
{
	GENERATED_BODY()
protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> Button_Ready;

    UFUNCTION()
    void OnReadyClicked();

    bool bPCReadyMode = false;
    bool bPCReadySubmitted = false;


	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Txt_BetLog;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableTextBox> SubRevolverCount;

	// UPROPERTY(meta = (BindWidget))
	// TObjectPtr<UEditableTextBox> Text_PlusTokenCount;

	UPROPERTY(EditAnywhere)
	int32 BetNum = 1;

	UPROPERTY(meta = (BindWidget))
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

public:
	// 플레이어 컨트롤러 및 플레이어 스테이트 등록
	void InitWidget();
	void SetPCReadyMode(bool bWaitingForReady);

	int32 GetBetNum() const;

	void UpdateCenterBetLog(const FString& Message);
	bool HandleVRClickAtWidgetLocation(const FVector2D& WidgetLocalHitLocation);

private:
	bool IsButtonUnderWidgetLocation(const UButton* Button, const FVector2D& WidgetLocalHitLocation) const;

	UFUNCTION()
	void MinusButtonClicked();

	UFUNCTION()
	void PlusButtonClicked();

	FTimerHandle BetLogTimerHandle;

	void ClearBetLog();
};