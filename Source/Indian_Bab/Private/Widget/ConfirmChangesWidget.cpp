#include "Widget/ConfirmChangesWidget.h"
#include "Components/Button.h"
#include "Widget/OptionMenuWidget.h"


void UConfirmChangesWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 2. C++ 함수와 BP 버튼의 OnClicked 이벤트를 바인딩
	if (Button_Yes)
	{
		Button_Yes->OnClicked.AddDynamic(this, &UConfirmChangesWidget::OnYesClicked);
	}
	if (Button_No)
	{
		Button_No->OnClicked.AddUniqueDynamic(this, &UConfirmChangesWidget::OnNoClicked);
	}
}


void UConfirmChangesWidget::SetParentMenu(UUserWidget* InParentMenu)
{
	OptionMenuOwner = Cast<UOptionMenuWidget>(InParentMenu);
}


void UConfirmChangesWidget::OnYesClicked()
{
	if (OptionMenuOwner)
	{
		// '예'를 눌렀다고 Owner(UOptionMenuWidget)에게 알림
		OptionMenuOwner->OnConfirmChangesYes();
	}

	// 팝업 위젯을 닫습니다.
	RemoveFromParent();
}


void UConfirmChangesWidget::OnNoClicked()
{
	if (OptionMenuOwner)
	{
		// '아니오'를 눌렀다고 Owner(UOptionMenuWidget)에게 알림
		OptionMenuOwner->OnConfirmChangesNo();
	}

	// 팝업 위젯을 닫음
	RemoveFromParent();
}
