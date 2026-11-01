#pragma once
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RevolverCountWidget.generated.h"

class UTextBlock;

/**
 * 메인 리볼버 위에 표시되는 베팅 발수 위젯
 * 예: "3/8" 형식으로 현재 누적 방아쇠 횟수 표시
 * EGamePhase::Playing 상태일 때만 표시됨
 */
UCLASS()
class INDIAN_BAB_API URevolverCountWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * 표시 텍스트 업데이트
	 * @param CurrentCount 현재 누적 방아쇠 횟수
	 * @param MaxCount 최대 방아쇠 횟수 (기본 8)
	 */
	UFUNCTION(BlueprintCallable, Category = "RevolverCount")
	void UpdateCount(int32 CurrentCount, int32 MaxCount = 8);

	UFUNCTION(BlueprintCallable, Category = "SubRevolverCount")
	void UpdateFoldCount(int32 Count);

	/** Playing 페이즈 여부에 따라 위젯 전체 가시성 설정 */
	UFUNCTION(BlueprintCallable, Category = "RevolverCount")
	void SetPlayingPhase(bool bIsPlaying);

protected:
	/** BP에서 바인딩할 텍스트 블록 (WBP에서 "Text_BulletCount" 이름으로 생성 필요) */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_BulletCount;
};