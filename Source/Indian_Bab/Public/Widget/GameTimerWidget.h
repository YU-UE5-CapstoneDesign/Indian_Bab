#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TimerManager.h"
#include "GameTimerWidget.generated.h"

class UTextBlock;

// Display only. Deadlines and timeout actions remain owned by the server.
UCLASS()
class INDIAN_BAB_API UGameTimerWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void RefreshTimer();
protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    UPROPERTY(meta=(BindWidget))
    TObjectPtr<UTextBlock> Time;
private:
    FTimerHandle RefreshHandle;
    int32 LastDisplayedSeconds = INDEX_NONE;
};
