#include "Game/MainGameMode.h"
#include "Game/MainGameState.h"
#include "Character/LobbyCharacter.h"
#include "Character/LobbyVRCharacter.h"
#include "PlayerController/MainGamePlayerController.h"
#include "PlayerState/MainPlayerState.h"
#include "Actor/Revolver.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"

#if WITH_SERVER_CODE

namespace
{
	constexpr int32 BettingActionMaxRaiseCount = 7;
}

void AMainGameMode::HandleBetAction(AMainGamePlayerController* RequestPC, EBetAction Action, int32 RaiseCount)
{
    if (!HasAuthority()) return;
    if (!RequestPC) return;

	AMainGameState* GS = GetGameState<AMainGameState>();
	if (!GS) return;

	// 중복 입력 방지
	if (GS->bTurnActionInProgress) return;

    int32 PlayerId = RequestPC->GetPlayerIdSafe();
	if (GS -> CurrentTurnPlayerId != PlayerId) return;

	// Raise 불가능하면 아예 막고 종료
    if (Action == EBetAction::Raise && (RaiseCount < 1 || RaiseCount > BettingActionMaxRaiseCount || GS->CurrentBulletCount + RaiseCount > GS->MainRevolverChamberCount))
    {
		UE_LOG(LogTemp, Warning, TEXT("[GM] Raise blocked: RaiseCount=%d CurrentBulletCount=%d"), RaiseCount, GS->CurrentBulletCount);
        //TODO 추후에 텍스트로 Raise 불가라고 뜨게
        return;
    }

	GS->bTurnActionInProgress = true;
	GetWorldTimerManager().ClearTimer(TimerHandle);
	GS->ClearTimerInfo();

	UE_LOG(LogTemp, Warning, TEXT("[GM] Player %d Action: %s"), PlayerId, *UEnum::GetValueAsString(Action));

	if(Action == EBetAction::Fold)
	{
		GS->ChangeCurrentBetInfo(Action);
		HandleFoldAction(RequestPC);
		return;
	}
	if(Action == EBetAction::Raise)
	{
		CheckPlayer = PlayerId;
		GS->ChangeCurrentBetInfo(Action, RaiseCount);
		CheckNext();
		return;
	}
	if(Action == EBetAction::CheckCall)
	{
		GS->ChangeCurrentBetInfo(Action);
		CheckNext();
		return;
	}

	// 예외 상황 복구
	GS->bTurnActionInProgress = false;

}

// 폴드 베팅 액션
void AMainGameMode::HandleFoldAction(AMainGamePlayerController* RequestPC)
{
    if (!HasAuthority()) return;
    if (!RequestPC) return;

	AMainPlayerState* PS = RequestPC->GetPlayerState<AMainPlayerState>();
	if (!PS) return;
	
	if (CheckPlayer == PS->GetPlayerId())
	{
		bCheckPlayerFolded = true;
	}


	PS->isFold = true;

	ALobbyCharacter* Character = Cast<ALobbyCharacter>(RequestPC->GetPawn());
	if (Character)
	{
		Character->SetActiveRevolver(Character->DeskRevolver);
		Character->Multicast_PlayGrabGunMontage(EGunHoldReason::Fold);
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[GM] Player %d has no fold character. Resolving fold immediately."), PS->GetPlayerId());
	bool PlayerAlive = PS -> ChangeSubRevolver();
	if(PlayerAlive)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GM] Player %d survived by sub revolver"), PS ->GetPlayerId());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[GM] Player %d died by sub revolver"), PS ->GetPlayerId());
		AMainGameState* GS = GetGameState<AMainGameState>();
		if(GS)
		{
			--GS -> AlivePlayerCount;
		}
	}

	CheckNext();
}

// 메인 리볼버 격발 시간 타이머
void AMainGameMode::StartMainshotTimer(float Time)
{
	if (!HasAuthority()) return;

	AMainGameState* GS = GetGameState<AMainGameState>();
	if (!GS) return;
	GS->SetTimerInfo(Time);

	GetWorldTimerManager().ClearTimer(TimerHandle);
	GetWorldTimerManager().SetTimer(TimerHandle, this, &AMainGameMode::OnMainShotTimerExpired, Time, false);

}

// 메인 리볼버 격발 시간 넘겼을 때
void AMainGameMode::OnMainShotTimerExpired()
{
	if (!HasAuthority()) return;

	AMainGameState* GS = GetGameState<AMainGameState>();
	if (GS)
		GS->ClearTimerInfo();

	ALobbyCharacter* WinnerCharacter = nullptr;
	if (CurrentWinnerPS)
	{
		if (AMainGamePlayerController* PC = Cast<AMainGamePlayerController>(CurrentWinnerPS->GetOwner()))
		{
			WinnerCharacter = Cast<ALobbyCharacter>(PC->GetPawn());
		}
	}

	if (!WinnerCharacter || !WinnerCharacter->IsMainRevolverGrabbed())
	{
		UE_LOG(LogTemp, Warning, TEXT("[GM] Main shot timer expired but MainRevolver is not grabbed. Auto fire skipped."));
		// PC와 VR 모두 기존 시간 초과 정리 경로로 격발 단계를 종료합니다.
		OnMainRevolverGrabTimerExpired();
		return;
	}

	ExecuteMainShot(true);
}

void AMainGameMode::OnMainRevolverGrabTimerExpired()
{
	if (!HasAuthority()) return;

	AMainGameState* GS = GetGameState<AMainGameState>();
	if (GS)
	{
		GS->ClearTimerInfo();
	}

	ALobbyCharacter* WinnerCharacter = nullptr;
	if (CurrentWinnerPS)
	{
		if (AMainGamePlayerController* PC = Cast<AMainGamePlayerController>(CurrentWinnerPS->GetOwner()))
		{
			WinnerCharacter = Cast<ALobbyCharacter>(PC->GetPawn());
		}
	}

	if (WinnerCharacter && WinnerCharacter->IsMainRevolverGrabbed())
	{
		UE_LOG(LogTemp, Warning, TEXT("[GM] MainRevolver was grabbed before grab timer expired. Ignored."));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[GM] MainRevolver grab timed out. Main shot phase skipped."));

	if (WinnerCharacter)
	{
		WinnerCharacter->ReturnMainRevolverToTableImmediately();
	}
	else if (ARevolver* Revolver = GetMainRevolver())
	{
		// 승자가 퇴장해 캐릭터가 없어도 공용 총을 책상으로 반환합니다.
		Revolver->ReturnToInitialTableTransform();
	}

	FinishMainShotPhase();
}

void AMainGameMode::HandleMainRevolverGrabbed(ALobbyCharacter* Character)
{
	if (!HasAuthority() || !Character) return;

	AMainGameState* GS = GetGameState<AMainGameState>();
	if (!GS || GS->CurrentGamePhase != EGamePhase::Result) return;
	if (!CurrentWinnerPS) return;

	AMainGamePlayerController* WinnerPC = Cast<AMainGamePlayerController>(CurrentWinnerPS->GetOwner());
	if (!WinnerPC || WinnerPC->GetPawn() != Character) return;

	if (GS->CurrentBulletCount <= 0)
	{
		StartMainRevolverPutBack();
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[GM] MainRevolver grabbed. Shot timer started."));
	StartMainshotTimer(10.0f);
}

// 메인 리볼버 격발 액션
// 승자의 잡기 완료 상태와 격발 조건을 서버에서 확인하고 발사합니다.
void AMainGameMode::HandleMainRevolverShotAction(AMainGamePlayerController* RequestPC)
{
	if (!HasAuthority()) return;
	if (!RequestPC) return;

	AMainGameState* GS = GetGameState<AMainGameState>();
	if (!GS) return;

	AMainPlayerState* RequestPS = RequestPC->GetPlayerState<AMainPlayerState>();
	if (!RequestPS) return;

	// result 페이즈가 아니면 리턴
	if (GS->CurrentGamePhase != EGamePhase::Result) return;

	// 승리 플레이어나 요청한 플레이어가 승리 플레이어와 동일하지 않으면 리턴
	if (!CurrentWinnerPS || RequestPS != CurrentWinnerPS) return;

	// PC와 VR 모두 실제 잡기 완료 이후에만 서버에서 격발을 허용합니다.
	const ALobbyCharacter* Shooter = Cast<ALobbyCharacter>(RequestPC->GetPawn());
	if (!RequestPS->isAlive || !Shooter || !Shooter->IsMainRevolverGrabbed()
		|| Shooter->GunHoldReason != EGunHoldReason::Win || !Shooter->ActiveRevolver
		|| bMainRevolverPutBackInProgress) return;

	if (GS -> CurrentBulletCount <= 0)
	{
		StartMainRevolverPutBack();
		return;
	}

	ExecuteMainShot(false);
}

// 폴드 애니메이션 끝났을 때 호출
void AMainGameMode::HandleFoldMontageFinished(ALobbyCharacter* Character)
{
    if (!HasAuthority()) return;
    if (!Character) return;

    AMainGameState* GS = GetGameState<AMainGameState>();
    if (!GS) return;

	AMainPlayerState* PS = Character->GetPlayerState<AMainPlayerState>();
	if (PS)
	{
		const bool PlayerAlive = PS->ChangeSubRevolver();

		if (ARevolver* SubRevolver = Character->DeskRevolver)
		{
			if (PlayerAlive)
			{
				SubRevolver->Multicast_PlayDryFireSound();
			}
			else
			{
				SubRevolver->Multicast_PlayFireSound();
			}
		}

		if (PlayerAlive)
		{
			UE_LOG(LogTemp, Warning, TEXT("[GM] Player %d survived by sub revolver after fold montage"), PS->GetPlayerId());
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[GM] Player %d died by sub revolver after fold montage"), PS->GetPlayerId());
			if (GS->AlivePlayerCount > 0)
			{
				--GS->AlivePlayerCount;
			}

			CheckNext();
			return;
		}
	}
	
    Character->Multicast_PutBackGunMontage(EGunHoldReason::Fold);
}

// 메인 리볼버 줍는 애니메이션 끝났을 때 호출
// 메인 총 잡기 몽타주 종료 후 PC 잡기를 완료하거나 VR 격발 단계를 진행합니다.
void AMainGameMode::HandleMainMontageFinished(ALobbyCharacter* Character)
{
	if (!HasAuthority()) return;
	if (!Character) return;

	AMainGameState* GS = GetGameState<AMainGameState>();
	if (!GS || GS->CurrentGamePhase != EGamePhase::Result || !CurrentWinnerPS) return;
	AMainGamePlayerController* WinnerPC = Cast<AMainGamePlayerController>(CurrentWinnerPS->GetOwner());
	if (!WinnerPC || WinnerPC->GetPawn() != Character || bMainRevolverPutBackInProgress) return;

	if (!Cast<ALobbyVRCharacter>(Character))
	{
		if (Character->GunHoldReason != EGunHoldReason::Win || !Character->ActiveRevolver
			|| Character->IsMainRevolverGrabbed()) return;
		Character->Multicast_CompletePCMainRevolverGrab();
		Character->MarkMainRevolverGrabbed();
		HandleMainRevolverGrabbed(Character);
		return;
	}
	ManageShotPhase();
}

// 리볼버 격발 끝난 후(폴드나 게임 후)
void AMainGameMode::HandlePutBackGunMontageFinished(ALobbyCharacter* Character, EGunHoldReason Reason)
{
	if (!HasAuthority()) return;
	if (!Character) return;

	AMainGameState* GS = GetGameState<AMainGameState>();
	if (!GS) return;

	if (Reason == EGunHoldReason::Fold)
	{
		CheckNext();
		return;
	}

	if (Reason == EGunHoldReason::Win)
	{
		bMainRevolverPutBackInProgress = false;
		FinishMainShotPhase();
		return;
	}
}
void AMainGameMode::ExecuteMainShot(bool bAutoFire)
{
	if (!HasAuthority()) return;

	AMainGameState* GS = GetGameState<AMainGameState>();
	if (!GS) return;

	if (!CurrentWinnerPS)
	{
		FinishMainShotPhase();
		return;
	}

	if (GS -> CurrentBulletCount <= 0)
	{
		StartMainRevolverPutBack();
		return;
	}
	GetWorldTimerManager().ClearTimer(TimerHandle);
	
	GS -> CurrentBulletCount -= 1;
	GS->OnTurnInfoChanged.Broadcast();
	UE_LOG(LogTemp, Warning, TEXT("[GM] Trigger pulled. RemainingTriggerCount=%d Auto=%d"), GS->CurrentBulletCount, bAutoFire);
	
	const bool bRealFire = PullMainRevolverTrigger();

	if (bRealFire)
	{
		if (ARevolver* Revolver = GetMainRevolver())
		{
			Revolver->Multicast_PlayFireSound();
		}

		FHitResult HitResult;

		AMainGamePlayerController* ShooterPC =Cast<AMainGamePlayerController>(CurrentWinnerPS->GetOwner());
		AMainPlayerState* TargetPS = GetMainShotTargetByAim(ShooterPC, HitResult);

		if (TargetPS)
		{
			UE_LOG(LogTemp, Warning, TEXT("[GM] Main revolver shot hit Player %d"), TargetPS->GetPlayerId());

			TargetPS->SetAliveState(false);

			if (GS->AlivePlayerCount > 0)
			{
				--GS->AlivePlayerCount;
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[GM] Real bullet fired into empty space"));
		}

		// 실탄이 한 번 나갔으면 이번 메인 리볼버 페이즈 종료
		StartMainRevolverPutBack();
		return;
	}

	if (ARevolver* Revolver = GetMainRevolver())
	{
		Revolver->Multicast_PlayDryFireSound();
	}

	if (GS->CurrentBulletCount <= 0)
	{
		StartMainRevolverPutBack();
		return;
	}

	ManageShotPhase();
}

ARevolver* AMainGameMode::GetMainRevolver()
{
	if (MainRevolver)
	{
		return MainRevolver;
	}

	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsWithTag(
		GetWorld(),
		FName("MainRevolver"),
		FoundActors
	);

	if (FoundActors.Num() <= 0)
	{
		UE_LOG(LogTemp, Error, TEXT("[GM] MainRevolver not found in map"));
		return nullptr;
	}

	MainRevolver = Cast<ARevolver>(FoundActors[0]);

	if (!MainRevolver)
	{
		UE_LOG(LogTemp, Error, TEXT("[GM] Found actor has MainRevolver tag but is not ARevolver"));
		return nullptr;
	}

	UE_LOG(LogTemp, Warning, TEXT("[GM] MainRevolver found: %s"), *GetNameSafe(MainRevolver));

	return MainRevolver;
}

void AMainGameMode::StartMainRevolverPutBack()
{
	if (!HasAuthority()) return;

	if (bMainRevolverPutBackInProgress) return;

	bMainRevolverPutBackInProgress = true;

	GetWorldTimerManager().ClearTimer(TimerHandle);

	if (!CurrentWinnerPS)
	{
		UE_LOG(LogTemp, Error, TEXT("[GM] StartMainRevolverPutBack failed: CurrentWinnerPS is NULL"));
		bMainRevolverPutBackInProgress = false;
		FinishMainShotPhase();
		return;
	}

	AMainGamePlayerController* PC = Cast<AMainGamePlayerController>(CurrentWinnerPS->GetOwner());
	if (!PC)
	{
		UE_LOG(LogTemp, Error, TEXT("[GM] StartMainRevolverPutBack failed: Winner PC is NULL"));
		bMainRevolverPutBackInProgress = false;
		FinishMainShotPhase();
		return;
	}

	ALobbyCharacter* WinnerCharacter = Cast<ALobbyCharacter>(PC->GetPawn());
	if (!WinnerCharacter)
	{
		UE_LOG(LogTemp, Error, TEXT("[GM] StartMainRevolverPutBack failed: WinnerCharacter is NULL"));
		bMainRevolverPutBackInProgress = false;
		FinishMainShotPhase();
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[GM] Start Main Revolver PutBack"));

	WinnerCharacter->ReturnMainRevolverToTableImmediately();
	bMainRevolverPutBackInProgress = false;
	FinishMainShotPhase();
}

void AMainGameMode::InitMainRevolverLiveBulletIfNeeded()
{
	if (MainLiveShotOffset <= 0)
	{
		RandomizeMainRevolverLiveBullet();
	}
}

void AMainGameMode::RandomizeMainRevolverLiveBullet()
{
	MainLiveShotOffset = FMath::RandRange(1, MainRevolverChamberCount);

	UE_LOG(LogTemp, Warning,
		TEXT("[GM] MainRevolver live bullet randomized. LiveShotOffset=%d / ChamberCount=%d"),
		MainLiveShotOffset,
		MainRevolverChamberCount
	);
}

bool AMainGameMode::PullMainRevolverTrigger()
{
	InitMainRevolverLiveBulletIfNeeded();

	// 방아쇠를 한 번 당겼으므로 실탄까지 남은 거리 감소
	MainLiveShotOffset--;

	if (MainLiveShotOffset <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GM] BANG! Main revolver real bullet fired."));

		MainRevolverChamberCount = MaxMainRevolverChamberCount;
		if (AMainGameState* GS = GetGameState<AMainGameState>())
		{
			GS->SetMainRevolverChamberCount(MainRevolverChamberCount);
		}

		// 실제 격발 후에는 실탄 위치를 다시 랜덤으로 설정
		RandomizeMainRevolverLiveBullet();

		return true;
	}

	MainRevolverChamberCount = FMath::Max(1, MainRevolverChamberCount - 1);
	if (AMainGameState* GS = GetGameState<AMainGameState>())
	{
		GS->SetMainRevolverChamberCount(MainRevolverChamberCount);
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[GM] Click. Empty chamber. LiveShotOffset now=%d ChamberCount=%d"),
		MainLiveShotOffset,
		MainRevolverChamberCount
	);

	return false;
}

bool AMainGameMode::GetMainShotTraceStartEnd(AMainGamePlayerController* ShooterPC, FVector& OutStart, FVector& OutEnd)
{
	if (!ShooterPC)
	{
		return false;
	}

	// VR은 파란선의 오른손 Aim, PC는 화면 중앙 시점을 기준으로 판정합니다.
	if (const ALobbyVRCharacter* VRCharacter = Cast<ALobbyVRCharacter>(ShooterPC->GetPawn()))
	{
		return VRCharacter->GetRightHandShotTrace(OutStart, OutEnd);
	}

	FVector ViewLocation;
	FRotator ViewRotation;

	ShooterPC->GetPlayerViewPoint(ViewLocation, ViewRotation);

	OutStart = ViewLocation;
	OutEnd = OutStart + ViewRotation.Vector() * MainShotTraceDistance;

	return true;
}

AMainPlayerState* AMainGameMode::GetMainShotTargetByAim(AMainGamePlayerController* ShooterPC, FHitResult& OutHit)
{
	if (!HasAuthority()) return nullptr;
	if (!ShooterPC) return nullptr;

	AMainGameState* GS = GetGameState<AMainGameState>();
	if (!GS) return nullptr;

	AMainPlayerState* ShooterPS = ShooterPC->GetPlayerState<AMainPlayerState>();
	if (!ShooterPS) return nullptr;

	FVector Start;
	FVector End;

	if (!GetMainShotTraceStartEnd(ShooterPC, Start, End))
	{
		UE_LOG(LogTemp, Warning, TEXT("[MainShot] Failed to get trace start/end"));
		return nullptr;
	}

	FCollisionQueryParams Params;
	Params.bTraceComplex = false;

	// 자기 자신 제외
	if (AActor* ShooterPawn = ShooterPC->GetPawn())
	{
		Params.AddIgnoredActor(ShooterPawn);

		if (const ALobbyCharacter* ShooterCharacter = Cast<ALobbyCharacter>(ShooterPawn))
		{
			if (ShooterCharacter->ActiveRevolver)
			{
				Params.AddIgnoredActor(ShooterCharacter->ActiveRevolver.Get());
			}
		}
	}

	if (ARevolver* Revolver = GetMainRevolver())
	{
		Params.AddIgnoredActor(Revolver);
	}

	// Fold / 사망 / 자기 자신은 타겟팅 제외
	for (ASeatActor* Seat : GS->SeatChairArray)
	{
		if (!Seat || !Seat->GetOccupant()) continue;

		ALobbyCharacter* Character = Cast<ALobbyCharacter>(Seat->GetOccupant());
		if (!Character) continue;

		AMainPlayerState* PS = Character->GetPlayerState<AMainPlayerState>();
		if (!PS) continue;

		if (PS == ShooterPS || PS->isFold || !PS->isAlive)
		{
			Params.AddIgnoredActor(Character);
		}
	}

	const bool bHit = GetWorld()->LineTraceSingleByChannel(
		OutHit,
		Start,
		End,
		ECC_Visibility,
		Params
	);

	if (!bHit)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MainShot] Miss: no actor hit"));
		return nullptr;
	}

	AActor* HitActor = OutHit.GetActor();

	UE_LOG(LogTemp, Warning, TEXT("[MainShot] HitActor=%s"), *GetNameSafe(HitActor));

	ALobbyCharacter* HitCharacter = Cast<ALobbyCharacter>(HitActor);
	if (!HitCharacter)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MainShot] Miss: hit actor is not LobbyCharacter"));
		return nullptr;
	}

	AMainPlayerState* TargetPS = HitCharacter->GetPlayerState<AMainPlayerState>();
	if (!TargetPS)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MainShot] Miss: TargetPS is NULL"));
		return nullptr;
	}

	if (TargetPS == ShooterPS)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MainShot] Miss: cannot shoot self"));
		return nullptr;
	}

	if (TargetPS->isFold)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MainShot] Miss: target is folded"));
		return nullptr;
	}

	if (!TargetPS->isAlive)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MainShot] Miss: target is already dead"));
		return nullptr;
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[MainShot] Target selected: Shooter=%d Target=%d"),
		ShooterPS->GetPlayerId(),
		TargetPS->GetPlayerId()
	);

	return TargetPS;
}

#endif // WITH_SERVER_CODE
