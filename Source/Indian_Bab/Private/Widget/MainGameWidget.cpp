#include "Widget/MainGameWidget.h"
#include "Widget/DeckLeftWidget.h"
#include "Components/TextBlock.h"
#include "PlayerState/MainPlayerState.h"
#include "Components/EditableTextBox.h"
//#include "Components/MultiLineEditableText.h"
#include "Components/Button.h"
#include "Widget/BetProgressWidget.h"
#include "Game/MainGameState.h"
#include "PlayerController\MainGamePlayerController.h"

namespace
{
	constexpr int32 MaxRaiseBetNum = 7;
}

void UMainGameWidget::UpdateCenterBetLog(const FString& Message)
{
	if (!Txt_BetLog) return;

	// 1. 텍스트 설정
	Txt_BetLog->SetText(FText::FromString(Message));

	// 2. 기존 타이머가 작동 중이었다면 중복 방지를 위해 초기화
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(BetLogTimerHandle);

		// 3. 3.0초 뒤에 ClearBetLog 함수를 호출하도록 타이머 세팅
		GetWorld()->GetTimerManager().SetTimer(
			BetLogTimerHandle,
			this,
			&UMainGameWidget::ClearBetLog,
			3.0f,
			false
		);
	}
}

void UMainGameWidget::ClearBetLog()
{
	if (Txt_BetLog)
	{
		Txt_BetLog->SetText(FText::GetEmpty());
	}
}

void UMainGameWidget::NativeDestruct()
{
	Super::NativeDestruct();
	
    // 자기 자신의 BindWidget UObject들 — 재오픈 대비 일괄 해제.
    if (Minus_Button)     Minus_Button->OnClicked.RemoveAll(this);
    if (Plus_Button)      Plus_Button->OnClicked.RemoveAll(this);
    if (Button_Raise)     Button_Raise->OnClicked.RemoveAll(this);
    if (Button_CheckCall) Button_CheckCall->OnClicked.RemoveAll(this);
    if (Button_Fold)      Button_Fold->OnClicked.RemoveAll(this);
    if (Button_Ready)     Button_Ready->OnClicked.RemoveAll(this);

    // 외부 객체 구독 해제
    if (MainPS)
    {
        MainPS->OnTriggerCountChanged.RemoveAll(this);
    }
}

void UMainGameWidget::OperateTimer() {
	if (!Time) return;

	AMainGameState* GS = GetWorld() ? GetWorld()->GetGameState<AMainGameState>() : nullptr;
	if (!GS) return;

	if (!GS->bTimerActive)
	{
		Time->SetText(FText::FromString(TEXT("-")));
		return;
	}

	Time->SetText(FText::AsNumber(GS->GetRemainingTimeCeil()));
}

// PC 모드에서의 Ready 상태
void UMainGameWidget::SetPCReadyMode(bool bWaitingForReady)
{
    if (bWaitingForReady && !bPCReadyMode) bPCReadySubmitted = false;
    bPCReadyMode = bWaitingForReady;
    if (Button_Ready)
    {
        Button_Ready->SetVisibility(bPCReadyMode && !bPCReadySubmitted ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
        Button_Ready->SetIsEnabled(bPCReadyMode && !bPCReadySubmitted);
    }
    else if (bPCReadyMode)
    {
        UE_LOG(LogTemp, Error, TEXT("WBP_MainGame needs a Button named Button_Ready."));
    }
    // PC 컨트롤러에서만 호출, VR은 기존 게임 UI 처리를 유지
    if (Button_Raise) Button_Raise->SetIsEnabled(!bPCReadyMode);
    if (Button_CheckCall) Button_CheckCall->SetIsEnabled(!bPCReadyMode);
    if (Button_Fold) Button_Fold->SetIsEnabled(!bPCReadyMode);
    if (Plus_Button) Plus_Button->SetIsEnabled(!bPCReadyMode);
    if (Minus_Button) Minus_Button->SetIsEnabled(!bPCReadyMode);
}

// Ready 버튼 눌렀을 때
void UMainGameWidget::OnReadyClicked()
{
    if (!bPCReadyMode || bPCReadySubmitted) return;
    AMainGamePlayerController* PC = Cast<AMainGamePlayerController>(GetOwningPlayer());
    if (!PC || !PC->IsLocalController()) return;
    bPCReadySubmitted = true;
    if (Button_Ready)
    {
        Button_Ready->SetIsEnabled(false);
        Button_Ready->SetVisibility(ESlateVisibility::Collapsed);
    }
    PC->Server_RequestReady();
}

void UMainGameWidget::NativeConstruct() 
{
	Super::NativeConstruct();
    if (Button_Ready)
    {
        Button_Ready->OnClicked.RemoveAll(this);
        Button_Ready->OnClicked.AddDynamic(this, &UMainGameWidget::OnReadyClicked);
        Button_Ready->SetVisibility(bPCReadyMode && !bPCReadySubmitted ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
        Button_Ready->SetIsEnabled(bPCReadyMode && !bPCReadySubmitted);
    }


	if (Minus_Button) 
	{
		Minus_Button->OnClicked.AddDynamic(this, &UMainGameWidget::MinusButtonClicked);
	}

	if (Plus_Button) 
	{
		Plus_Button->OnClicked.AddDynamic(this, &UMainGameWidget::PlusButtonClicked);
	}

	// 정리는 NativeDestruct에서 일괄 — Construct는 Add만.
	if (Button_Raise)
	{
		Button_Raise->OnClicked.AddDynamic(this, &UMainGameWidget::OnButtonRaise);
	}
	if (Button_CheckCall)
	{
		Button_CheckCall->OnClicked.AddDynamic(this, &UMainGameWidget::OnButtonCheckCall);
	}
	if (Button_Fold)
	{
		Button_Fold->OnClicked.AddDynamic(this, &UMainGameWidget::OnButtonFold);
	}
	if (BetCount)
	{
		BetCount->SetText(FText::AsNumber(BetNum));
		WBP_BetProgress->SetPerCent(-0.125f);
	}
	MainGamePC = Cast<AMainGamePlayerController>(GetOwningPlayer());

}

void UMainGameWidget::MinusButtonClicked()
{
	if (BetNum <= 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("[VR UI] MainGameWidget clicked: Minus blocked BetNum=%d OwnerPC=%s"),
			BetNum,
			*GetNameSafe(MainGamePC));
		return;
	}

	BetNum--;
	UE_LOG(LogTemp, Warning, TEXT("[VR UI] MainGameWidget clicked: Minus BetNum=%d OwnerPC=%s"),
		BetNum,
		*GetNameSafe(MainGamePC));

	if (BetCount)
	{
		BetCount->SetText(FText::AsNumber(BetNum));
	}

	if (WBP_BetProgress)
	{
		WBP_BetProgress->SetPerCent(0.125f);
	}
}

void UMainGameWidget::PlusButtonClicked()
{
	if (BetNum >= MaxRaiseBetNum)
	{
		UE_LOG(LogTemp, Warning, TEXT("[VR UI] MainGameWidget clicked: Plus blocked BetNum=%d OwnerPC=%s"),
			BetNum,
			*GetNameSafe(MainGamePC));
		return;
	}

	BetNum++;
	UE_LOG(LogTemp, Warning, TEXT("[VR UI] MainGameWidget clicked: Plus BetNum=%d OwnerPC=%s"),
		BetNum,
		*GetNameSafe(MainGamePC));

	if (BetCount)
	{
		BetCount->SetText(FText::AsNumber(BetNum));
	}

	if (WBP_BetProgress)
	{
		if (BetNum == MaxRaiseBetNum)
		{
			WBP_BetProgress->Fill();
		}
		else
		{
			WBP_BetProgress->SetPerCent(-0.125f);
		}
	}
}

void UMainGameWidget::OnButtonRaise()
{

	if (!MainGamePC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[VR UI] MainGameWidget clicked: Raise blocked OwnerPC=None BetNum=%d"), BetNum);
		return;
	}

	if (BetNum < 1 || BetNum > MaxRaiseBetNum)
	{
		UE_LOG(LogTemp, Warning, TEXT("[VR UI] MainGameWidget clicked: Raise blocked BetNum=%d OwnerPC=%s"),
			BetNum,
			*GetNameSafe(MainGamePC));
		return;
	}
	
	UE_LOG(LogTemp, Warning, TEXT("[VR UI] MainGameWidget clicked: Raise BetNum=%d OwnerPC=%s"),
		BetNum,
		*GetNameSafe(MainGamePC));
	MainGamePC->RequestRaise(BetNum);
}

void UMainGameWidget::OnButtonCheckCall()
{
	if (!MainGamePC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[VR UI] MainGameWidget clicked: CheckCall blocked OwnerPC=None BetNum=%d"), BetNum);
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[VR UI] MainGameWidget clicked: CheckCall BetNum=%d OwnerPC=%s"),
		BetNum,
		*GetNameSafe(MainGamePC));
	MainGamePC->RequestCheckCall();
}

void UMainGameWidget::OnButtonFold()
{
	if (!MainGamePC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[VR UI] MainGameWidget clicked: Fold blocked OwnerPC=None BetNum=%d"), BetNum);
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[VR UI] MainGameWidget clicked: Fold BetNum=%d OwnerPC=%s"),
		BetNum,
		*GetNameSafe(MainGamePC));
	MainGamePC->RequestFold();
}


void UMainGameWidget::UpdateSubRevolverCount(int32 Count)
{
    //UE_LOG(LogTemp, Warning, TEXT("[Widget] UpdateSubRevolverCount called : %d"), Count);


	if (!SubRevolverCount) return;

	SubRevolverCount->SetText(
		FText::FromString(FString::Printf(TEXT("%d"), 8 - Count))
	);
}

void UMainGameWidget::InitWidget()
{    
    MainGamePC = Cast<AMainGamePlayerController>(GetOwningPlayer());
    if (!MainGamePC) return;

    MainPS = MainGamePC->GetPlayerState<AMainPlayerState>();
    if (!MainPS) return;

    MainPS->OnTriggerCountChanged.RemoveAll(this);
    MainPS->OnTriggerCountChanged.AddUObject(this, &UMainGameWidget::UpdateSubRevolverCount);

    UpdateSubRevolverCount(MainPS->TotalTriggerCount);
}

int32 UMainGameWidget::GetBetNum() const
{
	return BetNum;
}

bool UMainGameWidget::HandleVRClickAtWidgetLocation(const FVector2D& WidgetLocalHitLocation)
{
	if (IsButtonUnderWidgetLocation(Plus_Button, WidgetLocalHitLocation))
	{
		PlusButtonClicked();
		return true;
	}

	if (IsButtonUnderWidgetLocation(Minus_Button, WidgetLocalHitLocation))
	{
		MinusButtonClicked();
		return true;
	}

	if (IsButtonUnderWidgetLocation(Button_Raise, WidgetLocalHitLocation))
	{
		OnButtonRaise();
		return true;
	}

	if (IsButtonUnderWidgetLocation(Button_CheckCall, WidgetLocalHitLocation))
	{
		OnButtonCheckCall();
		return true;
	}

	if (IsButtonUnderWidgetLocation(Button_Fold, WidgetLocalHitLocation))
	{
		OnButtonFold();
		return true;
	}

	return false;
}

bool UMainGameWidget::IsButtonUnderWidgetLocation(const UButton* Button, const FVector2D& WidgetLocalHitLocation) const
{
	if (!Button || !Button->GetIsEnabled() || !Button->IsVisible())
	{
		return false;
	}

	const FVector2D AbsoluteHitLocation = GetCachedGeometry().LocalToAbsolute(WidgetLocalHitLocation);
	return Button->GetCachedGeometry().IsUnderLocation(AbsoluteHitLocation);
}
