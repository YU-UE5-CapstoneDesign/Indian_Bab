#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TurnInfoWidget.generated.h"

class UTextBlock;
class AMainGameState;
class APlayerController;

UCLASS()
class INDIAN_BAB_API UTurnInfoWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeForPlayer(APlayerController* InOwningPlayer);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Turn;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_MainRevolver;

private:
	void BindGameState();
	void UnbindGameState();
	void SetTurnText(const FText& InTurnText);
	void SetMainRevolverCount(int32 CurrentCount, int32 MaxCount);

	UFUNCTION()
	void RefreshFromGameState();

	FString GetPlayerDisplayNameById(int32 PlayerId) const;

	UPROPERTY()
	TObjectPtr<AMainGameState> BoundGameState;
};
