#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Game/MainGameTypes.h"
#include "LobbyCharacter.generated.h"


class UInputAction;
class AMainGamePlayerController;
struct FInputActionValue;
class UCameraComponent;
class UGroomComponent;
class ASeatActor;
class ARevolver;
class UWidgetComponent;


UCLASS()
class INDIAN_BAB_API ALobbyCharacter : public ACharacter
{
	GENERATED_BODY()

private:
	// 폴드와 PC 승리 시 기존 총 잡기 몽타주 처리를 공유합니다.
	void PlayGrabGunMontage(EGunHoldReason Reason);

	// 총 반환과 PC 격발 종료에서 손의 총 메시 정리를 공유합니다.
	void ClearHeldRevolverMeshes();

	// 서버와 소유 클라이언트의 착석 완료 상태를 공통으로 적용합니다.
	void CompleteSeatedState();

	// 초기 시선과 좌석 방향에 맞춰 PC 카메라를 설정합니다.
	void ApplySeatedCamera(const FRotator& InitialViewRotation, const FRotator& SeatRotation);

public:
	// Sets default values for this character's properties
	ALobbyCharacter();

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	// 서버가 모든 클라이언트에게 애니메이션을 재생하라고 지시하는 함수
	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlaySitAnimation();

	// 앉기 상태 변경 함수 (서버에서 호출)
	void SetSittingState(bool bSitting);

	// 의자 상호작용 시 호출되어 몽타주 종료를 기다립니다.
	void StartSitTransition(ASeatActor* TargetSeat);

	void InitPCSeatedAtSeat(ASeatActor* TargetSeat);

	UFUNCTION(Client, Reliable)
	void Client_InitPCSeated(FVector Location, FRotator Rotation);

	UPROPERTY(EditDefaultsOnly, Category = "PC|Seat")
	float PCSeatHeightOffset = 0.0f;

	// 서버/클라이언트 모두에서 앉기 상태가 변할 때 시각적, 조작적 처리를 할 함수
	UFUNCTION()
	virtual void OnRep_IsSitting();

	// 서버가 몽타주 종료 후 클라이언트에게 최종 카메라 세팅을 지시하는 함수
	UFUNCTION(Client, Reliable)
	void Client_LockCameraAfterSit(FRotator FinalSitRotation);

	// ★ [새로 추가] 서버가 클라이언트에게 앉을 준비(시선 강제 회전)를 하라고 지시하는 함수
	UFUNCTION(Client, Unreliable)
	void Client_PrepareSit(FVector TargetLocation, FRotator TargetRotation);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* FirstPersonMetaHumanBody;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* FirstPersonMetaHumanTorso;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* ThirdPersonMetaHumanBody;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* ThirdPersonMetaHumanTorso;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* ThirdPersonMetaHumanFace;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UGroomComponent* ThirdPersonMetaHumanHair;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UGroomComponent* ThirdPersonMetaHumanEyebrows;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UGroomComponent* ThirdPersonMetaHumanBeard;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UGroomComponent* ThirdPersonMetaHumanMustache;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UGroomComponent* ThirdPersonMetaHumanEyelashes;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UGroomComponent* ThirdPersonMetaHumanFuzz;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* CameraComponent;

	// 의자에 앉을 때 재생할 애니메이션 몽타주 (BP에서 할당)
	UPROPERTY(EditDefaultsOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> SitMontage;

	// Fold 시 자기 머리를 겨냥하는 애니메이션 몽타주 (BP에서 할당)
	UPROPERTY(EditDefaultsOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> AimMyselfMontage;

	// Fold 격발 끝나고 돌려두는 애니메이션 몽타주 (BP에서 할당)
	UPROPERTY(EditDefaultsOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> EndAimMyselfMontage;

	// 승리 시 총을 겨냥하는 애니메이션 몽타주 (BP에서 할당)
	UPROPERTY(EditDefaultsOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> WinAimMontage;

	// 승리 후 총을 격발 후 되돌리는 애니메이션 몽타주 (BP에서 할당)
	UPROPERTY(EditDefaultsOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> WinEndMontage;

	// 총을 집어든 이유 - ABP 스테이트 머신 트랜지션 판별용 (Replicated)
	UPROPERTY(ReplicatedUsing = OnRep_GunHoldReason, BlueprintReadOnly, Category = "State")
	EGunHoldReason GunHoldReason;

	// ABP로 넘겨줄 최종 좌우 목 꺾임 각도 (모든 사람에게 동기화됨)
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Camera")
	float ReplicatedAimYaw = 0.0f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Camera")
	float ReplicatedAimPitch = 0.0f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Camera")
	FRotator ReplicatedAim;

	// 서버에서 스팀 닉네임 및 카드 바인딩
	virtual void PossessedBy(AController* NewController) override;

	// 스팀 닉네임 및 카드 바인딩 함수
	void BindPlayerStateDelegates();

	UFUNCTION()
	void UpdateNameWidget();

	UFUNCTION()
	void UpdateCardWidget();

	UFUNCTION()
	void UpdateCardMesh();

	UFUNCTION(Server, Unreliable)
	void Server_UpdateAim(FRotator NewAim);

	// 플레이어 이름 색상 업데이트 함수
	UFUNCTION()
	void UpdatePlayerNameColor();

	// 플레이어 상태 변화 시 호출
	UFUNCTION()
	void OnAliveStateChanged(bool bIsAlive);

	UFUNCTION()
	void OnRep_GunHoldReason();

	// 총 집어들기 몽타주 재생 (Fold/Win 공통)
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayGrabGunMontage(EGunHoldReason Reason);

	// 총 원래 위치로 보내는 몽타주 재생 (Fold/Win 공통)
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PutBackGunMontage(EGunHoldReason Reason);

	// 이 캐릭터 자리에 놓인 리볼버 (SeatActor 착석 시 할당, Replicated)
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<ARevolver> DeskRevolver;
	
	// 현재 애니메이션에서 손에 붙일 리볼버
	// Fold일 때는 자리 앞 서브 리볼버, Win일 때는 맵 중앙 메인 리볼버
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<ARevolver> ActiveRevolver;

	// 1인칭 리볼버 메시 (FirstPersonMetaHumanBody의 Revolver 소켓에 부착, 본인만 보임)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<USkeletalMeshComponent> FP_RevolverMesh;


	// 3인칭 리볼버 메시 (ThirdPersonMetaHumanBody의 Revolver 소켓에 부착, 타인만 보임)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<USkeletalMeshComponent> TP_RevolverMesh;

	// AnimNotify_GrabRevolver 에서 호출 - 책상 리볼버를 숨기고 FP/TP 메시를 소켓에 부착
	void AttachRevolverToSocket();

	// AnimNotify_ReturnRevolverToDesk에서 호출 - 손 리볼버 메시를 숨기고 책상 리볼버를 다시 보이게 함
	void ReturnRevolverToDesk();

	// ★ 추가: 현재 캐릭터가 앉아있는지 여부 (동기화 됨)
	UPROPERTY(ReplicatedUsing = OnRep_IsSitting, BlueprintReadOnly, Category = "State")
	bool bIsSitting;

	// 앉기 몽타주가 끝났는지 여부 (서버에서만 관리)
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "State")
	bool bIsSittingEnded;

	// 상호작용 거리
	UPROPERTY(EditDefaultsOnly, Category = "Interaction")
	float InteractRange = 250.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PlayerNameWidget")
	TObjectPtr<UWidgetComponent> NameWidgetComponent;

	UPROPERTY(BlueprintReadOnly, Category = "State")
	bool bIsPuttingBackGun = false;

	void SetActiveRevolver(ARevolver* NewRevolver);

	// PC는 기존 몽타주로 잡고, 서버 완료 시 모든 화면의 총 부착을 확정합니다.
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_BeginPCMainRevolver(ARevolver* Revolver);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_CompletePCMainRevolverGrab();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_ClearPCMainRevolver();

	void BeginManualMainRevolverPhase();
	void ReturnMainRevolverToTableImmediately();
	void MarkMainRevolverGrabbed();
	bool IsMainRevolverGrabbed() const;

	// 메인 리볼버 조준선 표시 여부
	UPROPERTY(BlueprintReadOnly, Category = "Main Revolver")
	bool bShowMainShotAimLine = false;

	UPROPERTY(BlueprintReadOnly, Category = "Main Revolver")
	bool bMainRevolverGrabbed = false;

	// 조준선 거리
	UPROPERTY(EditDefaultsOnly, Category = "Main Revolver")
	float MainShotAimLineDistance = 5000.0f;

	// PC 화면 중앙에 표시할 디버그 점의 크기입니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Main Revolver", meta = (ClampMin = "1.0"))
	float PCMainShotDotSize = 6.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Card")
	TObjectPtr<UStaticMeshComponent> CardDisplayMesh;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	virtual void OnRep_PlayerState() override;

	virtual void UpdateAimFromView();

	// 상호작용 입력 처리 함수
	void OnInteract(const FInputActionValue& Value);

	// 서버에 상호작용을 요청하는 RPC
	UFUNCTION(Server, Reliable)
	void ServerInteract(AActor* InteractableActor);

	// 리플리케이션(동기화) 설정
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 몽타주가 끝났을 때 서버에서 호출될 콜백 함수
	UFUNCTION()
	void OnSitMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	// 총 쏘는 몽타주가 끝났을 때 서버에서 호출될 콜백 함수
	UFUNCTION()
	void OnGrabGunMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	UFUNCTION()
	void OnPutBackGunMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	void DrawMainShotAimLine();

private:
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Interact;

	// 현재 상호작용 중인 의자 캐싱
	UPROPERTY()
	TObjectPtr<ASeatActor> CurrentSeat;

	EGunHoldReason FinishedReason;

	// 조준선 표시/숨김
	void SetMainShotAimLineVisible(bool bVisible);

};
