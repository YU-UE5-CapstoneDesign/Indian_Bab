#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PlayerNameWidget.generated.h"

class UTextBlock;
class UImage;

UCLASS()
class INDIAN_BAB_API UPlayerNameWidget : public UUserWidget
{
    GENERATED_BODY()

public:

    void SetPlayerName(const FString& Name);

    void SetCardText(const FString& Name);

    // 플레이어 이름 텍스트 색상 설정
    void SetNameTextColor(const FLinearColor& Color);
    void SetTurnImage(bool bIsMyTurn);

    // 턴 상태 업데이트 (파란색)
    UPROPERTY(EditAnywhere, Category = "Colors")
    FLinearColor ActiveTurnColor = FLinearColor::Blue;

    // 죽은 상태 색상 (빨간색)
    UPROPERTY(EditAnywhere, Category = "Colors")
    FLinearColor DeadColor = FLinearColor::Red;

    // 기본 색상 (흰색)
    UPROPERTY(EditAnywhere, Category = "Colors")
    FLinearColor DefaultColor = FLinearColor::White;


protected:
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> PlayerNameText;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> CardText;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UImage> PlayerTurnImage;
};