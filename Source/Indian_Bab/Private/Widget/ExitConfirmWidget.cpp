#include "Widget/ExitConfirmWidget.h"

#include "Components/Button.h"
#include "InputCoreTypes.h"
#include "PlayerController/MainGamePlayerController.h"

void UExitConfirmWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);

	Button_Yes->OnClicked.AddDynamic(this, &UExitConfirmWidget::OnConfirmClicked);
	Button_No->OnClicked.AddDynamic(this, &UExitConfirmWidget::OnCancelClicked);
}

void UExitConfirmWidget::NativeDestruct()
{
	Button_Yes->OnClicked.RemoveAll(this);
	Button_No->OnClicked.RemoveAll(this);
	Super::NativeDestruct();
}

FReply UExitConfirmWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
#if WITH_EDITOR
		if (!InKeyEvent.IsShiftDown()) return FReply::Unhandled();
#endif
		OnCancelClicked();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UExitConfirmWidget::OnConfirmClicked()
{
	if (AMainGamePlayerController* PC = Cast<AMainGamePlayerController>(GetOwningPlayer()))
	{
		PC->ConfirmExitFromMainGame();
	}
}

void UExitConfirmWidget::OnCancelClicked()
{
	if (AMainGamePlayerController* PC = Cast<AMainGamePlayerController>(GetOwningPlayer()))
	{
		PC->CancelExitFromMainGame();
	}
}
