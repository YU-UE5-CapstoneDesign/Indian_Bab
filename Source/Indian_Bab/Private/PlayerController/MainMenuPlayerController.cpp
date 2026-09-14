#include "PlayerController/MainMenuPlayerController.h"
#include "Blueprint/UserWidget.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "GameInstanceSubsystem/ConnectivitySubsystem.h"
#include "Widget/MainMenuWidget.h"
#include "Character/LobbyVRCharacter.h"
#include "GameInstanceSubsystem/IndianBabGameInstance.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Engine/GameViewportClient.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SOverlay.h"
#include "TimerManager.h"

void AMainMenuPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveAllViewportWidgets();
	}

	if (!IsLocalPlayerController())
		return;

	ApplyMainMenuMappingContext();
    // 기본 Pawn의 초기화가 끝난 뒤 선택창 또는 기존 메뉴를 엽니다.
    GetWorldTimerManager().SetTimerForNextTick(this, &AMainMenuPlayerController::BeginPlayModeSelection);

	// 연결성 구독 + 폴링 시작 — 메인메뉴 PC 살아있는 동안만 활성
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UConnectivitySubsystem* Connectivity = GI->GetSubsystem<UConnectivitySubsystem>())
		{
			LostHandle = Connectivity->OnConnectivityLost.AddUObject(
				this, &AMainMenuPlayerController::HandleConnectivityLost);
			RestoredHandle = Connectivity->OnConnectivityRestored.AddUObject(
				this, &AMainMenuPlayerController::HandleConnectivityRestored);

			Connectivity->StartPolling();
		}
	}
}

void AMainMenuPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    RemovePlayModePrompt();
	// 델리게이트 해제 + 폴링 정지 — PC 파괴 시 dangling 핸들 방지
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UConnectivitySubsystem* Connectivity = GI->GetSubsystem<UConnectivitySubsystem>())
		{
			Connectivity->OnConnectivityLost.Remove(LostHandle);
			Connectivity->OnConnectivityRestored.Remove(RestoredHandle);
			Connectivity->StopPolling();
		}
	}

	if (IsLocalPlayerController())
	{
		RemoveMainMenuMappingContext();
	}

	Super::EndPlay(EndPlayReason);
}

void AMainMenuPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

	if (!IsLocalPlayerController()) 
        return;

    if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent))
    {
        if (IA_RightTriggerClick)
        {
            UE_LOG(LogTemp, Warning, TEXT("[VR UI] IA_RightTriggerClick bound"));
            EnhancedInput->BindAction(IA_RightTriggerClick, ETriggerEvent::Started, this, &AMainMenuPlayerController::OnRightTriggerClickStarted);
            EnhancedInput->BindAction(IA_RightTriggerClick, ETriggerEvent::Completed, this, &AMainMenuPlayerController::OnRightTriggerClickReleased);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[VR UI] IA_RightTriggerClick is null"));
        }

        if (IA_LeftTriggerClick)
        {
            UE_LOG(LogTemp, Warning, TEXT("[VR UI] IA_LeftTriggerClick bound"));
            EnhancedInput->BindAction(IA_LeftTriggerClick, ETriggerEvent::Started, this, &AMainMenuPlayerController::OnLeftTriggerClickStarted);
            EnhancedInput->BindAction(IA_LeftTriggerClick, ETriggerEvent::Completed, this, &AMainMenuPlayerController::OnLeftTriggerClickReleased);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[VR UI] IA_LeftTriggerClick is null"));
        }
    }
}

void AMainMenuPlayerController::ApplyMainMenuMappingContext()
{
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!LocalPlayer)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Input] ApplyMainMenuMappingContext failed. LocalPlayer is null"));
		return;
	}

	auto* Subsys = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (!Subsys)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Input] ApplyMainMenuMappingContext failed. EnhancedInput subsystem is null"));
		return;
	}

	if (MainMenuMappingContext)
	{
		Subsys->AddMappingContext(MainMenuMappingContext, 0);
		UE_LOG(LogTemp, Warning, TEXT("[Input] MainMenuMappingContext applied: %s"), *GetNameSafe(MainMenuMappingContext));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Input] MainMenuMappingContext is null"));
	}
}

void AMainMenuPlayerController::RemoveMainMenuMappingContext()
{
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!LocalPlayer || !MainMenuMappingContext)
	{
		return;
	}

	if (auto* Subsys = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
	{
		Subsys->RemoveMappingContext(MainMenuMappingContext);
	}
}

void AMainMenuPlayerController::OnLeftTriggerClickStarted(const FInputActionValue& Value)
{
	if (ALobbyVRCharacter* VRCharacter = Cast<ALobbyVRCharacter>(GetPawn()))
	{
		VRCharacter->PressLeftWidgetInteraction();
	}
}

void AMainMenuPlayerController::OnLeftTriggerClickReleased(const FInputActionValue& Value)
{
	if (ALobbyVRCharacter* VRCharacter = Cast<ALobbyVRCharacter>(GetPawn()))
	{
		VRCharacter->ReleaseLeftWidgetInteraction();
	}
}

void AMainMenuPlayerController::OnRightTriggerClickStarted(const FInputActionValue& Value)
{
	if (ALobbyVRCharacter* VRCharacter = Cast<ALobbyVRCharacter>(GetPawn()))
	{
		VRCharacter->PressRightWidgetInteraction();
	}
}

void AMainMenuPlayerController::OnRightTriggerClickReleased(const FInputActionValue& Value)
{
	if (ALobbyVRCharacter* VRCharacter = Cast<ALobbyVRCharacter>(GetPawn()))
	{
		VRCharacter->ReleaseRightWidgetInteraction();
	}
}

void AMainMenuPlayerController::OpenMainMenu()
{
	if (ALobbyVRCharacter* VRCharacter = Cast<ALobbyVRCharacter>(GetPawn()))
	{
        VRCharacter->SetActiveVRUI(IsMouseMenuInputMode() ? EVRActiveUI::None : EVRActiveUI::MainMenu);
	}

	if (!MainMenuWidgetClass)
	{
		return;
	}

	MainMenuWidgetInstance = CreateWidget<UMainMenuWidget>(this, MainMenuWidgetClass);
	if (MainMenuWidgetInstance)
	{
		ApplyMenuInputMode(MainMenuWidgetInstance);
	}
}

void AMainMenuPlayerController::ApplyMenuInputMode(UUserWidget* FocusWidget)
{
	const bool bUseMouseInput = MenuInputMode == EMainMenuInputMode::Mouse;

	if (bUseMouseInput)
	{
		if (MainMenuWidgetInstance && !MainMenuWidgetInstance->IsInViewport())
		{
			MainMenuWidgetInstance->AddToViewport();
		}

		FInputModeUIOnly InputModeData;
		if (FocusWidget)
		{
			InputModeData.SetWidgetToFocus(FocusWidget->TakeWidget());
		}
		InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputModeData);

		bShowMouseCursor = true;
		bEnableClickEvents = true;
		bEnableMouseOverEvents = true;
		return;
	}

	if (MainMenuWidgetInstance && MainMenuWidgetInstance->IsInViewport())
	{
		MainMenuWidgetInstance->RemoveFromParent();
	}

	FInputModeGameOnly InputModeData;
	SetInputMode(InputModeData);

	bShowMouseCursor = false;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;
}

void AMainMenuPlayerController::SetMenuInputMode(EMainMenuInputMode NewInputMode)
{
	MenuInputMode = NewInputMode;
	ApplyMenuInputMode(MainMenuWidgetInstance);
}

void AMainMenuPlayerController::HandleConnectivityLost()
{
	if (!OfflineWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("MainMenuPC: OfflineWidgetClass is not set."));
		return;
	}

	if (!OfflineWidgetInstance)
	{
		OfflineWidgetInstance = CreateWidget<UUserWidget>(this, OfflineWidgetClass);
		if (OfflineWidgetInstance)
		{
			OfflineWidgetInstance->SetIsFocusable(true);
		}
	}

	if (OfflineWidgetInstance && !OfflineWidgetInstance->IsInViewport())
	{
		OfflineWidgetInstance->AddToViewport(100);

		// VR 모드에서 SetInputMode(UIOnly)는 WidgetInteractionComponent 시뮬 키 라우팅을 깨뜨림 — Mouse 모드만 적용 (오프라인 모달은 ~10s 후 자동 종료라 입력 불필요)
		if (IsMouseMenuInputMode())
		{
			FInputModeUIOnly InputModeData;
			InputModeData.SetWidgetToFocus(OfflineWidgetInstance->TakeWidget());
			InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			SetInputMode(InputModeData);
			bShowMouseCursor = true;
			bEnableClickEvents = true;
			bEnableMouseOverEvents = true;
		}
	}
}

void AMainMenuPlayerController::RefocusMainMenu()
{
	if (MainMenuWidgetInstance)
	{
		ApplyMenuInputMode(MainMenuWidgetInstance);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("MainMenuPC: RefocusMainMenu skipped because MainMenuWidget is missing."));
	}
}

void AMainMenuPlayerController::HandleConnectivityRestored()
{
	if (OfflineWidgetInstance && OfflineWidgetInstance->IsInViewport())
	{
		OfflineWidgetInstance->RemoveFromParent();
		RefocusMainMenu();
	}
}


// VR 기기가 있을 때만 선택창을 표시하며, 재방문 시에는 이전 선택을 사용합니다.
void AMainMenuPlayerController::BeginPlayModeSelection()
{
    if (!IsLocalPlayerController()) return;
    UIndianBabGameInstance* GI = Cast<UIndianBabGameInstance>(GetGameInstance());
    if (GI && GI->HasSelectedPlayMode())
    {
        FinishPlayModeSelection(GI->IsVRPlayMode());
        return;
    }
    if (!UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayConnected())
    {
        FinishPlayModeSelection(false);
        return;
    }

    UHeadMountedDisplayFunctionLibrary::EnableHMD(false);
    if (ALobbyVRCharacter* VRCharacter = Cast<ALobbyVRCharacter>(GetPawn()))
        VRCharacter->SetActiveVRUI(EVRActiveUI::None);
    if (APawn* MenuPawn = GetPawn())
    {
        MenuPawn->SetActorHiddenInGame(true);
        MenuPawn->SetActorTickEnabled(false);
    }

    if (!GetLocalPlayer() || !GetLocalPlayer()->ViewportClient)
    {
        FinishPlayModeSelection(false);
        return;
    }

    TSharedPtr<SButton> PCButton;
    PlayModePrompt = SNew(SOverlay)
        + SOverlay::Slot()
        [
            SNew(SBorder)
            .BorderBackgroundColor(FLinearColor(0.02f, 0.02f, 0.02f, 0.95f))
            .HAlign(HAlign_Center).VAlign(VAlign_Center)
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(20.0f)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(TEXT("VR 기기가 연결되어 있습니다. VR로 플레이하시겠습니까?")))
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(20.0f)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth().Padding(8.0f)
                    [
                        SNew(SButton).ContentPadding(FMargin(24.0f, 12.0f))
                        .Text(FText::FromString(TEXT("VR로 플레이")))
                        .OnClicked(FOnClicked::CreateUObject(this, &AMainMenuPlayerController::ChooseVRMode))
                    ]
                    + SHorizontalBox::Slot().AutoWidth().Padding(8.0f)
                    [
                        SAssignNew(PCButton, SButton).ContentPadding(FMargin(24.0f, 12.0f))
                        .Text(FText::FromString(TEXT("PC로 플레이")))
                        .OnClicked(FOnClicked::CreateUObject(this, &AMainMenuPlayerController::ChoosePCMode))
                    ]
                ]
            ]
        ];
    GetLocalPlayer()->ViewportClient->AddViewportWidgetContent(PlayModePrompt.ToSharedRef(), 1000);
    FInputModeUIOnly InputMode;
    InputMode.SetWidgetToFocus(PCButton);
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(InputMode);
    bShowMouseCursor = true;
}

// 선택창을 닫거나 맵을 이동할 때 Slate 위젯을 정리합니다.
void AMainMenuPlayerController::RemovePlayModePrompt()
{
    if (PlayModePrompt.IsValid() && GetLocalPlayer() && GetLocalPlayer()->ViewportClient)
        GetLocalPlayer()->ViewportClient->RemoveViewportWidgetContent(PlayModePrompt.ToSharedRef());
    PlayModePrompt.Reset();
}

// VR 시작 실패 또는 선택 직전 연결 해제 시에는 PC 메뉴로 진입합니다.
void AMainMenuPlayerController::FinishPlayModeSelection(bool bUseVR)
{
    RemovePlayModePrompt();
    const bool bVRAvailable = bUseVR
        && UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayConnected()
        && UHeadMountedDisplayFunctionLibrary::EnableHMD(true);
    if (!bVRAvailable)
        UHeadMountedDisplayFunctionLibrary::EnableHMD(false);
    if (bUseVR && !bVRAvailable)
        UE_LOG(LogTemp, Warning, TEXT("[PlayMode] VR activation failed; using PC mode."));

    UIndianBabGameInstance* GI = Cast<UIndianBabGameInstance>(GetGameInstance());
    if (GI) GI->SetSelectedPlayMode(bVRAvailable);
    MenuInputMode = bVRAvailable ? EMainMenuInputMode::VR : EMainMenuInputMode::Mouse;

    // 로컬 메인 메뉴에서도 선택한 모드에 맞는 기존 캐릭터를 사용합니다.
    APawn* PreviousMenuPawn = GetPawn();
    const bool bNeedsPawn = !PreviousMenuPawn || (PreviousMenuPawn->IsA<ALobbyVRCharacter>() != bVRAvailable);
    if (HasAuthority() && bNeedsPawn && GI)
    {
        const TSubclassOf<APawn> PawnClass = GI->GetPlayModePawnClass(bVRAvailable);
        const FTransform SpawnTransform = PreviousMenuPawn ? PreviousMenuPawn->GetActorTransform() : FTransform(GetControlRotation(), GetSpawnLocation());
        FActorSpawnParameters Params;
        Params.Owner = this;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        if (APawn* NewPawn = GetWorld()->SpawnActor<APawn>(PawnClass, SpawnTransform, Params))
        {
            Possess(NewPawn);
            if (PreviousMenuPawn) PreviousMenuPawn->Destroy();
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("[PlayMode] Failed to spawn menu Pawn."));
        }
    }
    if (APawn* MenuPawn = GetPawn())
    {
        MenuPawn->SetActorHiddenInGame(false);
        MenuPawn->SetActorTickEnabled(true);
    }
    OpenMainMenu();
}

// 선택 버튼에서 VR 모드를 확정합니다.
FReply AMainMenuPlayerController::ChooseVRMode()
{
    FinishPlayModeSelection(true);
    return FReply::Handled();
}

// 선택 버튼에서 PC 모드를 확정합니다.
FReply AMainMenuPlayerController::ChoosePCMode()
{
    FinishPlayModeSelection(false);
    return FReply::Handled();
}
