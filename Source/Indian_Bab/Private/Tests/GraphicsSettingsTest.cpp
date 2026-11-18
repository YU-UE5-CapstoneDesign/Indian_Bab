#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "GameInstanceSubsystem/SettingSubsystem.h"
#include "Widget/OptionMenuWidget.h"
#include "PlayerController/MainGamePlayerController.h"
#include "Character/LobbyVRCharacter.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/Slider.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/UserInterfaceSettings.h"
#include "Engine/World.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/IConsoleManager.h"
#include "UObject/Script.h"
#include "Slate/SceneViewport.h"
#include "Slate/SGameLayerManager.h"
#include "Slate/WidgetRenderer.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SViewport.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "ImageUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/BufferArchive.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGraphicsSettingsTest, "IndianBab.Settings.Graphics",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGraphicsSettingsTest::RunTest(const FString& Parameters)
{
    // This test applies and saves settings. Run it in a disposable project copy.
    if (!FParse::Param(FCommandLine::Get(), TEXT("GraphicsSettingsIsolated")))
    {
        AddWarning(TEXT("Skipped: use an isolated project with -GraphicsSettingsIsolated to protect user preferences."));
        return true;
    }
    FEditorScriptExecutionGuard ScriptGuard;
    TGuardValue<uint64> FrameGuard(GFrameCounter, GFrameCounter);
    UGameInstance* GI = NewObject<UGameInstance>(GEngine);
    GI->InitializeStandalone();
    UWorld* World = GI->GetWorld();
    FWorldContext& Context = GEngine->GetWorldContextFromWorldChecked(World);
    UGameViewportClient* Viewport = NewObject<UGameViewportClient>(GEngine);
    Context.GameViewport = Viewport;
    Viewport->Init(Context, GI, false);
    TSharedRef<SViewport> ViewportWidget = SNew(SViewport);
    TUniquePtr<FSceneViewport> SceneViewport(Viewport->CreateGameViewport(ViewportWidget));
    Viewport->SetViewport(SceneViewport.Get());
    TSharedRef<SOverlay> Overlay = SNew(SOverlay);
    Viewport->SetViewportOverlayWidget(nullptr, Overlay);
    TSharedRef<SGameLayerManager> Layers = SNew(SGameLayerManager).SceneViewport(static_cast<FSceneViewport*>(nullptr));
    Viewport->SetGameLayerManager(Layers);
    FString Error;
    ULocalPlayer* Local = GI->CreateLocalPlayer(0, Error, false);
    ON_SCOPE_EXIT
    {
        if (Local) GI->RemoveLocalPlayer(Local);
        GI->Shutdown();
        World->DestroyWorld(false);
        Viewport->SetViewport(nullptr);
        GEngine->DestroyWorldContext(World);
    };
    USettingSubsystem* Settings = GI->GetSubsystem<USettingSubsystem>();
    UGameUserSettings* EngineSettings = GEngine->GetGameUserSettings();
    if (!TestNotNull(TEXT("Local player"), Local) || !TestNotNull(TEXT("Settings subsystem"), Settings)
        || !TestNotNull(TEXT("Engine settings"), EngineSettings)) return false;
    const FGraphicsSettings Initial = Settings->GetGraphicsSettings();
    const float InitialVolume = Settings->GetMasterVolume();
    const float InitialSensitivity = Settings->GetMouseSensitivity();
    const float InitialBrightness = Settings->GetScreenBrightness();
    Settings->SaveSettings();
    FGraphicsSettings Invalid = Initial;
    Invalid.ShadowQuality = -1;
    Invalid.TextureQuality = 99;
    Settings->SetGraphicsSettings(Invalid);
    TestEqual(TEXT("Negative quality clamps"), Settings->GetGraphicsSettings().ShadowQuality, 0);
    TestEqual(TEXT("Excessive quality clamps"), Settings->GetGraphicsSettings().TextureQuality, 3);
    Settings->SetGraphicsSettings(Initial);

    AMainGamePlayerController* PC = World->SpawnActor<AMainGamePlayerController>();
    World->AddController(PC);
    PC->SetPlayer(Local);
    UClass* MenuClass = LoadClass<UOptionMenuWidget>(nullptr, TEXT("/Game/Blueprint/Widget/WBP_OptionMenu.WBP_OptionMenu_C"));
    if (!TestNotNull(TEXT("Production options BP"), MenuClass)) return false;
    // Retain Slate widgets while dispatching input; TakeWidget caches them weakly.
    TArray<TSharedRef<SWidget>> MenuSlateRoots;
    auto MakeMenu = [&]()
    {
        UOptionMenuWidget* Menu = CreateWidget<UOptionMenuWidget>(PC, MenuClass);
        MenuSlateRoots.Add(Menu->TakeWidget());
        return Menu;
    };
    auto Combo = [](UOptionMenuWidget* Menu, const TCHAR* Name)
    {
        return CastChecked<UComboBoxString>(Menu->GetWidgetFromName(Name));
    };
    auto Click = [](UOptionMenuWidget* Menu, const TCHAR* Name)
    {
        CastChecked<UButton>(Menu->GetWidgetFromName(Name))->OnClicked.Broadcast();
    };
    auto CheckQualityLabels = [&](UOptionMenuWidget* Menu)
    {
        for (const TCHAR* Name : {TEXT("ComboBoxString_ShadowQuality"), TEXT("ComboBoxString_TextureQuality"),
            TEXT("ComboBoxString_EffectsQuality"), TEXT("ComboBoxString_PostProcessQuality")})
        {
            const UComboBoxString* Control = Combo(Menu, Name);
            TFunction<bool(const TSharedRef<SWidget>&)> HasSelectedText;
            HasSelectedText = [&](const TSharedRef<SWidget>& Node)
            {
                if (Node->GetType() == FName(TEXT("STextBlock"))
                    && static_cast<const STextBlock&>(Node.Get()).GetText().ToString() == Control->GetSelectedOption()) return true;
                FChildren* Children = Node->GetChildren();
                for (int32 Index = 0; Index < Children->Num(); ++Index)
                    if (HasSelectedText(Children->GetChildAt(Index))) return true;
                return false;
            };
            TestTrue(FString(Name) + TEXT(" displays its selected label on open"),
                !Control->GetSelectedOption().IsEmpty() && HasSelectedText(Control->GetCachedWidget().ToSharedRef()));
        }
    };
    auto Render = [&](UOptionMenuWidget* Menu, bool bVR, FVector2D Size, const FString& Name)
    {
        FString Directory;
        if (!FParse::Value(FCommandLine::Get(), TEXT("GraphicsRenderDir="), Directory)) return;
        TSharedRef<SWidget> Content = Menu->TakeWidget();
        if (!bVR)
        {
            const float Scale = GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(FIntPoint(Size.X, Size.Y));
            Content = SNew(SDPIScaler).DPIScale(Scale)[Content];
        }
        FWidgetRenderer Renderer(true);
        UTextureRenderTarget2D* Target = Renderer.DrawWidget(Content, Size);
        for (int32 Frame = 0; Frame < 3; ++Frame)
        {
            ++GFrameCounter;
            Renderer.DrawWidget(Target, Content, Size, 0.016f);
        }
        FSlateApplication::Get().GetRenderer()->FlushCommands();
        IFileManager::Get().MakeDirectory(*Directory, true);
        FBufferArchive Png;
        TestTrue(TEXT("Render options PNG"), FImageUtils::ExportRenderTarget2DAsPNG(Target, Png));
        TestTrue(TEXT("Save options PNG"), FFileHelper::SaveArrayToFile(Png, *FPaths::Combine(Directory, Name)));
        const FSlateRect Root = Menu->GetCachedGeometry().GetRenderBoundingRect();
        for (const TCHAR* WidgetName : {TEXT("ComboBoxString_ShadowQuality"), TEXT("ComboBoxString_TextureQuality"),
            TEXT("ComboBoxString_EffectsQuality"), TEXT("ComboBoxString_PostProcessQuality"), TEXT("CheckBox_VSync"), TEXT("Button_Apply")})
        {
            const FSlateRect Bounds = Menu->GetWidgetFromName(WidgetName)->GetCachedGeometry().GetRenderBoundingRect();
            TestTrue(FString(WidgetName) + TEXT(" remains inside the viewport"), Bounds.Left >= Root.Left - 1
                && Bounds.Top >= Root.Top - 1 && Bounds.Right <= Root.Right + 1 && Bounds.Bottom <= Root.Bottom + 1
                && Bounds.GetSize().X > 0 && Bounds.GetSize().Y > 0);
        }
    };
    UOptionMenuWidget* Menu = MakeMenu();
    CheckQualityLabels(Menu);
    TestTrue(TEXT("Quality change event remains bound"), Combo(Menu, TEXT("ComboBoxString_ShadowQuality"))->OnSelectionChanged.IsBound());
    const int32 BeforeShadow = EngineSettings->GetShadowQuality();
    Combo(Menu, TEXT("ComboBoxString_ShadowQuality"))->SetSelectedIndex(Initial.ShadowQuality == 0 ? 1 : 0);
    TestTrue(TEXT("Editing only stages values"), Settings->GetGraphicsSettings() == Initial);
    TestEqual(TEXT("Editing does not alter renderer settings"), EngineSettings->GetShadowQuality(), BeforeShadow);
    TestTrue(TEXT("Graphics changes enable Apply"), Menu->GetWidgetFromName(TEXT("Button_Apply"))->GetIsEnabled());
    Click(Menu, TEXT("Button_Cancel"));
    Menu = MakeMenu();
    CheckQualityLabels(Menu);
    TestEqual(TEXT("Cancel restores saved selection on reopen"), Combo(Menu, TEXT("ComboBoxString_ShadowQuality"))->GetSelectedIndex(), Initial.ShadowQuality);
    for (const TCHAR* Name : {TEXT("ComboBoxString_ShadowQuality"), TEXT("ComboBoxString_TextureQuality"),
        TEXT("ComboBoxString_EffectsQuality"), TEXT("ComboBoxString_PostProcessQuality")}) Combo(Menu, Name)->SetSelectedIndex(0);
    UCheckBox* VSync = CastChecked<UCheckBox>(Menu->GetWidgetFromName(TEXT("CheckBox_VSync")));
    VSync->SetIsChecked(false);
    // Programmatic SetIsChecked does not dispatch the user-input delegate.
    VSync->OnCheckStateChanged.Broadcast(false);
    Menu->AddToViewport();
    TestTrue(TEXT("Options are open before applying"), Menu->IsInViewport());
    FString IniBeforePreview;
    // An unchanged default configuration may not have a generated file yet.
    TestFalse(TEXT("Engine preferences path is initialized"), GGameUserSettingsIni.IsEmpty());
    const bool bHadEngineIni = FFileHelper::LoadFileToString(IniBeforePreview, *GGameUserSettingsIni);
    auto EditSlider = [&](const TCHAR* Name, float Value)
    {
        USlider* Slider = CastChecked<USlider>(Menu->GetWidgetFromName(Name));
        Slider->SetValue(Value);
        Slider->OnValueChanged.Broadcast(Value);
    };
    EditSlider(TEXT("Slider_MasterVolume"), 0.17f);
    EditSlider(TEXT("Slider_MouseSensitivity"), 0.27f);
    EditSlider(TEXT("Slider_ScreenBrightness"), 0.37f);
    Click(Menu, TEXT("Button_Apply"));
    TestTrue(TEXT("Apply previews while keeping the options open"), Menu->IsInViewport());
    TestFalse(TEXT("Apply disables until another edit"), Menu->GetWidgetFromName(TEXT("Button_Apply"))->GetIsEnabled());
    TestEqual(TEXT("Volume is previewed"), Settings->GetMasterVolume(), 0.17f);
    TestEqual(TEXT("Sensitivity is previewed"), Settings->GetMouseSensitivity(), 0.27f);
    TestEqual(TEXT("Brightness is previewed"), Settings->GetScreenBrightness(), 0.37f);
    TestEqual(TEXT("Shadow reaches the renderer"), EngineSettings->GetShadowQuality(), 0);
    TestEqual(TEXT("Texture reaches the renderer"), EngineSettings->GetTextureQuality(), 0);
    TestEqual(TEXT("Effects reaches the renderer"), EngineSettings->GetVisualEffectQuality(), 0);
    TestEqual(TEXT("Post-process reaches the renderer"), EngineSettings->GetPostProcessingQuality(), 0);
    TestEqual(TEXT("Shadow group is not pinned by ini"), IConsoleManager::Get().FindConsoleVariable(TEXT("sg.ShadowQuality"))->GetInt(), 0);
    TestFalse(TEXT("PC VSync can be turned off"), EngineSettings->IsVSyncEnabled());
    USettingSaveGame* Saved = Cast<USettingSaveGame>(UGameplayStatics::LoadGameFromSlot(TEXT("GameSettings"), 0));
    if (!TestNotNull(TEXT("Settings saved"), Saved)) return false;
    TestTrue(TEXT("Preview does not commit the save game"), Saved->GraphicsSettings == Initial);
    FString IniAfterPreview;
    const bool bHasEngineIni = FFileHelper::LoadFileToString(IniAfterPreview, *GGameUserSettingsIni);
    TestEqual(TEXT("Preview preserves engine preferences file existence"), bHasEngineIni, bHadEngineIni);
    TestEqual(TEXT("Preview does not write engine preferences"), IniAfterPreview, IniBeforePreview);
    Combo(Menu, TEXT("ComboBoxString_ShadowQuality"))->SetSelectedIndex(1);
    Click(Menu, TEXT("Button_Apply"));
    TestEqual(TEXT("Multiple previews update the renderer"), EngineSettings->GetShadowQuality(), 1);
    Click(Menu, TEXT("Button_Cancel"));
    TestFalse(TEXT("Cancel closes the options"), Menu->IsInViewport());
    TestTrue(TEXT("Cancel restores pre-menu graphics after multiple previews"), Settings->GetGraphicsSettings() == Initial);
    TestEqual(TEXT("Cancel restores renderer quality"), EngineSettings->GetShadowQuality(), BeforeShadow);
    TestEqual(TEXT("Cancel restores master volume"), Settings->GetMasterVolume(), InitialVolume);
    TestEqual(TEXT("Cancel restores sensitivity"), Settings->GetMouseSensitivity(), InitialSensitivity);
    TestEqual(TEXT("Cancel restores brightness"), Settings->GetScreenBrightness(), InitialBrightness);
    TestEqual(TEXT("Cached menu also restores its original selection"), Combo(Menu, TEXT("ComboBoxString_ShadowQuality"))->GetSelectedIndex(), Initial.ShadowQuality);
    Menu->AddToViewport();
    for (const TCHAR* Name : {TEXT("ComboBoxString_ShadowQuality"), TEXT("ComboBoxString_TextureQuality"),
        TEXT("ComboBoxString_EffectsQuality"), TEXT("ComboBoxString_PostProcessQuality")}) Combo(Menu, Name)->SetSelectedIndex(0);
    Click(Menu, TEXT("Button_OK"));
    TestFalse(TEXT("OK applies and closes immediately without a popup"), Menu->IsInViewport());
    TestEqual(TEXT("OK applies without a preceding preview"), EngineSettings->GetShadowQuality(), 0);
    Saved = Cast<USettingSaveGame>(UGameplayStatics::LoadGameFromSlot(TEXT("GameSettings"), 0));
    if (!TestNotNull(TEXT("Confirmed preferences exist"), Saved)) return false;
    TestTrue(TEXT("OK persists the chosen settings"), Saved->GraphicsSettings == Settings->GetGraphicsSettings());
    Menu = MakeMenu();
    TestEqual(TEXT("Reopened menu retains applied quality"), Combo(Menu, TEXT("ComboBoxString_ShadowQuality"))->GetSelectedIndex(), 0);
    Click(Menu, TEXT("Button_ResetGraphics"));
    TestEqual(TEXT("Reset selects default shadow quality"), Combo(Menu, TEXT("ComboBoxString_ShadowQuality"))->GetSelectedIndex(), 3);
    TestEqual(TEXT("Reset is staged until Apply"), Settings->GetGraphicsSettings().ShadowQuality, 0);
    Menu->AddToViewport();
    Click(Menu, TEXT("Button_Apply"));
    TestTrue(TEXT("Reset can be previewed without closing"), Menu->IsInViewport());
    Click(Menu, TEXT("Button_OK"));
    TestFalse(TEXT("OK commits an existing preview and closes"), Menu->IsInViewport());
    Saved = Cast<USettingSaveGame>(UGameplayStatics::LoadGameFromSlot(TEXT("GameSettings"), 0));
    TestTrue(TEXT("OK persists a preview that was already applied"), Saved && Saved->GraphicsSettings == Settings->GetGraphicsSettings());
    Menu = MakeMenu();
    Render(Menu, false, FVector2D(1920,1080), TEXT("pc-options-1920x1080.png"));
    Render(Menu, false, FVector2D(1280,720), TEXT("pc-options-1280x720.png"));

    ALobbyVRCharacter* VR = World->SpawnActor<ALobbyVRCharacter>();
    PC->Possess(VR);
    Settings->RefreshDisplaySettingsForPlayMode();
    TestFalse(TEXT("VR ignores desktop display options"), Settings->CanUseDesktopDisplaySettings());
    TestEqual(TEXT("VR removes desktop FPS cap"), EngineSettings->GetFrameRateLimit(), 0.0f);
    TestFalse(TEXT("VR uses runtime synchronization"), EngineSettings->IsVSyncEnabled());
    TestTrue(TEXT("VR retains saved PC VSync preference"), Settings->GetGraphicsSettings().bVSync);
    Menu = MakeMenu();
    CheckQualityLabels(Menu);
    for (const TCHAR* Name : {TEXT("ComboBoxString_ScreenResolution"), TEXT("ComboBoxString_RefreshRate"),
        TEXT("ComboBoxString_WindowMode"), TEXT("CheckBox_VSync")})
        TestFalse(FString(Name) + TEXT(" disabled in VR"), Menu->GetWidgetFromName(Name)->GetIsEnabled());
    TestTrue(TEXT("Graphics controls remain available in VR"), Combo(Menu, TEXT("ComboBoxString_TextureQuality"))->GetIsEnabled());
    Render(Menu, true, FVector2D(1920,1080), TEXT("vr-options-1920x1080.png"));
    PC->UnPossess();
    VR->Destroy();
    Settings->RefreshDisplaySettingsForPlayMode();
    TestTrue(TEXT("PC VSync restored on return"), EngineSettings->IsVSyncEnabled());
    TestTrue(TEXT("PC FPS cap restored on return"), EngineSettings->GetFrameRateLimit() > 0);
    Settings->SetGraphicsSettings(Initial);
    Settings->ApplySettings();
    Settings->SaveSettings();
    PC->Player = nullptr;
    Local->PlayerController = nullptr;
    PC->Destroy();
    return true;
}
#endif
