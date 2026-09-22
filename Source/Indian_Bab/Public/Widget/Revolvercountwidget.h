#pragma once
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RevolverCountWidget.generated.h"

class UTextBlock;

/**
 * ���� ������ ���� ǥ�õǴ� ���� �߼� ����
 * ��: "3/8" �������� ���� ���� ��Ƽ� Ƚ�� ǥ��
 * EGamePhase::Playing ������ ���� ǥ�õ�
 */
UCLASS()
class INDIAN_BAB_API URevolverCountWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * ǥ�� �ؽ�Ʈ ������Ʈ
	 * @param CurrentCount ���� ���� ��Ƽ� Ƚ��
	 * @param MaxCount �ִ� ��Ƽ� Ƚ�� (�⺻ 8)
	 */
	UFUNCTION(BlueprintCallable, Category = "RevolverCount")
	void UpdateCount(int32 CurrentCount, int32 MaxCount = 8);

	UFUNCTION(BlueprintCallable, Category = "SubRevolverCount")
	void UpdateFoldCount(int32 Count);

	/** Playing ������ ���ο� ���� ���� ��ü ���ü� ���� */
	UFUNCTION(BlueprintCallable, Category = "RevolverCount")
	void SetPlayingPhase(bool bIsPlaying);

protected:
	/** BP���� ���ε��� �ؽ�Ʈ ���� (WBP���� "Text_BulletCount" �̸����� ���� �ʿ�) */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_BulletCount;
};