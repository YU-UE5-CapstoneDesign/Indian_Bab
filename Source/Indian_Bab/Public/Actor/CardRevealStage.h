#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Game/CardRevealTypes.h"
#include "CardRevealStage.generated.h"

class UMaterialInterface;
class USceneComponent;
class USoundBase;
class UStaticMesh;
class UStaticMeshComponent;
class UTextRenderComponent;

// 각 클라이언트가 자기 자리 앞에 만드는 결과 공개 연출입니다.
UCLASS()
class INDIAN_BAB_API ACardRevealStage : public AActor
{
	GENERATED_BODY()

public:
	ACardRevealStage();

	void StartReveal(APlayerController* LocalPlayer, const TArray<FCardRevealEntry>& Entries, float Duration);

	virtual void Tick(float DeltaSeconds) override;

private:
	void StartRevealAtView(FVector ViewLocation, FRotator ViewRotation, const TArray<FCardRevealEntry>& Entries, float Duration);

	struct FCardVisual
	{
		USceneComponent* Pivot = nullptr;
		UStaticMeshComponent* Front = nullptr;
		UStaticMeshComponent* Back = nullptr;
		UStaticMeshComponent* Token = nullptr;
		UTextRenderComponent* NameText = nullptr;
		UTextRenderComponent* WinnerText = nullptr;
		UTextRenderComponent* ValueText = nullptr;
		bool bHasToken = false;
		bool bDropSoundPlayed = false;
	};

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> SceneRoot;

	// BP_CardRevealStage의 기본값에서 공개 연출에 쓰는 에셋을 교체할 수 있습니다.
	UPROPERTY(EditDefaultsOnly, Category = "Card Reveal|Assets")
	TObjectPtr<UStaticMesh> PlaneMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Card Reveal|Assets")
	TObjectPtr<UMaterialInterface> BackMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Card Reveal|Assets")
	TObjectPtr<UMaterialInterface> FrontMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Card Reveal|Assets")
	TObjectPtr<UMaterialInterface> CoinMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Card Reveal|Assets")
	TObjectPtr<USoundBase> CoinDropSound;

	UPROPERTY(EditDefaultsOnly, Category = "Card Reveal|Layout", meta = (ClampMin = "0.0"))
	float StageDistance = 105.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Card Reveal|Layout")
	float StageDrop = 60.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Card Reveal|Layout", meta = (ClampMin = "0.0"))
	float CardSpacing = 30.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Card Reveal|Layout", meta = (ClampMin = "0.001"))
	float CardScale = 0.038f;

	UPROPERTY(EditDefaultsOnly, Category = "Card Reveal|Layout")
	FVector BackScale = FVector(0.25f, 0.38f, 1.0f);

	UPROPERTY(EditDefaultsOnly, Category = "Card Reveal|Layout")
	FVector TokenOffset = FVector(6.0f, 9.0f, 3.0f);

	UPROPERTY(EditDefaultsOnly, Category = "Card Reveal|Layout", meta = (ClampMin = "0.001"))
	float TokenScale = 0.12f;

	// 일반 플레이어 이름은 표시하지 않고 승리자 이름과 비교값만 표시합니다.
	UPROPERTY(EditDefaultsOnly, Category = "Card Reveal|Text")
	FVector WinnerNameOffset = FVector(0.0f, -25.0f, 2.0f);

	UPROPERTY(EditDefaultsOnly, Category = "Card Reveal|Text", meta = (ClampMin = "0.1"))
	float WinnerNameSize = 6.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Card Reveal|Text", meta = (ClampMin = "0.1"))
	float PlayerNameSize = 4.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Card Reveal|Text")
	FColor WinnerNameColor = FColor(255, 200, 80);

	// 카드 사이의 간격 안에 들어오도록 긴 닉네임의 뒤를 ...으로 줄입니다.
	UPROPERTY(EditDefaultsOnly, Category = "Card Reveal|Text", meta = (ClampMin = "1.0"))
	float PlayerNameMaxWidth = 28.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Card Reveal|Text")
	FColor PlayerNameColor = FColor::White;

	UPROPERTY(EditDefaultsOnly, Category = "Card Reveal|Text")
	FVector WinnerLabelOffset = FVector(0.0f, -35.0f, 2.0f);

	UPROPERTY(EditDefaultsOnly, Category = "Card Reveal|Text")
	FVector ComparisonValueOffset = FVector(0.0f, 25.0f, 2.0f);

	UPROPERTY(EditDefaultsOnly, Category = "Card Reveal|Text", meta = (ClampMin = "0.1"))
	float ComparisonValueSize = 7.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Card Reveal|Text")
	FColor ComparisonValueColor = FColor(255, 232, 190);

	// 전체 결과 공개 시간은 MainGameMode의 CardRevealDurationSeconds에서 정합니다.
	UPROPERTY(EditDefaultsOnly, Category = "Card Reveal|Timing", meta = (ClampMin = "0.0"))
	float CardInterval = 0.55f;

	UPROPERTY(EditDefaultsOnly, Category = "Card Reveal|Timing", meta = (ClampMin = "0.01"))
	float CardFlipDuration = 0.55f;

	UPROPERTY(EditDefaultsOnly, Category = "Card Reveal|Timing", meta = (ClampMin = "0.0"))
	float TokenDelay = 0.60f;

	UPROPERTY(EditDefaultsOnly, Category = "Card Reveal|Timing", meta = (ClampMin = "0.01"))
	float TokenDropDuration = 0.40f;

	UPROPERTY(EditDefaultsOnly, Category = "Card Reveal|Timing", meta = (ClampMin = "0.0"))
	float WinnerDelay = 0.45f;

	UPROPERTY(EditDefaultsOnly, Category = "Card Reveal|Audio", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CoinDropVolume = 0.35f;

	TArray<FCardVisual> Visuals;
	float ElapsedTime = 0.0f;
};
