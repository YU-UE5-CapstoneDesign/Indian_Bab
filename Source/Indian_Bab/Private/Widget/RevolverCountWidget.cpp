#include "Widget/RevolverCountWidget.h"
#include "Components/TextBlock.h"

void URevolverCountWidget::UpdateCount(int32 CurrentCount, int32 MaxCount)
{
	if (!Text_BulletCount) return;

	FString CountText = FString::Printf(TEXT("%d/%d"), CurrentCount, MaxCount);
	Text_BulletCount->SetText(FText::FromString(CountText));
}

void URevolverCountWidget::UpdateFoldCount(int32 Count)
{
	if (!Text_BulletCount) return;

	Text_BulletCount->SetText(FText::AsNumber(Count));
}

void URevolverCountWidget::SetPlayingPhase(bool bIsPlaying)
{
	// Playing 페이즈일 때만 위젯 전체를 보이게 함
	// 손에 총이 달려있을 때 텍스트가 보이면 어색하므로 Playing에서만 표시
	SetVisibility(bIsPlaying ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
}