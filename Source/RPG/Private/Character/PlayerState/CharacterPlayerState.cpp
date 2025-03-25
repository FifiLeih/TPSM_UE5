// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/PlayerState/CharacterPlayerState.h"
#include "Character/MainCharacter.h"
#include "Character/PlayerController/CharacterPlayerController.h"
#include "Net/UnrealNetwork.h"

void ACharacterPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
}

void ACharacterPlayerState::AddToScore(float ScoreAmount)
{
	SetScore(GetScore() + ScoreAmount);
	Character = Character == nullptr ? Cast<AMainCharacter>(GetPawn()) : Character;
	if (Character)
	{
		CharacterController = CharacterController == nullptr ? Cast<ACharacterPlayerController>(Character->Controller) : CharacterController;
		if (CharacterController)
		{
			CharacterController->SetHudScore(GetScore());
		}
	}
}

void ACharacterPlayerState::OnRep_Score()
{
	Super::OnRep_Score();
	
	Character = Character == nullptr ? Cast<AMainCharacter>(GetPawn()) : Character;
	if (Character)
	{
		CharacterController = CharacterController == nullptr ? Cast<ACharacterPlayerController>(Character->Controller) : CharacterController;
		if (CharacterController)
		{
			CharacterController->SetHudScore(GetScore());
		}
	}
}
