#include "PlayerController/MainGamePlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "EnhancedInputComponent.h"
#include "GameInstanceSubsystem/SettingSubsystem.h"
#include "Widget/MainGameWidget.h"
#include "Widget/DeckLeftWidget.h"
#include "Game/MainGameMode.h"
#include "Game/MainGameTypes.h"
#include "Actor/Revolver.h"
#include "GameFramework/PlayerState.h"
#include "Character/LobbyCameraManager.h"
#include "Character/LobbyCharacter.h"
#include "Character/LobbyVRCharacter.h"
#include "InputCoreTypes.h"
#include "Interface/InteractableInterface.h"
#include "OnlineSubsystem.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "PlayerState/MainPlayerState.h"
#include "GameInstanceSubsystem/ConnectivitySubsystem.h"
#include "GameInstanceSubsystem/IndianBabGameInstance.h"
#include "Engine/GameInstance.h"
#include "Blueprint/UserWidget.h"
#include "Components/WidgetComponent.h"
#include "Components/WidgetSwitcher.h"


AMainGamePlayerController::AMainGamePlayerController()
{
	PlayerCameraManagerClass = ALobbyCameraManager::StaticClass();
}

void AMainGamePlayerController::ClientWasKicked_Implementation(const FText& KickReason)
{
    // 데디 KickPlayer reason 을 GameInstance 에 stash → OnNetworkFailure(ConnectionLost)
    // 가 곧바로 떨어져 CleanupHostSession 모달로 노출.
    const FString ReasonStr = KickReason.ToString();
    if (UIndianBabGameInstance* GI = Cast<UIndianBabGameInstance>(GetGameInstance()))
    {
        GI->SetPendingKickReason(ReasonStr);
    }
    UE_LOG(LogTemp, Warning, TEXT("[PC] ClientWasKicked reason=%s"), *ReasonStr);
}


void AMainGamePlayerController::BeginPlay()
{
    Super::BeginPlay();

    if(!IsLocalPlayerController()) 
        return;

    CreateMainGameWidget();
    CreateDeckLeftWidget();
    
    // 자동 착석이므로 주석 처리
    // EnterCameraMode();
	// ApplyLobbyMappingContext();

    // 자동 착석에 맞게 진입
    EnterUIMode();
    ApplyMainGameMappingContext();

    if (USettingSubsystem* SettingSS = GetGameInstance()->GetSubsystem<USettingSubsystem>())
    {
        LookSensitivity = SettingSS->GetMouseSensitivity();
    }

    // 자동 착석이므로 주석 처리
	// FInputModeGameOnly Mode;
	// SetInputMode(Mode);
	// bShowMouseCursor = false;

    TrySendSteamNickname();

    // 연결성 구독 + 폴링 시작 — 인게임에서도 NLM 폴링으로 로컬 끊김을 빠르게 감지.
    // NLA 사각지대(NLM online 인데 서버 unreachable) 는 NetDriver OnNetworkFailure → ForceTriggerLost 가 백업.
    if (UGameInstance* GI = GetGameInstance())
    {
        if (UConnectivitySubsystem* Connectivity = GI->GetSubsystem<UConnectivitySubsystem>())
        {
            LostHandle = Connectivity->OnConnectivityLost.AddUObject(
                this, &AMainGamePlayerController::HandleConnectivityLost);
            RestoredHandle = Connectivity->OnConnectivityRestored.AddUObject(
                this, &AMainGamePlayerController::HandleConnectivityRestored);

            Connectivity->StartPolling();
        }
    }
}

void AMainGamePlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (IsLocalPlayerController())
    {
        if (UGameInstance* GI = GetGameInstance())
        {
            if (UConnectivitySubsystem* Connectivity = GI->GetSubsystem<UConnectivitySubsystem>())
            {
                Connectivity->OnConnectivityLost.Remove(LostHandle);
                Connectivity->OnConnectivityRestored.Remove(RestoredHandle);
                Connectivity->StopPolling();
            }
        }
    }

    Super::EndPlay(EndPlayReason);
}

// 인터넷 끊김 또는 NetDriver disconnect (ForceTriggerLost 경유)
void AMainGamePlayerController::HandleConnectivityLost()
{
    if (!OfflineWidgetClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("MainGamePC: OfflineWidgetClass 설정되지 않았습니다!"));
        return;
    }

    if (!OfflineWidgetInstance)
    {
        OfflineWidgetInstance = CreateWidget<UUserWidget>(this, OfflineWidgetClass);
    }

    if (OfflineWidgetInstance && !OfflineWidgetInstance->IsInViewport())
    {
        OfflineWidgetInstance->AddToViewport(100); // ZOrder 높게 — 인게임 HUD 위에 표시

        // 오프라인 모달에만 포커스 — 인게임 입력 차단
        FInputModeUIOnly InputModeData;
        InputModeData.SetWidgetToFocus(OfflineWidgetInstance->TakeWidget());
        InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        SetInputMode(InputModeData);
        bShowMouseCursor = true;
    }
}

// 연결 복구 (Lost 카운트다운 중 회복된 경우)
void AMainGamePlayerController::HandleConnectivityRestored()
{
    if (OfflineWidgetInstance && OfflineWidgetInstance->IsInViewport())
    {
        OfflineWidgetInstance->RemoveFromParent();

        // 인게임 입력 모드 복구 — 카메라 모드 기본 (BeginPlay 와 동일)
        FInputModeGameOnly Mode;
        SetInputMode(Mode);
        bShowMouseCursor = false;
    }
}


void AMainGamePlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

	if (!IsLocalPlayerController()) 
        return;

    if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent))
    {
        if (IA_MainGameLook)
        {
            EnhancedInput->BindAction(IA_MainGameLook, ETriggerEvent::Triggered, this, &AMainGamePlayerController::OnMainGameLook);
        }
        if (IA_MainGameRMB)
        {
            EnhancedInput->BindAction(IA_MainGameRMB, ETriggerEvent::Started, this, &AMainGamePlayerController::OnMainGameRMBPressed);
            EnhancedInput->BindAction(IA_MainGameRMB, ETriggerEvent::Completed, this, &AMainGamePlayerController::OnMainGameRMBReleased);
        }
        if (IA_MainGameCheckCall)
        {
            EnhancedInput->BindAction(IA_MainGameCheckCall, ETriggerEvent::Started, this, &AMainGamePlayerController::OnMainGameCheckCall);
        }
        if (IA_MainGameFold)
        {
            EnhancedInput->BindAction(IA_MainGameFold, ETriggerEvent::Started, this, &AMainGamePlayerController::OnMainGameFold);
        }
        if (IA_MainGameRaise)
        {
            EnhancedInput->BindAction(IA_MainGameRaise, ETriggerEvent::Started, this, &AMainGamePlayerController::OnMainGameRaise);
        }

        if (IA_LobbyMove)
        {
            EnhancedInput->BindAction(IA_LobbyMove, ETriggerEvent::Triggered, this, &AMainGamePlayerController::OnLobbyMove);
        }

        if (IA_LobbyLook)
        {
            EnhancedInput->BindAction(IA_LobbyLook, ETriggerEvent::Triggered, this, &AMainGamePlayerController::OnLobbyLook);
        }

        if (IA_MainGameTab)
        {
            EnhancedInput->BindAction(IA_MainGameTab, ETriggerEvent::Started, this, &AMainGamePlayerController::OnMainGameTabPressed);
        }

        if (IA_Fire && IA_RightTriggerClick)
        {
            EnhancedInput->BindAction(IA_Fire, ETriggerEvent::Started, this, &AMainGamePlayerController::OnFire);
        }

        if (IA_RightTriggerClick)
        {
            UE_LOG(LogTemp, Warning, TEXT("[VR UI] IA_RightTriggerClick bound"));
            EnhancedInput->BindAction(IA_RightTriggerClick, ETriggerEvent::Started, this, &AMainGamePlayerController::OnRightTriggerClickStarted);
            EnhancedInput->BindAction(IA_RightTriggerClick, ETriggerEvent::Completed, this, &AMainGamePlayerController::OnRightTriggerClickReleased);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[VR UI] IA_RightTriggerClick is null"));
        }

        if (IA_LeftTriggerClick)
        {
            UE_LOG(LogTemp, Warning, TEXT("[VR UI] IA_LeftTriggerClick bound"));
            EnhancedInput->BindAction(IA_LeftTriggerClick, ETriggerEvent::Started, this, &AMainGamePlayerController::OnLeftTriggerClickStarted);
            EnhancedInput->BindAction(IA_LeftTriggerClick, ETriggerEvent::Completed, this, &AMainGamePlayerController::OnLeftTriggerClickReleased);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[VR UI] IA_LeftTriggerClick is null"));
        }

        if (!IA_RightTriggerClick && IA_Fire)
        {
            UE_LOG(LogTemp, Warning, TEXT("[VR UI] IA_Fire is used as temporary right trigger UI click fallback"));
            EnhancedInput->BindAction(IA_Fire, ETriggerEvent::Started, this, &AMainGamePlayerController::OnRightTriggerClickStarted);
            EnhancedInput->BindAction(IA_Fire, ETriggerEvent::Completed, this, &AMainGamePlayerController::OnRightTriggerClickReleased);
        }
        if (IA_MainGameInteract) 
        {
            EnhancedInput->BindAction(IA_MainGameInteract, ETriggerEvent::Started, this, &AMainGamePlayerController::OnMainGameInteract);
        }

		if (IA_MenuToggleLeft)
		{
			EnhancedInput->BindAction(IA_MenuToggleLeft, ETriggerEvent::Started, this, &AMainGamePlayerController::OnMenuToggleLeft);
			UE_LOG(LogTemp, Warning, TEXT("[VR UI] IA_MenuToggleLeft bound"));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[VR UI] IA_MenuToggleLeft is null"));
		}
    }
}


void AMainGamePlayerController::ApplyLobbyMappingContext()
{
    ULocalPlayer* LocalPlayer = GetLocalPlayer();
    if (!LocalPlayer)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Input] ApplyLobbyMappingContext failed. LocalPlayer is null"));
        return;
    }

    auto* Subsys = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
    if (!Subsys)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Input] ApplyLobbyMappingContext failed. EnhancedInput subsystem is null"));
        return;
    }

    // 나중에 게임 모드 별로 바꾸도록 변경 필요
    if (Subsys->HasMappingContext(MainGameMappingContext))
    {
        Subsys->RemoveMappingContext(MainGameMappingContext);
    }

    if (LobbyMappingContext)
    {
        Subsys->AddMappingContext(LobbyMappingContext, 0);
        UE_LOG(LogTemp, Warning, TEXT("[Input] LobbyMappingContext applied: %s"), *GetNameSafe(LobbyMappingContext));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[Input] LobbyMappingContext is null"));
    }
}


void AMainGamePlayerController::ApplyMainGameMappingContext()
{
    ULocalPlayer* LocalPlayer = GetLocalPlayer();
    if (!LocalPlayer) return;

    auto* Subsys = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
    if (!Subsys) return;

    if (Subsys->HasMappingContext(LobbyMappingContext))
    {
        Subsys->RemoveMappingContext(LobbyMappingContext);
	}

    if (MainGameMappingContext)
    {
        Subsys->AddMappingContext(MainGameMappingContext, 0);
    }
    
}


void AMainGamePlayerController::CreateMainGameWidget()
{
    if (!MainGameWidgetClass) return;
    
    if (!MainGameWidgetInstance)
    {
        MainGameWidgetInstance = CreateWidget<UMainGameWidget>(this, MainGameWidgetClass);
    }

    if (MainGameWidgetInstance && !MainGameWidgetInstance->IsInViewport())
    {
        MainGameWidgetInstance->AddToViewport();
    }

    if (MainGameWidgetInstance)
    {
        MainGameWidgetInstance->InitWidget();
        if (Cast<ALobbyVRCharacter>(GetPawn()))
        {
            MainGameWidgetInstance->SetVisibility(ESlateVisibility::Hidden);
        }
    }
}

void AMainGamePlayerController::CreateDeckLeftWidget() 
{
    if (!DeckLeftWidgetClass) return;

    if (!DeckLeftWidgetInstance) 
    {
        DeckLeftWidgetInstance = CreateWidget<UDeckLeftWidget>(this, DeckLeftWidgetClass);
    }
    if (DeckLeftWidgetInstance && !DeckLeftWidgetInstance->IsInViewport()) 
    {
        DeckLeftWidgetInstance->AddToViewport();
    }
    if (DeckLeftWidgetInstance) 
    {
        DeckLeftWidgetInstance->InvisibleWidget();
    }
}

void AMainGamePlayerController::EnterUIMode()
{
    bShowMouseCursor = true;
    bEnableClickEvents = true;
    bEnableMouseOverEvents = true;

    FInputModeGameAndUI Mode;
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(Mode);

    SetIgnoreLookInput(true);
}


void AMainGamePlayerController::EnterCameraMode()
{
    bShowMouseCursor = false;
    bEnableClickEvents = false;
    bEnableMouseOverEvents = false;

    FInputModeGameOnly Mode;
    SetInputMode(Mode);
    SetIgnoreLookInput(false);
}


void AMainGamePlayerController::RequestRaise(int32 RaiseCount)
{
    Server_RequestBetAction(EBetAction::Raise, RaiseCount);
}


void AMainGamePlayerController::RequestCheckCall()
{
    Server_RequestBetAction(EBetAction::CheckCall, 0);
}


void AMainGamePlayerController::RequestFold()
{
    Server_RequestBetAction(EBetAction::Fold, 0);
}

// 내 스팀 닉네임 읽기
FString AMainGamePlayerController::GetMySteamNickname() const
{
    IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
    if (!Subsystem) return TEXT("");

    IOnlineIdentityPtr Identity = Subsystem->GetIdentityInterface();
    if (Identity.IsValid())
    {
        return Identity->GetPlayerNickname(0);
    }

    return TEXT("");
}

void AMainGamePlayerController::TrySendSteamNickname()
{
    if (bSteamNicknameSent) return;

    if (!IsLocalPlayerController()) return;

    AMainPlayerState* PS = GetPlayerState<AMainPlayerState>();
    if (!PS) return;

    const FString MyNickname = GetMySteamNickname();
    if (MyNickname.IsEmpty()) return;

    Server_SetSteamNickname(MyNickname);
    bSteamNicknameSent = true;
}

void AMainGamePlayerController::OnMainGameLook(const FInputActionValue& Value)
{
    if (!bRMBHeld) return;

    const FVector2D LookAxis = Value.Get<FVector2D>();

    AddYawInput(LookAxis.X * LookSensitivity);
    AddPitchInput(-LookAxis.Y * LookSensitivity);
}


void AMainGamePlayerController::OnMainGameRMBPressed(const FInputActionValue& Value)
{
    bRMBHeld = true;
    EnterCameraMode();
}


void AMainGamePlayerController::OnMainGameRMBReleased(const FInputActionValue& Value)
{
    bRMBHeld = false;
    EnterUIMode();
}


void AMainGamePlayerController::OnMainGameCheckCall(const FInputActionValue& Value)
{
    RequestCheckCall();
}


void AMainGamePlayerController::OnMainGameFold(const FInputActionValue& Value)
{
    RequestFold();
}


void AMainGamePlayerController::OnMainGameRaise(const FInputActionValue& Value)
{
    RequestRaise(MainGameWidgetInstance->GetBetNum());
}


void AMainGamePlayerController::OnLobbyMove(const FInputActionValue& Value)
{
    const FVector2D MoveAxis = Value.Get<FVector2D>();
    if (APawn* MyPawn = GetPawn())
    {
        MyPawn->AddMovementInput(MyPawn->GetActorForwardVector(), MoveAxis.Y);
        MyPawn->AddMovementInput(MyPawn->GetActorRightVector(), MoveAxis.X);
    }
}


void AMainGamePlayerController::OnLobbyLook(const FInputActionValue& Value)
{
    const FVector2D LookAxis = Value.Get<FVector2D>();
    AddYawInput(LookAxis.X * LookSensitivity);
    AddPitchInput(-LookAxis.Y * LookSensitivity);
}

void AMainGamePlayerController::OnRep_PlayerState()
{
    Super::OnRep_PlayerState();

    if (MainGameWidgetInstance)
    {
        MainGameWidgetInstance->InitWidget();
    }

    TrySendSteamNickname();
}

void AMainGamePlayerController::Server_RequestBetAction_Implementation(EBetAction Action, int32 RaiseCount)
{
#if WITH_SERVER_CODE
    AMainGameMode* GM = GetWorld() -> GetAuthGameMode<AMainGameMode>();
    if(!GM) return;

    GM->HandleBetAction(this, Action, RaiseCount);
#endif

}

void AMainGamePlayerController::Server_SetSteamNickname_Implementation(const FString& NewNickname)
{
    AMainPlayerState* PS = GetPlayerState<AMainPlayerState>();
    if (!PS) return;

    PS->SetSteamNickname(NewNickname);
}

void AMainGamePlayerController::Server_RequestMainRevolverShot_Implementation()
{
#if WITH_SERVER_CODE
    AMainGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AMainGameMode>() : nullptr;
	if (!GM) return;

	GM->HandleMainRevolverShotAction(this);
#endif
}

// 앉았을 때 대기화면 생성
void AMainGamePlayerController::ClientOnSeated_Implementation()
{
    // 로비 조작(WASD)을 끄고 메인 게임(마우스/UI) 조작으로 스위칭(기존)
    // 현재는 자동 착석시작이라 메인 게임 조작으로 시작
    ApplyMainGameMappingContext();

    // 착석 시(게임 시작 시) 메인 게임 위젯 생성
    if (!MainGameWidgetInstance) CreateMainGameWidget();

    // ReadyButton만 보이게
    MainGameWidgetInstance->SetVisibility(ESlateVisibility::Visible);
    MainGameWidgetInstance->SetPCReadyMode(true);
}

// 접속 인원 모두 레디를 눌렀을 때
void AMainGamePlayerController::Client_FinishPCReady_Implementation()
{
    // 메인 게임 위젯 보이게
    if (MainGameWidgetInstance)
    {
        MainGameWidgetInstance->SetPCReadyMode(false);
        MainGameWidgetInstance->SetVisibility(ESlateVisibility::Visible);
    }
}

void AMainGamePlayerController::Server_RequestReady_Implementation()
{
#if WITH_SERVER_CODE
	UE_LOG(LogTemp, Warning, TEXT("[PC] Server_RequestReady called"));

	AMainGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AMainGameMode>() : nullptr;
	if (!GM) return;

	GM->HandlePlayerReady(this);
#endif
}

int AMainGamePlayerController::GetPlayerIdSafe()
{
    const APlayerState* PS = GetPlayerState<APlayerState>();
    return PS ? PS->GetPlayerId() : -1;
}

void AMainGamePlayerController::OnMainGameTabPressed(const FInputActionValue& Value)
{

    if (DeckLeftWidgetInstance)
    {
        DeckLeftWidgetInstance->VisibleWidget();
    }
}

void AMainGamePlayerController::OnFire(const FInputActionValue& Value)
{
	Server_RequestMainRevolverShot();
}

void AMainGamePlayerController::OnRightTriggerClickStarted(const FInputActionValue& Value)
{
	if (ALobbyVRCharacter* VRCharacter = Cast<ALobbyVRCharacter>(GetPawn()))
	{
        if (VRCharacter->GunHoldReason == EGunHoldReason::Win
            && VRCharacter->ActiveRevolver
            && VRCharacter->ActiveRevolver->ActorHasTag(FName("MainRevolver")))
        {
            UE_LOG(LogTemp, Warning, TEXT("[VR Gun] Right trigger used as main revolver fire. PC=%s Pawn=%s Revolver=%s"),
                *GetNameSafe(this),
                *GetNameSafe(VRCharacter),
                *GetNameSafe(VRCharacter->ActiveRevolver));
            Server_RequestMainRevolverShot();
            return;
        }

		VRCharacter->PressRightWidgetInteraction();
	}
}

void AMainGamePlayerController::OnRightTriggerClickReleased(const FInputActionValue& Value)
{
	if (ALobbyVRCharacter* VRCharacter = Cast<ALobbyVRCharacter>(GetPawn()))
	{
		VRCharacter->ReleaseRightWidgetInteraction();
	}
}

void AMainGamePlayerController::OnLeftTriggerClickStarted(const FInputActionValue& Value)
{
	if (ALobbyVRCharacter* VRCharacter = Cast<ALobbyVRCharacter>(GetPawn()))
	{
		VRCharacter->PressLeftWidgetInteraction();
	}
}

void AMainGamePlayerController::OnLeftTriggerClickReleased(const FInputActionValue& Value)
{
	if (ALobbyVRCharacter* VRCharacter = Cast<ALobbyVRCharacter>(GetPawn()))
	{
		VRCharacter->ReleaseLeftWidgetInteraction();
	}
}

void AMainGamePlayerController::OnMenuToggleLeft(const FInputActionValue& Value)
{
	ALobbyVRCharacter* VRCharacter = Cast<ALobbyVRCharacter>(GetPawn());
	if (!VRCharacter || !VRCharacter->IsLocallyControlled())
	{
		return;
	}

	TArray<UWidgetComponent*> WidgetComponents;
	VRCharacter->GetComponents<UWidgetComponent>(WidgetComponents);

	for (UWidgetComponent* WidgetComponent : WidgetComponents)
	{
		if (!WidgetComponent)
		{
			continue;
		}

		const FString ComponentName = WidgetComponent->GetName();
		const bool bIsMenuWidget =
			ComponentName.Contains(TEXT("VRMenuWidget"), ESearchCase::IgnoreCase);

		if (!bIsMenuWidget)
		{
			continue;
		}

		WidgetComponent->InitWidget();

		UUserWidget* MasterMenu = WidgetComponent->GetUserWidgetObject();
		UWidget* WidgetMenuPage = MasterMenu
			? MasterMenu->GetWidgetFromName(TEXT("WidgetMenu"))
			: nullptr;
		UWidgetSwitcher* MenuSwitcher = WidgetMenuPage
			? Cast<UWidgetSwitcher>(WidgetMenuPage->GetParent())
			: nullptr;

		if (!MasterMenu || !WidgetMenuPage || !MenuSwitcher)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[VR UI] Failed to open WidgetMenu. Component=%s Master=%s Page=%s Switcher=%s"),
				*ComponentName,
				*GetNameSafe(MasterMenu),
				*GetNameSafe(WidgetMenuPage),
				*GetNameSafe(MenuSwitcher));
			return;
		}

		const bool bMenuCurrentlyOpen =
			WidgetComponent->IsVisible() &&
			!WidgetComponent->bHiddenInGame &&
			MenuSwitcher->GetActiveWidget() == WidgetMenuPage &&
			WidgetMenuPage->IsVisible();
		const bool bShowMenu = !bMenuCurrentlyOpen;

		if (bShowMenu)
		{
			// A button may have hidden this page or switched away from it.
			// Restore both states whenever the menu is opened again.
			WidgetMenuPage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			MenuSwitcher->SetActiveWidget(WidgetMenuPage);
		}

		WidgetComponent->SetVisibility(bShowMenu, true);
		WidgetComponent->SetHiddenInGame(!bShowMenu);
		WidgetComponent->SetCollisionEnabled(
			bShowMenu ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
		WidgetComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
		WidgetComponent->SetCollisionResponseToChannel(
			ECC_Visibility,
			bShowMenu ? ECR_Block : ECR_Ignore);
		WidgetComponent->SetGenerateOverlapEvents(false);

		UE_LOG(LogTemp, Warning, TEXT("[VR UI] %s %s"),
			*ComponentName,
			bShowMenu ? TEXT("shown") : TEXT("hidden"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[VR UI] Menu widget component not found on %s"), *GetNameSafe(VRCharacter));
}

bool AMainGamePlayerController::CloseVRMenu()
{
	ALobbyVRCharacter* VRCharacter = Cast<ALobbyVRCharacter>(GetPawn());
	if (!VRCharacter || !VRCharacter->IsLocallyControlled())
	{
		return false;
	}

	TArray<UWidgetComponent*> WidgetComponents;
	VRCharacter->GetComponents<UWidgetComponent>(WidgetComponents);

	for (UWidgetComponent* WidgetComponent : WidgetComponents)
	{
		if (!WidgetComponent ||
			!WidgetComponent->GetName().Contains(TEXT("VRMenuWidget"), ESearchCase::IgnoreCase))
		{
			continue;
		}

		WidgetComponent->SetVisibility(false, true);
		WidgetComponent->SetHiddenInGame(true);
		WidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		WidgetComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
		WidgetComponent->SetGenerateOverlapEvents(false);

		UE_LOG(LogTemp, Warning, TEXT("[VR UI] VR menu closed; returning to gameplay"));
		return true;
	}

	return false;
}

void AMainGamePlayerController::OnDebugRightTriggerPressed()
{
	UE_LOG(LogTemp, Warning, TEXT("[VR UI] Debug R press"));
	if (ALobbyVRCharacter* VRCharacter = Cast<ALobbyVRCharacter>(GetPawn()))
	{
		VRCharacter->PressRightWidgetInteraction();
	}
}

void AMainGamePlayerController::OnDebugRightTriggerReleased()
{
	UE_LOG(LogTemp, Warning, TEXT("[VR UI] Debug R release"));
	if (ALobbyVRCharacter* VRCharacter = Cast<ALobbyVRCharacter>(GetPawn()))
	{
		VRCharacter->ReleaseRightWidgetInteraction();
	}
}

void AMainGamePlayerController::OnMainGameInteract(const FInputActionValue& Value)
{
    if(ALobbyVRCharacter* VRCharacter = Cast<ALobbyVRCharacter>(GetPawn()))
    {
        VRCharacter->GrabGun();
    }
}
