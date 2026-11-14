#include "Widget/RoomJoinWidget.h"
#include "Widget/RoomListEntryData.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/ListView.h"
#include "GameInstanceSubsystem/SessionSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Widget/MainMenuWidget.h"
#include "PlayerController/MainMenuPlayerController.h"


void URoomJoinWidget::SetParentMenu(UUserWidget* InParentMenu)
{
	ParentMenu = InParentMenu;
}


void URoomJoinWidget::NativeConstruct()
{
	Super::NativeConstruct();

	PlayerControllerRef = GetOwningPlayer();

	if (GetGameInstance())
	{
		SessionSubsystem = GetGameInstance()->GetSubsystem<USessionSubsystem>();
	}

	if (SessionSubsystem)
	{
		SessionSubsystem->OnJoinSessionCompleteEvent.RemoveDynamic(this, &URoomJoinWidget::OnJoinSessionComplete);
		SessionSubsystem->OnJoinSessionCompleteEvent.AddDynamic(this, &URoomJoinWidget::OnJoinSessionComplete);

		SessionSubsystem->OnRoomsFoundEvent.RemoveDynamic(this, &URoomJoinWidget::OnRoomsFound);
		SessionSubsystem->OnRoomsFoundEvent.AddDynamic(this, &URoomJoinWidget::OnRoomsFound);

		SessionSubsystem->OnSessionErrorEvent.RemoveDynamic(this, &URoomJoinWidget::OnSessionError);
		SessionSubsystem->OnSessionErrorEvent.AddDynamic(this, &URoomJoinWidget::OnSessionError);
	}

	// 바인딩은 Add만, 정리는 NativeDestruct에서 일괄.
	// 버튼 상태는 매번 깨끗하게 리셋 — OnYesClicked/OnRefreshClicked로 disable된 채 박제 방지.
	if (Button_Yes)
	{
		Button_Yes->OnClicked.AddDynamic(this, &URoomJoinWidget::OnYesClicked);
		// 초기엔 선택 없음 → 참가 비활성
		Button_Yes->SetIsEnabled(false);
	}
	if (Button_No)
	{
		Button_No->OnClicked.AddDynamic(this, &URoomJoinWidget::OnNoClicked);
		Button_No->SetIsEnabled(true);
	}
	if (Button_Refresh)
	{
		Button_Refresh->OnClicked.AddDynamic(this, &URoomJoinWidget::OnRefreshClicked);
		Button_Refresh->SetIsEnabled(true);
	}

	if (ListView_Rooms)
	{
		ListView_Rooms->OnItemClicked().AddUObject(this, &URoomJoinWidget::OnRoomItemClicked);
	}

	// 진입 시 자동 1회 검색
	OnRefreshClicked();
}


void URoomJoinWidget::NativeDestruct()
{
	// 자기 자신의 BindWidget UObject들 — 누적 방지 위해 RemoveAll로 일괄 해제.
	if (Button_Yes)     Button_Yes->OnClicked.RemoveAll(this);
	if (Button_No)      Button_No->OnClicked.RemoveAll(this);
	if (Button_Refresh) Button_Refresh->OnClicked.RemoveAll(this);
	if (ListView_Rooms) ListView_Rooms->OnItemClicked().RemoveAll(this);

	// 외부 객체 구독 해제 (dangling 방지)
	if (SessionSubsystem)
	{
		SessionSubsystem->OnJoinSessionCompleteEvent.RemoveDynamic(this, &URoomJoinWidget::OnJoinSessionComplete);
		SessionSubsystem->OnRoomsFoundEvent.RemoveDynamic(this, &URoomJoinWidget::OnRoomsFound);
		SessionSubsystem->OnSessionErrorEvent.RemoveDynamic(this, &URoomJoinWidget::OnSessionError);
	}

	Super::NativeDestruct();
}


FReply URoomJoinWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		OnNoClicked();
		return FReply::Handled();
	}
	return FReply::Unhandled();
}


FReply URoomJoinWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	FReply Reply = Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);

	// VR 모드에서 SetInputMode(UIOnly)는 WidgetInteractionComponent 시뮬 키 라우팅을 깨뜨림 — Mouse 모드만 포커스 회복 적용
	const AMainMenuPlayerController* MainMenuPC = Cast<AMainMenuPlayerController>(PlayerControllerRef);
	if (MainMenuPC && !MainMenuPC->IsMouseMenuInputMode())
	{
		return FReply::Handled();
	}

	if (PlayerControllerRef)
	{
		FInputModeUIOnly InputModeData;
		InputModeData.SetWidgetToFocus(this->TakeWidget());
		PlayerControllerRef->SetInputMode(InputModeData);
	}

	return FReply::Handled();
}


void URoomJoinWidget::OnRefreshClicked()
{
	if (!SessionSubsystem)
	{
		SetStatusText(TEXT("Session unavailable."), FLinearColor::Red);
		return;
	}

	SelectedSearchIndex = INDEX_NONE;
	if (Button_Yes) Button_Yes->SetIsEnabled(false);
	if (ListView_Rooms) ListView_Rooms->ClearListItems();

	SetStatusText(TEXT("Searching rooms..."), FLinearColor::White);

	if (Button_Refresh) Button_Refresh->SetIsEnabled(false);
	SessionSubsystem->FindRooms();
}


void URoomJoinWidget::OnRoomsFound(bool bWasSuccessful, const TArray<FRoomListEntry>& Rooms)
{
	if (Button_Refresh) Button_Refresh->SetIsEnabled(true);

	if (!ListView_Rooms)
	{
		return;
	}

	ListView_Rooms->ClearListItems();

	if (!bWasSuccessful)
	{
		SetStatusText(TEXT("Search failed."), FLinearColor::Red);
		return;
	}

	if (Rooms.Num() == 0)
	{
		SetStatusText(TEXT("No rooms found."), FLinearColor::Gray);
		return;
	}

	for (const FRoomListEntry& E : Rooms)
	{
		URoomListEntryData* Data = NewObject<URoomListEntryData>(this);
		Data->Entry = E;
		Data->bJoinable = !E.bGameStarted;
		ListView_Rooms->AddItem(Data);
	}

	SetStatusText(FString::Printf(TEXT("%d room(s) found."), Rooms.Num()), FLinearColor::White);
}


void URoomJoinWidget::OnRoomItemClicked(UObject* Item)
{
	if (SessionSubsystem && SessionSubsystem->IsJoiningRoom()) return;
	URoomListEntryData* Data = Cast<URoomListEntryData>(Item);
	if (Data == nullptr || !Data->bJoinable)
	{
		SelectedSearchIndex = INDEX_NONE;
		if (Button_Yes) Button_Yes->SetIsEnabled(false);
		return;
	}

	SelectedSearchIndex = Data->Entry.SearchResultIndex;
	if (Button_Yes) Button_Yes->SetIsEnabled(true);
}


void URoomJoinWidget::OnYesClicked()
{
	if (!SessionSubsystem || SelectedSearchIndex == INDEX_NONE)
	{
		SetStatusText(TEXT("Select a room first."), FLinearColor::Red);
		return;
	}

	Button_Yes->SetIsEnabled(false);
	if (Button_No) Button_No->SetIsEnabled(false);
	if (Button_Refresh) Button_Refresh->SetIsEnabled(false);

	SetStatusText(TEXT("Joining..."), FLinearColor::White);

	SessionSubsystem->JoinRoomByIndex(SelectedSearchIndex);
}


void URoomJoinWidget::OnJoinSessionComplete(bool bWasSuccessful)
{
	if (bWasSuccessful)
	{
		SetStatusText(TEXT("Joined. Traveling..."), FLinearColor::Green);
		// 레벨 이동은 SessionSubsystem 내부에서 ClientTravel 처리됨
		return;
	}

	// 실패 → UI 복구
	if (Button_No) Button_No->SetIsEnabled(true);
	if (Button_Refresh) Button_Refresh->SetIsEnabled(true);
	// Yes 버튼은 선택이 살아있어야 다시 활성
	if (Button_Yes) Button_Yes->SetIsEnabled(SelectedSearchIndex != INDEX_NONE);

	SetStatusText(TEXT("Failed to join."), FLinearColor::Red);
}


void URoomJoinWidget::OnNoClicked()
{
	// ESC도 참가 처리 중 비활성화된 취소 버튼과 같은 규칙을 따릅니다.
	if (Button_No && !Button_No->GetIsEnabled()) return;
	if (UMainMenuWidget* MainMenu = Cast<UMainMenuWidget>(ParentMenu))
	{
		if (!IsInViewport())
		{
			MainMenu->ShowMainMenuRoot();
			return;
		}
	}

	if (ParentMenu && PlayerControllerRef)
	{
		FInputModeGameAndUI InputModeData;
		InputModeData.SetWidgetToFocus(ParentMenu->TakeWidget());
		InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::LockInFullscreen);
		PlayerControllerRef->SetInputMode(InputModeData);
	}
	RemoveFromParent();
}


// 세션 에러 수신 — 위젯은 닫지 않고 버튼만 재활성화 (사용자가 재시도/취소 선택 가능).
// 사유 표시는 MainMenuWidget의 SessionErrorWidget 모달이 위에 떠서 담당. 모달 닫히면
// MainMenuWidget::RefocusSelf가 TopmostChildModal(=이 위젯)로 포커스 되돌려줌.
void URoomJoinWidget::OnSessionError(const FString& Reason)
{
	UE_LOG(LogTemp, Warning, TEXT("RoomJoinWidget: session error received, keeping widget open. reason=%s"), *Reason);

	// OnYesClicked/OnRefreshClicked에서 disable된 버튼 복원.
	if (Button_No) Button_No->SetIsEnabled(true);
	if (Button_Refresh) Button_Refresh->SetIsEnabled(true);
	if (Button_Yes) Button_Yes->SetIsEnabled(SelectedSearchIndex != INDEX_NONE);

	SetStatusText(TEXT("Session error."), FLinearColor::Red);
}


void URoomJoinWidget::SetStatusText(const FString& Msg, const FLinearColor& Color)
{
	if (Text_Status)
	{
		Text_Status->SetText(FText::FromString(Msg));
		Text_Status->SetColorAndOpacity(FSlateColor(Color));
		return;
	}
	// fallback: 상태 전용 슬롯이 없으면 TitleText 재사용
	if (TitleText)
	{
		TitleText->SetText(FText::FromString(Msg));
		TitleText->SetColorAndOpacity(FSlateColor(Color));
	}
}
