#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RoomCreateWidget.generated.h"


class UButton;
class UTextBlock;
class UEditableTextBox;
class UCheckBox;
class USessionSubsystem;


UCLASS()
class INDIAN_BAB_API URoomCreateWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetParentMenu(UUserWidget* InParentMenu);

protected:
	// 위젯 생성 시 (EventConstruct) 호출
	virtual void NativeConstruct() override;

	// [추가] 위젯이 제거될 때 호출 (이벤트 해제용)
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
	UPROPERTY() 
	TObjectPtr<USessionSubsystem> SessionSubsystem;

	UPROPERTY(meta = (BindWidget)) 
	TObjectPtr<UButton> Button_Yes;
	UPROPERTY(meta = (BindWidget)) 
	TObjectPtr<UButton> Button_No;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Status;
	// 체크 시 FriendsOnly, 미체크 시 Public — 기본 unchecked = Public
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCheckBox> CheckBox_FriendsOnly;
	//--- 상태 변수 ---
	UPROPERTY() 
	TObjectPtr<APlayerController> PlayerControllerRef;
	UPROPERTY() 
	TObjectPtr<UUserWidget> ParentMenu;

	// 로비 레벨 이름
	UPROPERTY(EditAnywhere) 
	FName LobbyLevelName;

	UFUNCTION() 
	void OnYesClicked();
	UFUNCTION() 
	void OnNoClicked();

	// 에러 발생 시 자기 자신을 닫는 콜백 — 실제 사유 표시는 MainMenuPC의 모달이 담당.
	// 성공 케이스는 ClientTravel로 위젯이 자동 소멸하므로 별도 처리 불필요.
	UFUNCTION()
	void OnSessionError(const FString& Reason);
};
