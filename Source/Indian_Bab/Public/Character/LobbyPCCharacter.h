#pragma once
#include "CoreMinimal.h"
#include "Character/LobbyCharacter.h"
#include "LobbyPCCharacter.generated.h"

class UPCCrosshairWidget;

UCLASS()
class INDIAN_BAB_API ALobbyPCCharacter : public ALobbyCharacter
{
    GENERATED_BODY()
public:
    ALobbyPCCharacter();
    void Move(const FVector2D& Axis);
    void Look(const FVector2D& Axis, float Sensitivity);
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    virtual void OnRep_IsSitting() override;
    virtual void InitSeatedAtSeat(ASeatActor* TargetSeat) override;
    virtual bool GetMainShotTrace(float TraceDistance, FVector& OutStart, FVector& OutEnd) const override;
    virtual void ReturnMainRevolverToTableImmediately() override;

    // PC 즉시 착석과 소유 클라이언트 위치 동기화
    void InitPCSeatedAtSeat(ASeatActor* TargetSeat);
    UFUNCTION(Client, Reliable)
    void Client_InitPCSeated(FVector Location, FRotator Rotation);

    UPROPERTY(EditDefaultsOnly, Category = "PC|Seat")
    float PCSeatHeightOffset = 0.0f;

    // PC 몽타주와 손의 총 표시를 모든 클라이언트에 적용합니다.
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_BeginPCMainRevolver(ARevolver* Revolver);
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_CompletePCMainRevolverGrab();
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_ClearPCMainRevolver();

    // 독립 PC 조준점의 표시 여부를 확인합니다.
    UFUNCTION(BlueprintPure, Category = "PC|Aim")
    bool ShouldShowMainShotCrosshair() const;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void UpdateAimFromView() override;
    virtual void OnSeatedCameraReady(const FRotator& SeatRotation) override;
    void ApplySeatedCamera(const FRotator& InitialViewRotation, const FRotator& SeatRotation);
    virtual void DrawMainShotAimLine() override;

private:
    UPROPERTY(EditDefaultsOnly, Category = "PC|Aim")
    TSubclassOf<UPCCrosshairWidget> PCCrosshairWidgetClass;

    UPROPERTY(Transient)
    TObjectPtr<UPCCrosshairWidget> PCCrosshairWidget;

    UPROPERTY(Transient)
    TSubclassOf<UAnimInstance> DefaultFirstPersonAnimClass;

    bool bUsingSharedAimPose = false;
    bool bSavedFirstPersonFOV = false;
    bool bSavedFirstPersonScale = false;
    void UpdateMainAimPresentation();

    // 기존 PC BP의 상호작용 액션을 재사용합니다.
    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputAction> IA_Interact;
    void OnInteract(const FInputActionValue& Value);
};
