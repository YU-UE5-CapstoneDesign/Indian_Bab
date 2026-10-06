// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "CardController/CardData.h"
#include "MainPlayerState.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnTriggerCountChanged, int32);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnAliveStateChanged, bool);
DECLARE_MULTICAST_DELEGATE(FOnAddTokenStateChanged);

class ACardManager;

UCLASS()
class INDIAN_BAB_API AMainPlayerState : public APlayerState
{
	GENERATED_BODY()



public:
	AMainPlayerState();

	// 상태 복제를 위한 필수 함수
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:

public:
	DECLARE_MULTICAST_DELEGATE(FOnSteamNicknameChanged);
	FOnSteamNicknameChanged OnSteamNicknameChanged;

	DECLARE_MULTICAST_DELEGATE(FOnCardChanged);
	FOnCardChanged OnCardChanged;

	FOnTriggerCountChanged OnTriggerCountChanged;
	FOnAliveStateChanged OnAliveStateChanged;
	FOnAddTokenStateChanged OnAddTokenStateChanged;

	static constexpr int32 MaxAddTokenUses = 3;

	// 플레이어의 생존 유무
	UPROPERTY(ReplicatedUsing = "OnRep_isAlive", BlueprintReadOnly, Category = "PlayerState")
	bool isAlive;

	// 플레이어의 폴드 유무
	UPROPERTY(ReplicatedUsing = "OnRep_isFold", BlueprintReadOnly, Category = "PlayerState")
	bool isFold;

	// 서브 리볼버의 탄환 위치
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PlayerState")
	TArray<bool> BulletArray;

	// 서브 리볼버 당김 횟수
	UPROPERTY(ReplicatedUsing = "OnRep_TotalTriggerCount", BlueprintReadOnly, Category = "PlayerState")
	int32 TotalTriggerCount;

	// 닉네임 Set 함수
    void SetSteamNickname(const FString& NewNickname);
    FString GetSteamNickname() const;

	// 카드 관련 함수
	void SetMyCard(const FCardData& NewCard);
	FCardData GetMyCard() const;

	// 덧셈 토큰은 서버에서 선택하고, 최종 카드 비교에 참가할 때만 소모됩니다.
	bool SetAddTokenSelected(bool bSelected);
	bool CommitAddTokenForComparison();
	void ResetAddToken();

	UFUNCTION(BlueprintPure, Category = "PlayerState|AdditionToken")
	bool IsAddTokenSelected() const { return bAddTokenSelected; }

	UFUNCTION(BlueprintPure, Category = "PlayerState|AdditionToken")
	int32 GetRemainingAddTokenCount() const;

	int32 GetCardComparisonValue() const;

	// 처음 서브 리볼버 설정
	void SetInitSubRevolver();
	void SetAliveState(bool bNewAlive);

	// 리볼버 당김 횟수 변경
	bool ChangeSubRevolver();

protected:
	// On_Rep : 오른쪽에 적힌 함수 변화했을 때 실행하는 함수
	UFUNCTION()
	void OnRep_TotalTriggerCount();

	UFUNCTION()
	void OnRep_isAlive();

	UFUNCTION()
	void OnRep_isFold();

    UFUNCTION()
    void OnRep_SteamNickname();

	UFUNCTION()
	void OnRep_MyCard();

	UFUNCTION()
	void OnRep_AddTokenState();

	// 스팀 닉네임 변수
	UPROPERTY(ReplicatedUsing = OnRep_SteamNickname, BlueprintReadOnly, Category = "PlayerState")
    FString SteamNickname;

	UPROPERTY(ReplicatedUsing = "OnRep_MyCard", BlueprintReadOnly, Category = "PlayerState")
	FCardData MyCard;

	UPROPERTY(ReplicatedUsing = OnRep_AddTokenState, BlueprintReadOnly, Category = "PlayerState|AdditionToken")
	int32 AddTokenUsedCount;

	UPROPERTY(ReplicatedUsing = OnRep_AddTokenState, BlueprintReadOnly, Category = "PlayerState|AdditionToken")
	bool bAddTokenSelected;

};
