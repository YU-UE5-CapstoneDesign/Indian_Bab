#include "GameInstanceSubsystem/SettingSubsystem.h"
#include "SaveGame/SettingSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/GameUserSettings.h"

namespace
{
bool ParseResolutionSize(const FString& Text, FIntPoint& OutSize)
{
    FString Width, Height;
    if (!Text.Split(TEXT("x"), &Width, &Height)) return false;
    OutSize = FIntPoint(FCString::Atoi(*Width.TrimStartAndEnd()), FCString::Atoi(*Height.TrimStartAndEnd()));
    return OutSize.X > 0 && OutSize.Y > 0;
}

FString FormatResolutionSize(const FIntPoint& Size)
{
    return FString::Printf(TEXT("%d x %d"), Size.X, Size.Y);
}

FIntPoint GetDesktopSize()
{
    FDisplayMetrics Metrics;
    FDisplayMetrics::GetDisplayMetrics(Metrics);
    return FIntPoint(Metrics.PrimaryDisplayWidth, Metrics.PrimaryDisplayHeight);
}
}


void USettingSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    LoadSettings();
    ApplySettings();
}


void USettingSubsystem::LoadSettings()
{
    const bool bHadSave = UGameplayStatics::DoesSaveGameExist(TEXT("GameSettings"), 0);
    // 저장된 파일이 있는지 확인하고 불러오기
    if (bHadSave)
    {
        CurrentSettings = Cast<USettingSaveGame>(UGameplayStatics::LoadGameFromSlot(TEXT("GameSettings"), 0));
    }
    else
    {
        // 없다면 기본값으로 새로 생성
        CurrentSettings = Cast<USettingSaveGame>(UGameplayStatics::CreateSaveGameObject(USettingSaveGame::StaticClass()));

    }
    if (!CurrentSettings)
    {
        CurrentSettings = Cast<USettingSaveGame>(UGameplayStatics::CreateSaveGameObject(USettingSaveGame::StaticClass()));
    }
    if (!bHadSave)
    {
        const FIntPoint Desktop = GetDesktopSize();
        CurrentSettings->ResolutionWidth = Desktop.X;
        CurrentSettings->ResolutionHeight = Desktop.Y;
        CurrentSettings->RefreshRate = GetDefaultRefreshRate();
        CurrentSettings->bHasDisplayModeValues = true;
    }
    NormalizeSavedDisplayMode();
}


uint32 USettingSubsystem::FindOptimalResolutionIndex() const
{
    const int32 Index = GetSupportedScreenResolutions().IndexOfByKey(GetDefaultResolution());
    return Index == INDEX_NONE ? 0 : Index;
}

void USettingSubsystem::NormalizeSavedDisplayMode()
{
    if (!CurrentSettings) return;

    if (!CurrentSettings->bHasDisplayModeValues)
    {
        // The old index referred to the unfiltered RHI list, including every Hz variant.
        FScreenResolutionArray LegacyModes;
        if (RHIGetAvailableResolutions(LegacyModes, false) &&
            LegacyModes.IsValidIndex(static_cast<int32>(CurrentSettings->ResolutionIndex)))
        {
            const FScreenResolutionRHI& Mode = LegacyModes[CurrentSettings->ResolutionIndex];
            CurrentSettings->ResolutionWidth = Mode.Width;
            CurrentSettings->ResolutionHeight = Mode.Height;
            CurrentSettings->RefreshRate = Mode.RefreshRate;
        }
        CurrentSettings->bHasDisplayModeValues = true;
    }

    const TArray<FString> Sizes = GetSupportedScreenResolutions();
    FString Size = FormatResolutionSize(FIntPoint(CurrentSettings->ResolutionWidth, CurrentSettings->ResolutionHeight));
    if (!Sizes.Contains(Size))
    {
        Size = GetDefaultResolution();
        FIntPoint Parsed;
        if (ParseResolutionSize(Size, Parsed))
        {
            CurrentSettings->ResolutionWidth = Parsed.X;
            CurrentSettings->ResolutionHeight = Parsed.Y;
        }
    }

    const TArray<FString> Rates = GetSupportedRefreshRates(Size);
    const FString SavedRate = FString::Printf(TEXT("%d FPS"), CurrentSettings->RefreshRate);
    if (!Rates.Contains(SavedRate))
    {
        CurrentSettings->RefreshRate = Rates.IsEmpty() ? 60 : FCString::Atoi(*Rates.Last());
    }
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
    if (!CurrentSettings || !GEngine) return;
    if (CurrentSettings->ResolutionWidth <= 0 || CurrentSettings->ResolutionHeight <= 0 || CurrentSettings->RefreshRate <= 0) return;
    UGameUserSettings* Settings = GEngine->GetGameUserSettings();
    if (!Settings) return;
    Settings->SetScreenResolution(FIntPoint(CurrentSettings->ResolutionWidth, CurrentSettings->ResolutionHeight));
    // Unreal's GameUserSettings uses this value as an FPS cap; it does not switch the monitor's physical Hz.
    Settings->SetFrameRateLimit(CurrentSettings->RefreshRate);
    Settings->SetVSyncEnabled(true);
    Settings->ApplySettings(false);
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
    if (CurrentSettings) CurrentSettings->MasterVolume = NewValue;
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
    const FIntPoint Desktop = GetDesktopSize();
    const TArray<FIntPoint> CommonSizes = {
        FIntPoint(640, 480), FIntPoint(800, 600), FIntPoint(960, 540),
        FIntPoint(1024, 576), FIntPoint(1024, 768), FIntPoint(1152, 648),
        FIntPoint(1280, 720), FIntPoint(1600, 900), FIntPoint(1920, 1080),
        FIntPoint(2560, 1440), FIntPoint(3840, 2160)
    };
    TArray<FIntPoint> Sizes;
    FScreenResolutionArray Modes;
    if (RHIGetAvailableResolutions(Modes, false))
    {
        for (const FScreenResolutionRHI& Mode : Modes)
        {
            const FIntPoint Size(Mode.Width, Mode.Height);
            if (CommonSizes.Contains(Size) || Size == Desktop) Sizes.AddUnique(Size);
        }
    }
    if (Desktop.X > 0 && Desktop.Y > 0) Sizes.AddUnique(Desktop);
    Sizes.Sort([](const FIntPoint& A, const FIntPoint& B) {
        return A.X == B.X ? A.Y < B.Y : A.X < B.X;
    });
    TArray<FString> Result;
    for (const FIntPoint& Size : Sizes) Result.Add(FormatResolutionSize(Size));
    return Result;
}

TArray<FString> USettingSubsystem::GetSupportedRefreshRates(const FString& Resolution) const
{
    FIntPoint Size;
    if (!ParseResolutionSize(Resolution, Size)) return {};
    TArray<int32> Rates;
    FScreenResolutionArray Modes;
    if (RHIGetAvailableResolutions(Modes, false))
    {
        for (const FScreenResolutionRHI& Mode : Modes)
        {
            if (Mode.Width == Size.X && Mode.Height == Size.Y && Mode.RefreshRate > 0)
                Rates.AddUnique(Mode.RefreshRate);
        }
    }
    if (Rates.IsEmpty() && Size == GetDesktopSize()) Rates.Add(60);
    Rates.Sort();
    TArray<FString> Result;
    for (int32 Rate : Rates) Result.Add(FString::Printf(TEXT("%d FPS"), Rate));
    return Result;
}

FString USettingSubsystem::GetSavedResolution() const
{
    return CurrentSettings ? FormatResolutionSize(FIntPoint(CurrentSettings->ResolutionWidth, CurrentSettings->ResolutionHeight)) : GetDefaultResolution();
}

int32 USettingSubsystem::GetSavedRefreshRate() const
{
    return CurrentSettings ? CurrentSettings->RefreshRate : GetDefaultRefreshRate();
}

FString USettingSubsystem::GetDefaultResolution() const
{
    const FString Desktop = FormatResolutionSize(GetDesktopSize());
    const TArray<FString> Sizes = GetSupportedScreenResolutions();
    return Sizes.Contains(Desktop) ? Desktop : (Sizes.IsEmpty() ? Desktop : Sizes.Last());
}

int32 USettingSubsystem::GetDefaultRefreshRate() const
{
    const TArray<FString> Rates = GetSupportedRefreshRates(GetDefaultResolution());
    return Rates.IsEmpty() ? 60 : FCString::Atoi(*Rates.Last());
}

void USettingSubsystem::SetDisplayModeValues(const FString& Resolution, int32 RefreshRate)
{
    if (!CurrentSettings || !GetSupportedScreenResolutions().Contains(Resolution)) return;
    if (!GetSupportedRefreshRates(Resolution).Contains(FString::Printf(TEXT("%d FPS"), RefreshRate))) return;
    FIntPoint Size;
    if (!ParseResolutionSize(Resolution, Size)) return;
    CurrentSettings->ResolutionWidth = Size.X;
    CurrentSettings->ResolutionHeight = Size.Y;
    CurrentSettings->RefreshRate = RefreshRate;
    CurrentSettings->bHasDisplayModeValues = true;
}


void USettingSubsystem::SetScreenResolution(const FString& DisplayModeString)
{
    FString ResolutionPart;
    FString RefreshRatePart;
    if (!DisplayModeString.Split(TEXT(","), &ResolutionPart, &RefreshRatePart))
    {
        const TArray<FString> Rates = GetSupportedRefreshRates(DisplayModeString);
        const FString SavedRate = FString::Printf(TEXT("%d FPS"), GetSavedRefreshRate());
        const int32 Rate = Rates.Contains(SavedRate) ? GetSavedRefreshRate()
            : (Rates.IsEmpty() ? 0 : FCString::Atoi(*Rates.Last()));
        SetDisplayModeValues(DisplayModeString, Rate);
        ApplySettingScreenResolution();
        return;
    }
    SetDisplayModeValues(ResolutionPart.TrimStartAndEnd(), FCString::Atoi(*RefreshRatePart.TrimStartAndEnd()));
    ApplySettingScreenResolution();
}


int USettingSubsystem::GetResolutionIndex() const
{
	const int32 Index = GetSupportedScreenResolutions().IndexOfByKey(GetSavedResolution());
	return Index == INDEX_NONE ? 0 : Index;
}


int USettingSubsystem::GetDefaultResolutionIndex() const
{
	return FindOptimalResolutionIndex();
}


void USettingSubsystem::SetResolutionIndex(int NewValue)
{
	const TArray<FString> Sizes = GetSupportedScreenResolutions();
	if (!CurrentSettings || !Sizes.IsValidIndex(NewValue)) return;
	const TArray<FString> Rates = GetSupportedRefreshRates(Sizes[NewValue]);
	const int32 Rate = Rates.Contains(FString::Printf(TEXT("%d FPS"), CurrentSettings->RefreshRate))
		? CurrentSettings->RefreshRate : (Rates.IsEmpty() ? 0 : FCString::Atoi(*Rates.Last()));
	SetDisplayModeValues(Sizes[NewValue], Rate);
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
    const FIntPoint Desktop = GetDesktopSize();
    CurrentSettings->ResolutionWidth = Desktop.X;
    CurrentSettings->ResolutionHeight = Desktop.Y;
    CurrentSettings->RefreshRate = GetDefaultRefreshRate();
    CurrentSettings->bHasDisplayModeValues = true;
    SaveSettings();
    ApplySettings();
}
