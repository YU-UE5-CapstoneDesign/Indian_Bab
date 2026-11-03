#include "Widget/PlayerNameWidget.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"

void UPlayerNameWidget::SetPlayerName(const FString& Name)
{
    if (PlayerNameText)
    {
        PlayerNameText->SetText(FText::FromString(Name));
    }
}

void UPlayerNameWidget::SetCardText(const FString& Name)
{
    if (CardText)
    {
        CardText->SetText(FText::FromString(Name));
    }
}

void UPlayerNameWidget::SetNameTextColor(const FLinearColor& Color)
{
    if (PlayerNameText)
    {
        PlayerNameText->SetColorAndOpacity(Color);
    }
}

void UPlayerNameWidget::SetTurnImage(bool bIsMyTurn) 
{
    if (bIsMyTurn && PlayerTurnImage) 
    {
        PlayerTurnImage->SetVisibility(ESlateVisibility::Visible);
    }
    else {
        PlayerTurnImage->SetVisibility(ESlateVisibility::Hidden);
    }
}
