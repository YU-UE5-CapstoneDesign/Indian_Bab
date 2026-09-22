#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Sound/SoundBase.h"
#include "Revolver.generated.h"


class USkeletalMeshComponent;
class USphereComponent;
class UWidgetComponent;
class URevolverCountWidget;

UCLASS()
class INDIAN_BAB_API ARevolver : public AActor
{
	GENERATED_BODY()

public:
	// 생성자: 기본값들을 초기화하는 곳입니다.
	ARevolver();

	// 각 클라이언트의 카메라를 향하도록 월드 위젯 위치와 회전을 갱신합니다.
	virtual void Tick(float DeltaTime) override;

	// --- 컴포넌트 ---

	// 캡슐 콜리전 컴포넌트 (충돌 감지용)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USphereComponent* CollisionSphere;

	// 무기의 시각적 모델 (스켈레탈 메시)
	// 블루프린트에서 리볼버 메시를 여기에 할당
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USkeletalMeshComponent* WeaponMesh;

	// 탄창 수 표시용 위젯 컴포넌트 (월드 스페이스)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UWidgetComponent* BulletCountWidgetComponent;

	// 리볼버 원점에서 월드 Z축 방향으로 띄울 위젯 높이입니다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Widget", meta = (ClampMin = "0.0"))
	float CountWidgetHeight = 5.0f;

	// 위젯 에셋의 앞면 방향이 반대라면 에디터에서 180으로 조절합니다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Widget")
	float CountWidgetFacingYawOffset = 0.0f;


	// --- 무기 스탯 변수 ---

	// 최대 장탄수 (리볼버니까 기본 6발이 적당하겠죠?)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Stats")
	int32 MaxAmmo;

	// 현재 장탄수
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon Stats")
	int32 CurrentAmmo;

	// 무기 데미지
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Stats")
	float Damage;


	// --- 무기 기능(함수) ---

	// 격발 함수 (블루프린트에서도 부를 수 있도록 BlueprintCallable을 붙입니다)
	UFUNCTION(BlueprintCallable, Category = "Weapon Actions")
	void Fire();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayFireSound();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayDryFireSound();

	// 장전 함수
	UFUNCTION(BlueprintCallable, Category = "Weapon Actions")
	void Reload();

	// 탄창 수 위젯 업데이트 함수
	UFUNCTION(BlueprintCallable, Category = "Weapon Actions")
	void UpdateBulletCountWidget(int32 CurrentCount, int32 MaxCount = 8);

	// 폴드 카운트 위젯을 업데이트하는 함수
	void UpdateFoldCountWidget(int32 Count);

	// 게임 페이즈에 따라 위젯 표시 여부를 제어하는 함수
	UFUNCTION(BlueprintCallable, Category = "Weapon Actions")
	void SetWidgetPlayingPhase(bool bIsPlaying);

	void ReturnToInitialTableTransform();
	// 총소리 함수
	UPROPERTY(EditDefaultsOnly, Category = "Sound")
	TObjectPtr<USoundBase> FireSound;

	UPROPERTY(EditDefaultsOnly, Category = "Sound")
	TObjectPtr<USoundBase> DryFireSound;

protected:
	// 게임이 시작되거나 이 무기가 스폰될 때 호출됩니다.
	virtual void BeginPlay() override;

private:
	void PlayFireSound() const;
	void PlayDryFireSound() const;
	void UpdateCountWidgetTransform();

	FTransform InitialTableTransform;
	bool bInitialTableTransformSaved = false;

};
