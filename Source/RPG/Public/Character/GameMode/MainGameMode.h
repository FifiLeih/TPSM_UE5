// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "MainGameMode.generated.h"


namespace MatchState
{
	extern RPG_API const FName Cooldown; // Match has ended. Display winner and timer
}

/**
 * 
 */
UCLASS()
class RPG_API AMainGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	AMainGameMode();
	virtual void Tick(float DeltaSeconds) override;
	virtual void PlayerEliminated(class AMainCharacter* ElimmedCharacter,
	                              class ACharacterPlayerController* VictimController,
	                              class ACharacterPlayerController* AttackerController,
	                              FVector HitDirection);
	virtual void RequestRespawn(ACharacter* ElimmedCharacter, AController* ElimmedController);

	UPROPERTY(EditDefaultsOnly)
	float WarmupTime = 10.f;
	
	UPROPERTY(EditDefaultsOnly)
	float MatchTime = 120.f;

	UPROPERTY(EditDefaultsOnly)
	float CooldownTime = 10.f;

	float LevelStartingTime = 0.f;

private:
	float CountDownTime = 0.f;

public:
	FORCEINLINE float GetCountdownTime() const { return CountDownTime; }

protected:
	virtual void BeginPlay() override;
	virtual void OnMatchStateSet() override;
};
