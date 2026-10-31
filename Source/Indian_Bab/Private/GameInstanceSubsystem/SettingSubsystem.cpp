#include "GameInstanceSubsystem/SettingSubsystem.h"
#include "SaveGame/SettingSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/GameUserSettings.h"
#include "GameInstanceSubsystem/IndianBabGameInstance.h"


void USettingSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    LoadSettings();
    ApplySettings();
}


void USettingSubsystem::LoadSettings()
{
    // 저장된 파일이 있는지 확인하고 불러오기
    if (UGameplayStatics::DoesSaveGameExist(TEXT("GameSettings"), 0))
    {
        CurrentSettings = Cast<USettingSaveGame>(UGameplayStatics::LoadGameFromSlot(TEXT("GameSettings"), 0));
    }
    else
    {
        // 없다면 기본값으로 새로 생성
        CurrentSettings = Cast<USettingSaveGame>(UGameplayStatics::CreateSaveGameObject(USettingSaveGame::StaticClass()));

        // 새로 추출한 함수를 호출하여 최적의 인덱스를 찾아 설정
        CurrentSettings->ResolutionIndex = FindOptimalResolutionIndex();
    }
}


uint32 USettingSubsystem::FindOptimalResolutionIndex() const
{
    // 1. 현재 데스크톱 해상도를 가져옵니다.
    FDisplayMetrics DisplayMetrics;
    FDisplayMetrics::GetDisplayMetrics(DisplayMetrics);
    const int32 DesktopWidth = DisplayMetrics.PrimaryDisplayWidth;
    const int32 DesktopHeight = DisplayMetrics.PrimaryDisplayHeight;

    // 2. 가장 적합한 옵션을 찾기 위한 변수를 초기화합니다.
    const TArray<FString> Resolutions = GetSupportedScreenResolutions();
    int32 BestMatchIndex = -1; // 가장 적합한 인덱스
    int32 MaxRefreshRate = 0;  // 찾은 최고 주사율

    // 3. 지원하는 모든 해상도/주사율 목록을 순회합니다.
    for (int32 i = 0; i < Resolutions.Num(); ++i)
    {
        const FString& ModeString = Resolutions[i];

        FString ResolutionPart, RefreshRatePart;
        if (ModeString.Split(TEXT(","), &ResolutionPart, &RefreshRatePart))
        {
            FString WidthString, HeightString;
            if (ResolutionPart.Split(TEXT("x"), &WidthString, &HeightString))
            {
                const int32 Width = FCString::Atoi(*WidthString.TrimStartAndEnd());
                const int32 Height = FCString::Atoi(*HeightString.TrimStartAndEnd());

                // 4. 현재 데스크톱 해상도와 일치하는지 확인합니다.
                if (Width == DesktopWidth && Height == DesktopHeight)
                {
					RefreshRatePart.RemoveAt(RefreshRatePart.Find(TEXT("Hz")));
                    const int32 RefreshRate = FCString::Atoi(*RefreshRatePart.TrimStartAndEnd());

                    // 5. 이전에 찾은 주사율보다 더 높은 주사율이면, '가장 적합한 옵션'으로 교체합니다.
                    if (RefreshRate > MaxRefreshRate)
                    {
                        MaxRefreshRate = RefreshRate;
                        BestMatchIndex = i;
                    }
                }
            }
        }
    }

    // 6. 유효한 인덱스를 찾았으면 반환하고, 못 찾았으면 0을 반환합니다.
    return BestMatchIndex != -1 ? BestMatchIndex : 0;
}


void USettingSubsystem::SaveSettings()
{
    if (CurrentSettings)
    {
        UGameplayStatics::SaveGameToSlot(CurrentSettings, CurrentSettings->SaveSlotName, CurrentSettings->UserIndex);
    }
}


void USettingSubsystem::ApplySettings()
{
    if (!CurrentSettings) return;


    ApplySettingScreenResolution();
    ApplySettingWindowMode();

}


void USettingSubsystem::ApplySettingScreenResolution()
{
    // 1. 지원하는 모든 해상도 목록을 가져옵니다.
    const TArray<FString> Resolutions = GetSupportedScreenResolutions();

    // 2. 저장된 인덱스가 유효한 범위 내에 있는지 확인합니다. (매우 중요! 크래시 방지)
    if (Resolutions.IsValidIndex(CurrentSettings->ResolutionIndex))
    {
        // 3. 인덱스에 해당하는 해상도 문자열을 가져와 적용합니다.
        SetScreenResolution(Resolutions[CurrentSettings->ResolutionIndex]);
    }
}


void USettingSubsystem::ApplySettingWindowMode()
{
    if (GEngine)
    {
        UGameUserSettings* Settings = GEngine->GetGameUserSettings();
        if (Settings)
        {
            Settings->SetFullscreenMode(static_cast<EWindowMode::Type>(CurrentSettings->WindowModeIndex));
            Settings->ApplySettings(false);
        }
	}
}


/*
* 블루프린트용 Getter / Setter 구현
*/ 
float USettingSubsystem::GetMasterVolume() const
{
    return CurrentSettings ? CurrentSettings->MasterVolume : 1.0f;
}


float USettingSubsystem::GetDefaultMasterVolume() const
{
    return GetDefault<USettingSaveGame>()->MasterVolume;
}



void USettingSubsystem::SetMasterVolume(float NewValue)
{
    if (!CurrentSettings)
    {
        return;
    }

    const float Volume = FMath::Clamp(NewValue, 0.0f, 1.0f);

    CurrentSettings->MasterVolume = Volume;

    // 현재 재생 중인 BGM 볼륨 적용
    if (UIndianBabGameInstance* GI =
        Cast<UIndianBabGameInstance>(GetGameInstance()))
    {
        GI->SetBGMVolume(Volume);
    }
}


float USettingSubsystem::GetMouseSensitivity() const
{
    return CurrentSettings ? CurrentSettings->MouseSensitivity : 1.0f;
}


float USettingSubsystem::GetDefaultMouseSensitivity() const
{
    return GetDefault<USettingSaveGame>()->MouseSensitivity;
}


void USettingSubsystem::SetMouseSensitivity(float NewValue)
{
    if (CurrentSettings) CurrentSettings->MouseSensitivity = NewValue;
}


float USettingSubsystem::GetScreenBrightness() const
{
	return CurrentSettings ? CurrentSettings->ScreenBrightness : 1.0f;
}


float USettingSubsystem::GetDefaultScreenBrightness() const
{
	return GetDefault<USettingSaveGame>()->ScreenBrightness;
}


void USettingSubsystem::SetScreenBrightness(float NewValue)
{
	if (CurrentSettings) CurrentSettings->ScreenBrightness = NewValue;
}


TArray<FString> USettingSubsystem::GetSupportedScreenResolutions() const
{
    TArray<FString> ResolutionsAndRates;
    FScreenResolutionArray ResolutionsArray;

    if (RHIGetAvailableResolutions(ResolutionsArray, false))
    {
        for (const FScreenResolutionRHI& Res : ResolutionsArray)
        {
            // "너비 x 높이, 주사율Hz" 형식의 문자열로 만듭니다.
            // 예: "1920 x 1080, 144Hz"
            FString DisplayModeString = FString::Printf(TEXT("%d x %d, %dHz"), Res.Width, Res.Height, Res.RefreshRate);
            ResolutionsAndRates.Add(DisplayModeString);
        }
    }

    // 모든 디스플레이 모드(해상도+주사율 조합)를 반환합니다.
    return ResolutionsAndRates;
}


void USettingSubsystem::SetScreenResolution(const FString& DisplayModeString)
{
    FString ResolutionPart;
    FString RefreshRatePart;

    // 1. 쉼표(,)를 기준으로 "해상도" 부분과 "주사율" 부분으로 나눕니다.
    if (!DisplayModeString.Split(TEXT(","), &ResolutionPart, &RefreshRatePart))
    {
        // 쉼표가 없는 잘못된 형식이면 함수를 종료합니다.
        return;
    }

    FString WidthString;
    FString HeightString;

    // 2. "해상도" 부분을 'x'를 기준으로 "너비"와 "높이"로 나눕니다.
    if (!ResolutionPart.Split(TEXT("x"), &WidthString, &HeightString))
    {
        // 'x'가 없는 잘못된 형식이면 함수를 종료합니다.
        return;
    }

    // 3. 각 문자열의 공백을 제거하고 정수(int)로 변환합니다.
    const int32 Width = FCString::Atoi(*WidthString.TrimStartAndEnd());
    const int32 Height = FCString::Atoi(*HeightString.TrimStartAndEnd());

    // "주사율" 부분에서 "Hz"를 제거하고 공백을 없앤 뒤 숫자로 변환합니다.
	RefreshRatePart.RemoveAt(RefreshRatePart.Find(TEXT("Hz")));
    const int32 RefreshRate = FCString::Atoi(*RefreshRatePart.TrimStartAndEnd());

    // 변환된 값이 유효한지 간단히 확인 (0보다 커야 함)
    if (Width <= 0 || Height <= 0 || RefreshRate <= 0)
    {
        return;
    }

    // 4. GameUserSettings를 가져와서 설정을 적용합니다.
    if (GEngine)
    {
        UGameUserSettings* Settings = GEngine->GetGameUserSettings();
        if (Settings)
        {
            Settings->SetScreenResolution(FIntPoint(Width, Height));
            Settings->SetFrameRateLimit(RefreshRate);
            Settings->SetVSyncEnabled(true);
            Settings->ApplySettings(false);
        }
    }
}


int USettingSubsystem::GetResolutionIndex() const
{
	return CurrentSettings ? CurrentSettings->ResolutionIndex : uint32();
}


int USettingSubsystem::GetDefaultResolutionIndex() const
{
	return FindOptimalResolutionIndex();
}


void USettingSubsystem::SetResolutionIndex(int NewValue)
{
	if (CurrentSettings) CurrentSettings->ResolutionIndex = NewValue;
}


int USettingSubsystem::GetWindowModeIndex() const
{
    return CurrentSettings ? CurrentSettings->WindowModeIndex : GetDefault<USettingSaveGame>()->WindowModeIndex;
}


int USettingSubsystem::GetDefaultWindowModeIndex() const
{
	return GetDefault<USettingSaveGame>()->WindowModeIndex;
}


void USettingSubsystem::SetWindowModeIndex(int NewValue)
{
	if (CurrentSettings) CurrentSettings->WindowModeIndex = NewValue;
}


void USettingSubsystem::ResetAllSettingsToDefaults()
{
    // 기본값을 가진 새 인스턴스를 만들어 현재 설정에 덮어쓰기
    CurrentSettings = Cast<USettingSaveGame>(UGameplayStatics::CreateSaveGameObject(USettingSaveGame::StaticClass()));
	CurrentSettings->ResolutionIndex = FindOptimalResolutionIndex();
    SaveSettings();
    ApplySettings();
}