#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "SettingSaveGame.generated.h"

/**
 * 
 */
UCLASS()
class INDIAN_BAB_API USettingSaveGame : public USaveGame
{
	GENERATED_BODY()
public:
    // 기본값을 설정하는 생성자
    USettingSaveGame();

    // 저장할 설정 값들

    UPROPERTY()
    float MasterVolume;

    UPROPERTY()
    float MouseSensitivity;

	UPROPERTY()
	float ScreenBrightness;

	UPROPERTY()
	uint32 ResolutionIndex;

	// ResolutionIndex is kept only to migrate older GameSettings saves.
	UPROPERTY()
	int32 ResolutionWidth;

	UPROPERTY()
	int32 ResolutionHeight;

	UPROPERTY()
	int32 RefreshRate;

	UPROPERTY()
	bool bHasDisplayModeValues;

    /** 0: 전체 화면, 1: 테두리 없는 창모드, 2: 창 모드 */
    UPROPERTY()
    uint32 WindowModeIndex;

    // 저장 시스템을 위한 정보

    UPROPERTY()
    FString SaveSlotName;

    UPROPERTY()
    uint32 UserIndex;
	
};
