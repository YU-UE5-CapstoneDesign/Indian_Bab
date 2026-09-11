#pragma once

#include "CoreMinimal.h"
#include "Character/LobbyCharacter.h"
#include "TimerManager.h"
#include "LobbyVRCharacter.generated.h"

class ASeatActor;
class UMotionControllerComponent;
class UGameResultWidget;
class UReadyWidget;
class USceneComponent;
class USkeletalMeshComponent;
class UWidgetComponent;
class UWidgetInteractionComponent;
class UTurnInfoWidget;

UENUM(BlueprintType)
enum class EVRActiveUI : uint8
{
	None,
	MainMenu,
	Ready,
	InGame,
	Result
};

UCLASS()
class INDIAN_BAB_API ALobbyVRCharacter : public ALobbyCharacter
{
	GENERATED_BODY()

public:
	ALobbyVRCharacter();

	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;
	virtual void PawnClientRestart() override;
	virtual void Tick(float DeltaTime) override;

	void InitSeatedAtSeat(ASeatActor* TargetSeat);

	UFUNCTION(Client, Reliable)
	void Client_InitSeatedAtSeat(FVector TargetLocation, FRotator TargetRotation);

	UFUNCTION(Client, Reliable)
	void Client_ShowReadyWidget();

	UFUNCTION(Client, Reliable)
	void Client_HideReadyWidget();

	UFUNCTION(Client, Reliable)
	void Client_ShowMainGameWidget();

	UFUNCTION(Client, Reliable)
	void Client_HideMainGameWidget();

	UFUNCTION(Client, Reliable)
	void Client_ShowResultWidget(const FString& WinnerName, int32 WinnerPlayerId);

	void PressRightWidgetInteraction();
	void ReleaseRightWidgetInteraction();
	void PressLeftWidgetInteraction();
	void ReleaseLeftWidgetInteraction();

	void ShowReadyWidget();
	void HideReadyWidget();
	void ShowMainGameWidget();
	void HideMainGameWidget();
	void ShowResultWidget(const FString& WinnerName, int32 WinnerPlayerId);

	UFUNCTION(BlueprintCallable, Category = "VR UI")
	void SetActiveVRUI(EVRActiveUI ActiveUI);

	virtual void OnRep_IsSitting() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR")
	TObjectPtr<USceneComponent> VROrigin;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR")
	TObjectPtr<UMotionControllerComponent> MotionControllerRightGrip;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR")
	TObjectPtr<UMotionControllerComponent> MotionControllerLeftGrip;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR")
	TObjectPtr<UMotionControllerComponent> MotionControllerRightAim;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR")
	TObjectPtr<UMotionControllerComponent> MotionControllerLeftAim;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR")
	TObjectPtr<USkeletalMeshComponent> HandRight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR")
	TObjectPtr<USkeletalMeshComponent> HandLeft;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR")
	TObjectPtr<UWidgetInteractionComponent> WidgetInteractionRight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR")
	TObjectPtr<UWidgetInteractionComponent> WidgetInteractionLeft;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR UI")
	TObjectPtr<UWidgetComponent> ReadyWidgetComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VR UI")
	TSubclassOf<UReadyWidget> ReadyWidgetClass;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR UI")
	TObjectPtr<UWidgetComponent> ResultWidgetComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VR UI")
	TSubclassOf<UGameResultWidget> ResultWidgetClass;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR UI")
	TObjectPtr<UWidgetComponent> TurnInfoWidgetComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VR UI")
	TSubclassOf<UTurnInfoWidget> TurnInfoWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VR UI", meta = (ClampMin = "0.0"))
	float ReadyWidgetDelaySeconds = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VR|Seat")
	bool bUseCapsuleHalfHeightSeatOffset = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VR|Seat")
	float SeatHeightOffset = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VR|Seat|Debug")
	bool bShowSeatDebugCapsule = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VR|Seat|Debug")
	float DebugSeatCapsuleDuration = 10.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VR|Pointer")
	float VRPointerMaxDistance = 500.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VR|Pointer")
	float LaserThickness = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VR|Pointer")
	bool bShowVRPointer = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VR|Pointer|Debug")
	bool bShowWidgetInteractionDebug = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VR|Pointer")
	bool bLogVRPointerHits = false;

	UPROPERTY(ReplicatedUsing=OnRep_ArmTransforms, BlueprintReadOnly, Category = "MotionController")
	FTransform LeftArm;

	UPROPERTY(ReplicatedUsing=OnRep_ArmTransforms, BlueprintReadOnly, Category = "MotionController")
	FTransform RightArm;

	// 서버 피격 판정에 사용할 오른손 파란선의 월드 좌표입니다.
	FTransform RightAimTransform = FTransform::Identity;
	bool bHasRightAimTransform = false;

	UFUNCTION(Server, Unreliable)
	void Server_UpdateArm(const FTransform& NewLeftArm, const FTransform& NewRightArm, const FTransform& NewRightAim);

	UFUNCTION()
	void OnRep_ArmTransforms();

	// 로컬 컨트롤러 또는 서버에 전달된 오른손 좌표로 조준 방향을 구합니다.
	bool GetRightHandShotTrace(FVector& OutStart, FVector& OutEnd) const;

	void GrabGun();

protected:
	virtual void UpdateAimFromView() override;
	void UpdateArmPosition();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
private:
	UFUNCTION(Server, Reliable)
	void Server_GrabMainRevolver();

	void AttachMainRevolverToRightGrip();

	void ConfigureLocalVRTracking();
	void ApplyReplicatedArmTransforms();
	void ConfigureVRSeatedState();
	void ConfigureWidgetInteraction();
	void InitializeMainGameWidgetComponents();
	void InitializeTurnInfoWidgetComponent();
	void ShowReadyWidgetAfterDelay();
	void SetComponentsForVRUIState(EVRActiveUI UIState, bool bActive);
	void ApplyVRWidgetComponentState(UWidgetComponent* WidgetComponent, bool bActive);
	bool DoesWidgetComponentMatchName(const UWidgetComponent* WidgetComponent, FName ComponentName);
	void UpdateVRPointers();
	void UpdateLaserPointer(const UMotionControllerComponent* AimController, const TCHAR* PointerName) const;
	void DrawSeatDebugCapsule() const;

	FTimerHandle ReadyWidgetDelayTimerHandle;

};
