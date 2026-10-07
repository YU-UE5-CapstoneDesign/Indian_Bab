#include "Widget/RoomCreateWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/CheckBox.h"
#include "Kismet/GameplayStatics.h"
#include "GameInstanceSubsystem/SessionSubsystem.h"
#include "Net/UnrealNetwork.h"
#include "Widget/MainMenuWidget.h"
#include "PlayerController/MainMenuPlayerController.h"


void URoomCreateWidget::SetParentMenu(UUserWidget* InParentMenu)
{
	ParentMenu = InParentMenu;
}


void URoomCreateWidget::OnYesClicked()
{
	if (SessionSubsystem)
	{
		// 1. 버튼 비활성화 (중복 클릭 방지)
		Button_Yes->SetIsEnabled(false);
		Button_No->SetIsEnabled(false);

		// 2. [상태 업데이트] "방 생성 중..." 표시
		if (Text_Status)
		{
			Text_Status->SetText(FText::FromString(TEXT("방을 생성하고 있습니다... 잠시만 기다려주세요.")));
			Text_Status->SetColorAndOpacity(FLinearColor::White); // 흰색
		}

		// 3. 서브시스템 호출 — 체크박스로 가시성 결정 (기본 unchecked = Public)
		const ERoomVisibility RoomVisibility =
			(CheckBox_FriendsOnly && CheckBox_FriendsOnly->IsChecked())
			? ERoomVisibility::FriendsOnly
			: ERoomVisibility::Public;
		SessionSubsystem->CreateRoom(4, RoomVisibility);
	}
}


void URoomCreateWidget::OnNoClicked()
{
	if (UMainMenuWidget* MainMenu = Cast<UMainMenuWidget>(ParentMenu))
	{
		if (!IsInViewport())
		{
			MainMenu->ShowMainMenuRoot();
			return;
		}
	}

	// 부모 메뉴가 있다면 다시 보이게 함
	if (ParentMenu)
	{
		// 부모 위젯(PauseMenu)으로 돌아가기 위해 GameAndUI 모드로 '복원' 및 포커싱 재설정
		if (PlayerControllerRef)
		{
			FInputModeGameAndUI InputModeData;
			InputModeData.SetWidgetToFocus(ParentMenu->TakeWidget());
			InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::LockInFullscreen);
			PlayerControllerRef->SetInputMode(InputModeData);
		}
	}
	RemoveFromParent();
}


void URoomCreateWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 플레이어 컨트롤러 캐시
	PlayerControllerRef = GetOwningPlayer();

	// --- 서브시스템 가져오기 및 이벤트 바인딩 ---
	if (GetGameInstance())
	{
		SessionSubsystem = GetGameInstance()->GetSubsystem<USessionSubsystem>();
		if (SessionSubsystem)
		{
			// 이미 바인딩 되어 있는지 확인 후 추가 (혹은 Remove 후 Add)
			SessionSubsystem->OnSessionErrorEvent.RemoveDynamic(this, &URoomCreateWidget::OnSessionError);
			SessionSubsystem->OnSessionErrorEvent.AddDynamic(this, &URoomCreateWidget::OnSessionError);
		}
	}

	// --- 버튼 이벤트 바인딩 --- (정리는 NativeDestruct에서 일괄)
	if (Button_Yes)
	{
		Button_Yes->OnClicked.AddDynamic(this, &URoomCreateWidget::OnYesClicked);
	}
	if (Button_No)
	{
		Button_No->OnClicked.AddDynamic(this, &URoomCreateWidget::OnNoClicked);
	}

	// [초기화] 시작 시 상태 텍스트 비우기
	if (Text_Status)
	{
		Text_Status->SetText(FText::GetEmpty());
	}

	// 캐시 인스턴스 재오픈 대비 — 이전 세션에서 OnYesClicked로 disable된 채 박제되는 것 방지.
	// 닫힘 트리거(OnNoClicked/OnSessionError)마다 흩지 말고 재진입 시점에서 idempotent하게 리셋.
	if (Button_Yes) Button_Yes->SetIsEnabled(true);
	if (Button_No) Button_No->SetIsEnabled(true);
}


void URoomCreateWidget::NativeDestruct()
{
	// [중요] BindWidget UButton은 UUserWidget(=this)이 살아있는 한 GC 대상이 아니므로
	// OnClicked 바인딩이 누적된다. 캐시 인스턴스 재오픈 시 NativeConstruct 재진입으로
	// 중복 AddDynamic ensure가 발생하므로 여기서 일괄 해제.
	if (Button_Yes) Button_Yes->OnClicked.RemoveAll(this);
	if (Button_No)  Button_No->OnClicked.RemoveAll(this);

	// 외부 객체 구독 해제 (dangling 방지)
	if (SessionSubsystem)
	{
		SessionSubsystem->OnSessionErrorEvent.RemoveDynamic(this, &URoomCreateWidget::OnSessionError);
	}

	Super::NativeDestruct();
}


/**
 * ESC 키 입력을 처리하는 함수
 */
FReply URoomCreateWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// ESC 키가 눌렸는지 확인
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		//UE_LOG(LogTemp, Log, TEXT("RoomJoinWidget: ESC key pressed."));

		// 그냥 닫습니다
		OnNoClicked();

		// [중요] ESC 키 입력을 '처리했음(Handled)'으로 반환
		// 이렇게 해야 입력이 PlayerController로 전파되어
		// PauseMenu가 닫히는 것을 막을 수 있음
		return FReply::Handled();
	}

	// 다른 키가 눌렸다면 '처리 안 함(Unhandled)'으로 반환
	return FReply::Unhandled();
}


/**
 * '배경' 클릭 시 포커스를 잃지 않도록 처리하는 함수
 */
FReply URoomCreateWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// 기본 마우스 로직을 먼저 호출 (예: 버튼 클릭)
	FReply Reply = Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);

	// VR 모드에서 SetInputMode(UIOnly)는 WidgetInteractionComponent 시뮬 키 라우팅을 깨뜨림 — Mouse 모드만 포커스 회복 적용
	const AMainMenuPlayerController* MainMenuPC = Cast<AMainMenuPlayerController>(PlayerControllerRef);
	if (MainMenuPC && !MainMenuPC->IsMouseMenuInputMode())
	{
		return FReply::Handled();
	}

	// 팝업이 없고, 플레이어 컨트롤러가 유효하다면
	// (사용자가 이 위젯의 빈 공간(배경)을 클릭했더라도)
	// 이 위젯에게 다시 키보드 포커스를 강제로 설정
	if (PlayerControllerRef)
	{
		//// SWidget에 대한 포인터를 가져옵니다.
		//TSharedPtr<SWidget> SafeWidget = this->TakeWidget();
		//if (SafeWidget.IsValid())
		//{
		//	// "이 위젯에 키보드 포커스를 설정하라"
		//	FSlateApplication::Get().SetKeyboardFocus(SafeWidget);
		//}
		FInputModeUIOnly InputModeData;
		InputModeData.SetWidgetToFocus(this->TakeWidget()); // 포커스를 이 위젯으로
		PlayerControllerRef->SetInputMode(InputModeData);
	}

	// FReply::Handled()를 반환
	// 이 클릭을 "처리했음(Handled)"으로 표시하여,
	// 클릭 이벤트가 뷰포트로 "떨어지는(fall-through)" 것을 막음
	return FReply::Handled();
}


// 세션 에러 수신 — 자기 자신만 닫고 끝.
// 사유 표시는 MainMenuPC의 SessionErrorWidget 모달이 단일 채널로 담당 (이중 표시 방지).
// OnNoClicked과 동일하게 부모 메뉴로 입력 모드 복원 + RemoveFromParent.
void URoomCreateWidget::OnSessionError(const FString& Reason)
{
	UE_LOG(LogTemp, Warning, TEXT("RoomCreateWidget: session error received, auto-closing. reason=%s"), *Reason);
	OnNoClicked();
}
