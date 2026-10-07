#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ExitConfirmWidget.generated.h"

class UButton;

/** Confirmation dialog opened by the in-game option menu's exit button. */
UCLASS()
class INDIAN_BAB_API UExitConfirmWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Yes;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_No;

	UFUNCTION()
	void OnConfirmClicked();

	UFUNCTION()
	void OnCancelClicked();
};
