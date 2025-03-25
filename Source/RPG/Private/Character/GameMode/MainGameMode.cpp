// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/GameMode/MainGameMode.h"

#include "Character/MainCharacter.h"
#include "Character/GameState/CharacterGameState.h"
#include "Character/PlayerController/CharacterPlayerController.h"
#include "Character/PlayerState/CharacterPlayerState.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"

namespace MatchState
{
	const FName Cooldown = FName("Cooldown");
}

AMainGameMode::AMainGameMode()
{
	bDelayedStart = true;
}

void AMainGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (MatchState == MatchState::WaitingToStart)
	{
		CountDownTime = WarmupTime - GetWorld()->GetTimeSeconds() + LevelStartingTime;
		if (CountDownTime < 0.f)
		{
			StartMatch();
		}
	}
	else if (MatchState == MatchState::InProgress)
	{
		CountDownTime = WarmupTime + MatchTime - GetWorld()->GetTimeSeconds() + LevelStartingTime;
		if (CountDownTime < 0.f)
		{
			SetMatchState(MatchState::Cooldown);
		}
	}
	else if (MatchState == MatchState::Cooldown)
	{
		CountDownTime = CooldownTime + WarmupTime + MatchTime - GetWorld()->GetTimeSeconds() + LevelStartingTime;
		if (CountDownTime < 0.f)
		{
			RestartGame();
		}
	}
}

void AMainGameMode::BeginPlay()
{
	Super::BeginPlay();

	LevelStartingTime = GetWorld()->GetTimeSeconds();
}

void AMainGameMode::PlayerEliminated(class AMainCharacter* ElimmedCharacter,
									 class ACharacterPlayerController* VictimController,
									 class ACharacterPlayerController* AttackerController,
									 FVector HitDirection)
{
	ACharacterPlayerState* AttackerPlayerState = AttackerController
													 ? Cast<ACharacterPlayerState>(AttackerController->PlayerState)
													 : nullptr;
	ACharacterPlayerState* VictimPlayerState = VictimController
												   ? Cast<ACharacterPlayerState>(VictimController->PlayerState)
												   : nullptr;

	ACharacterGameState* GameState = GetGameState<ACharacterGameState>();
	
	if (AttackerPlayerState && AttackerPlayerState != VictimPlayerState && GameState)
	{
		AttackerPlayerState->AddToScore(1.f);
		GameState->UpdateTopScore(AttackerPlayerState);
	}
	
	if (ElimmedCharacter)
	{
		ElimmedCharacter->Elim(HitDirection);
	}
}

void AMainGameMode::RequestRespawn(ACharacter* ElimmedCharacter, AController* ElimmedController)
{
	if (ElimmedCharacter)
	{
		ElimmedCharacter->Reset();
		ElimmedCharacter->Destroy();
	}
	if (ElimmedController)
	{
		TArray<AActor*> PlayerStarts;
		UGameplayStatics::GetAllActorsOfClass(this, APlayerStart::StaticClass(), PlayerStarts);
		int32 Selection = FMath::RandRange(0, PlayerStarts.Num() - 1);
		RestartPlayerAtPlayerStart(ElimmedController, PlayerStarts[Selection]);
	}
}

void AMainGameMode::OnMatchStateSet()
{
	Super::OnMatchStateSet();

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		ACharacterPlayerController* Character = Cast<ACharacterPlayerController>(*It);
		if (Character)
		{
			Character->OnMatchStateSet(MatchState);
		}
	}
}

