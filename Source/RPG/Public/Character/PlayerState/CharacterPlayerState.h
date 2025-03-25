// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "CharacterPlayerState.generated.h"

/**
 * 
 */
UCLASS()
class RPG_API ACharacterPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const override;
	virtual void OnRep_Score() override;
	void AddToScore(float ScoreAmount);
	
private:
	UPROPERTY()
	class AMainCharacter* Character;
	UPROPERTY()
	class ACharacterPlayerController* CharacterController;
};
