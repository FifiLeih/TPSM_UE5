// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "CharacterGameState.generated.h"

/**
 * 
 */
UCLASS()
class RPG_API ACharacterGameState : public AGameState
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	void UpdateTopScore(class ACharacterPlayerState* ScoringPlayer);

	UPROPERTY(replicated)
	TArray<ACharacterPlayerState*> TopScoringPlayers;

private:
	float TopScore = 0.f;
};
