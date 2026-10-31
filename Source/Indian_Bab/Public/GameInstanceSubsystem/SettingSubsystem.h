#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SettingSubsystem.generated.h"


class USettingSaveGame;


UCLASS()
class INDIAN_BAB_API USettingSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
public:
    // 서브시스템 초기화 시 호출
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    // --- 블루프린트에서 사용할 함수들 ---

    UFUNCTION(BlueprintCallable, Category = "Settings")
    void SaveSettings();

    UFUNCTION(BlueprintCallable, Category = "Settings")
    void ApplySettings();

    void ApplySettingScreenResolution();

	void ApplySettingWindowMode();

    UFUNCTION(BlueprintPure, Category = "Settings")
    float GetMasterVolume() const;

    UFUNCTION(BlueprintPure, Category = "Settings")
    float GetDefaultMasterVolume() const;

    UFUNCTION(BlueprintCallable, Category = "Settings")
    void SetMasterVolume(float NewValue);

    UFUNCTION(BlueprintPure, Category = "Settings")
    float GetMouseSensitivity() const;

    UFUNCTION(BlueprintPure, Category = "Settings")
    float GetDefaultMouseSensitivity() const;

    UFUNCTION(BlueprintCallable, Category = "Settings")
    void SetMouseSensitivity(float NewValue);

	UFUNCTION(BlueprintPure, Category = "Settings")
	float GetScreenBrightness() const;

	UFUNCTION(BlueprintPure, Category = "Settings")
	float GetDefaultScreenBrightness() const;

	UFUNCTION(BlueprintCallable, Category = "Settings")
	void SetScreenBrightness(float NewValue);

    // Return only common monitor sizes supported on this PC, plus the desktop size.
    UFUNCTION(BlueprintCallable, Category = "Settings|Video")
    TArray<FString> GetSupportedScreenResolutions() const;

    UFUNCTION(BlueprintCallable, Category = "Settings|Video")
    TArray<FString> GetSupportedRefreshRates(const FString& Resolution) const;

    UFUNCTION(BlueprintPure, Category = "Settings|Video")
    FString GetSavedResolution() const;

    UFUNCTION(BlueprintPure, Category = "Settings|Video")
    int32 GetSavedRefreshRate() const;

    UFUNCTION(BlueprintPure, Category = "Settings|Video")
    FString GetDefaultResolution() const;

    UFUNCTION(BlueprintPure, Category = "Settings|Video")
    int32 GetDefaultRefreshRate() const;

    UFUNCTION(BlueprintCallable, Category = "Settings|Video")
    void SetDisplayModeValues(const FString& Resolution, int32 RefreshRate);

    // Compatibility entry point for a size or the old "size, Hz" text.
    UFUNCTION(BlueprintCallable, Category = "Settings|Video")
    void SetScreenResolution(const FString& Resolution);

	UFUNCTION(BlueprintPure, Category = "Settings")
	int GetResolutionIndex() const;

	UFUNCTION(BlueprintPure, Category = "Settings")
	int GetDefaultResolutionIndex() const;

	UFUNCTION(BlueprintCallable, Category = "Settings")
	void SetResolutionIndex(int NewValue);

	UFUNCTION(BlueprintPure, Category = "Settings")
	int GetWindowModeIndex() const;

	UFUNCTION(BlueprintPure, Category = "Settings")
	int GetDefaultWindowModeIndex() const;

	UFUNCTION(BlueprintCallable, Category = "Settings")
	void SetWindowModeIndex(int NewValue);

    // 기본값으로 되돌리기
    UFUNCTION(BlueprintCallable, Category = "Settings")
    void ResetAllSettingsToDefaults();


private:
    void LoadSettings();

    // Legacy index, used to migrate old saves.
    uint32 FindOptimalResolutionIndex() const;

    void NormalizeSavedDisplayMode();

    // 현재 게임에 적용된 설정값을 담고 있는 인스턴스
    UPROPERTY()
    TObjectPtr<USettingSaveGame> CurrentSettings;
};
