#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PCCrosshairWidget.generated.h"

class UImage;

UCLASS()
class INDIAN_BAB_API UPCCrosshairWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void SetCrosshairVisible(bool bVisible);

protected:
    virtual void NativeConstruct() override;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UImage> UI_Crosshair;
};
