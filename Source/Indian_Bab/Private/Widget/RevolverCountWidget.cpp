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
	// Playing �������� ���� ���� ��ü�� ���̰� ��
	// �տ� ���� �޷����� �� �ؽ�Ʈ�� ���̸� ����ϹǷ� Playing������ ǥ��
	SetVisibility(bIsPlaying ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
}