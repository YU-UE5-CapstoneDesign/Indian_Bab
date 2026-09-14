#include "Widget/ReadyWidget.h"

#include "Character/LobbyVRCharacter.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"
#include "PlayerController/MainGamePlayerController.h"

void UReadyWidget::NativeConstruct()
{
	Super::NativeConstruct();

	bReadySubmitted = false;
	SetVisibility(ESlateVisibility::Visible);

	if (Button_Ready)
	{
		Button_Ready->SetIsEnabled(true);
		Button_Ready->SetVisibility(ESlateVisibility::Visible);
		Button_Ready->OnClicked.RemoveAll(this);
		Button_Ready->OnClicked.AddDynamic(this, &UReadyWidget::OnReadyButtonClicked);
	}

	if (Text_ReadyState)
	{
		Text_ReadyState->SetVisibility(ESlateVisibility::Visible);
		Text_ReadyState->SetText(FText::FromString(TEXT("Ready")));
	}
}

void UReadyWidget::OnReadyButtonClicked()
{
	ConfirmReady();
}

void UReadyWidget::ConfirmReady()
{
	if (bReadySubmitted)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[ReadyWidget] Ready button clicked"));

	AMainGamePlayerController* PC = Cast<AMainGamePlayerController>(GetOwningPlayer());
	if (!PC)
	{
		UE_LOG(LogTemp, Error, TEXT("[ReadyWidget] OwningPlayer is null or not AMainGamePlayerController"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[ReadyWidget] OwningPlayer=%s"), *GetNameSafe(PC));

	bReadySubmitted = true;
	PC->Server_RequestReady();

	if (Button_Ready)
	{
		Button_Ready->SetIsEnabled(false);
	}

	if (Text_ReadyState)
	{
		Text_ReadyState->SetText(FText::FromString(TEXT("Ready Complete")));
	}

	if (ALobbyVRCharacter* VRCharacter = Cast<ALobbyVRCharacter>(PC->GetPawn()))
	{
		VRCharacter->HideReadyWidget();
	}
}
