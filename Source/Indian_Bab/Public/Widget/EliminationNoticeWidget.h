#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EliminationNoticeWidget.generated.h"

class UWidgetSwitcher;
class UButton;

UCLASS()
class INDIAN_BAB_API UEliminationNoticeWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category = "Elimination")
    void SetNotice(bool bSpectating);
    bool HandleSpectateClickAtAbsoluteLocation(const FVector2D& AbsoluteLocation);
protected:
    virtual void NativePreConstruct() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Elimination")
    bool bShowingSpectator = false;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UWidgetSwitcher> NoticeStates;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> Button_Spectate;
private:
    UFUNCTION()
    void OnSpectateClicked();
};
