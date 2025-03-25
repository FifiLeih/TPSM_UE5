// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/PlayerController/CharacterPlayerController.h"

#include "Character/MainCharacter.h"
#include "Character/CharacterComponents/CombatComponent.h"
#include "Character/GameMode/MainGameMode.h"
#include "Character/GameState/CharacterGameState.h"
#include "Character/HUD/Announcment.h"
#include "Character/HUD/CharacterHUD.h"
#include "Character/HUD/CharacterOverlay.h"
#include "Character/PlayerState/CharacterPlayerState.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "GameFramework/GameMode.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

void ACharacterPlayerController::BeginPlay()
{
	Super::BeginPlay();
	CharacterHUD = Cast<ACharacterHUD>(GetHUD());
	ServerCheckMatchState();
}

void ACharacterPlayerController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	SetHudTime();
	CheckTimeSync(DeltaTime);
	PollInit();
}

void ACharacterPlayerController::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACharacterPlayerController, MatchState);
}

void ACharacterPlayerController::CheckTimeSync(float DeltaTime)
{
	TimeSyncRunningTime += DeltaTime;
	if (IsLocalController() && TimeSyncRunningTime > TimeSyncFrequency)
	{
		ServerRequestServerTime(GetWorld()->GetTimeSeconds());
		TimeSyncRunningTime = 0.f;
	}
}

void ACharacterPlayerController::ServerCheckMatchState_Implementation()
{
	AMainGameMode* GameMode = Cast<AMainGameMode>(UGameplayStatics::GetGameMode(this));
	if (GameMode)
	{
		WarmupTime = GameMode->WarmupTime;
		MatchTime = GameMode->MatchTime;
		CooldownTime = GameMode->CooldownTime;
		LevelStartingTime = GameMode->LevelStartingTime;
		MatchState = GameMode->GetMatchState();
		ClientJoinMidgame(MatchState, WarmupTime, MatchTime, CooldownTime , LevelStartingTime);
		if (CharacterHUD && MatchState == MatchState::WaitingToStart)
		{
			CharacterHUD->AddAnnouncement();
		}
	}
}

void ACharacterPlayerController::ClientJoinMidgame_Implementation(FName StateOfMatch, float Warmup, float Match, float Cooldown, float StartingTime)
{
	WarmupTime = Warmup;
	MatchTime = Match;
	CooldownTime = Cooldown;
	LevelStartingTime = StartingTime;
	MatchState = StateOfMatch;
	OnMatchStateSet(MatchState);
	if (CharacterHUD && MatchState == MatchState::WaitingToStart)
	{
		CharacterHUD->AddAnnouncement();
	}
}

void ACharacterPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	AMainCharacter* MainCharacter = Cast<AMainCharacter>(InPawn);
	if (MainCharacter)
	{
		SetHudHealth(MainCharacter->GetHealth(), MainCharacter->GetMaxHealth());
	}
}

void ACharacterPlayerController::SetHudHealth(float Health, float MaxHealth)
{
	CharacterHUD = CharacterHUD == nullptr ? Cast<ACharacterHUD>(GetHUD()) : CharacterHUD;

	bool bHudValid = CharacterHUD && CharacterHUD->CharacterOverlay;
	
	if (bHudValid)
	{
		const float HealthPercent = Health / MaxHealth;
		CharacterHUD->CharacterOverlay->HealthBar->SetPercent(HealthPercent);
		FString HealthText = FString::Printf(TEXT("%d/%d"), FMath::CeilToInt(Health), FMath::CeilToInt(MaxHealth));
		CharacterHUD->CharacterOverlay->HealthText->SetText(FText::FromString(HealthText));
	}
	else
	{
		bInitializeCharacterOverlay = true;
		HudHealth = Health;
		HudMaxHealth = MaxHealth;
	}
}

void ACharacterPlayerController::SetHudScore(float Score)
{
	CharacterHUD = CharacterHUD == nullptr ? Cast<ACharacterHUD>(GetHUD()) : CharacterHUD;

	bool bHudValid = CharacterHUD && CharacterHUD->CharacterOverlay && CharacterHUD->CharacterOverlay->ScoreAmount;

	if (bHudValid)
	{
		FString ScoreText = FString::Printf(TEXT("%d"), FMath::FloorToInt(Score));
		CharacterHUD->CharacterOverlay->ScoreAmount->SetText(FText::FromString(ScoreText));
	}
	else
	{
		bInitializeCharacterOverlay = true;
		HudScore = Score;
	}
}

void ACharacterPlayerController::SetHudWeaponAmmo(int32 Ammo)
{
	CharacterHUD = CharacterHUD == nullptr ? Cast<ACharacterHUD>(GetHUD()) : CharacterHUD;

	bool bHudValid = CharacterHUD && CharacterHUD->CharacterOverlay && CharacterHUD->CharacterOverlay->WeaponAmmoAmount ;

	if (bHudValid)
	{
		FString AmmoText = FString::Printf(TEXT("%d"), Ammo);
		CharacterHUD->CharacterOverlay->WeaponAmmoAmount->SetText(FText::FromString(AmmoText));
	}
}

void ACharacterPlayerController::SetHudCarriedAmmo(int32 Ammo)
{
	CharacterHUD = CharacterHUD == nullptr ? Cast<ACharacterHUD>(GetHUD()) : CharacterHUD;

	bool bHudValid = CharacterHUD && CharacterHUD->CharacterOverlay && CharacterHUD->CharacterOverlay->CarriedAmmoAmount; ;

	if (bHudValid)
	{
		FString AmmoText = FString::Printf(TEXT("%d"), Ammo);
		CharacterHUD->CharacterOverlay->CarriedAmmoAmount->SetText(FText::FromString(AmmoText));
	}
}

void ACharacterPlayerController::SetHudMatchCountdown(float CountdownTime)
{
	CharacterHUD = CharacterHUD == nullptr ? Cast<ACharacterHUD>(GetHUD()) : CharacterHUD;

	bool bHudValid = CharacterHUD && CharacterHUD->CharacterOverlay && CharacterHUD->CharacterOverlay->MatchCountdownText;

	if (bHudValid)
	{
		if (CountdownTime < 0.f)
		{
			CharacterHUD->CharacterOverlay->MatchCountdownText->SetText(FText());
			return;
		}
		int32 Minutes = FMath::FloorToInt(CountdownTime / 60.f);
		int32 Seconds = CountdownTime - Minutes * 60.f;
		
		FString CountdownText = FString::Printf(TEXT("%02d:%02d"), Minutes, Seconds);
		CharacterHUD->CharacterOverlay->MatchCountdownText->SetText(FText::FromString(CountdownText));
	}
}

void ACharacterPlayerController::SetHudAnnouncementCountdown(float CountdownTime)
{
	CharacterHUD = CharacterHUD == nullptr ? Cast<ACharacterHUD>(GetHUD()) : CharacterHUD;

	bool bHudValid = CharacterHUD && CharacterHUD->Announcement && CharacterHUD->Announcement->WarmupTime;

	if (bHudValid)
	{
		if (CountdownTime < 0.f)
		{
			CharacterHUD->Announcement->WarmupTime->SetText(FText());
			return;
		}
		int32 Minutes = FMath::FloorToInt(CountdownTime / 60.f);
		int32 Seconds = CountdownTime - Minutes * 60.f;
		
		FString CountdownText = FString::Printf(TEXT("%02d:%02d"), Minutes, Seconds);
		CharacterHUD->Announcement->WarmupTime->SetText(FText::FromString(CountdownText));
	}
}

void ACharacterPlayerController::SetHudTime()
{
	float TimeLeft = 0.f;
	if (MatchState == MatchState::WaitingToStart) TimeLeft = WarmupTime - GetServerTime() + LevelStartingTime;
	else if (MatchState == MatchState::InProgress) TimeLeft = WarmupTime + MatchTime - GetServerTime() + LevelStartingTime;
	else if (MatchState == MatchState::Cooldown) TimeLeft = CooldownTime + WarmupTime + MatchTime - GetServerTime() + LevelStartingTime;
	uint32 SecondsLeft = FMath::CeilToInt(TimeLeft);

	if (HasAuthority())
	{
		MainGameMode = MainGameMode == nullptr ? Cast<AMainGameMode>(UGameplayStatics::GetGameMode(this)) : MainGameMode;
		if (MainGameMode)
		{
			SecondsLeft = FMath::CeilToInt(MainGameMode->GetCountdownTime() + LevelStartingTime);
		}
	}
	
	if (CountdownInt != SecondsLeft)
	{
		if (MatchState == MatchState::WaitingToStart || MatchState == MatchState::Cooldown)
		{
			SetHudAnnouncementCountdown(TimeLeft);
		}
		if (MatchState == MatchState::InProgress)
		{
			SetHudMatchCountdown(TimeLeft);
		}
	}
	CountdownInt = SecondsLeft;
}

void ACharacterPlayerController::PollInit()
{
	if (CharacterOverlay == nullptr)
	{
		if (CharacterHUD && CharacterHUD->CharacterOverlay)
		{
			CharacterOverlay = CharacterHUD->CharacterOverlay;
			if (CharacterOverlay)
			{
				SetHudHealth(HudHealth, HudMaxHealth);
				SetHudScore(HudScore);
			}
		}
	}
}

void ACharacterPlayerController::ServerRequestServerTime_Implementation(float TimeOfClientRequest)
{
	float ServerTimeOfReceipt = GetWorld()->GetTimeSeconds();
	ClientReportServerTime(TimeOfClientRequest, ServerTimeOfReceipt);
}

void ACharacterPlayerController::ClientReportServerTime_Implementation(float TimeOfClientRequest,
	float TimeServerReceivedClientRequest)
{
	float RoundTripTime = GetWorld()->GetTimeSeconds() - TimeOfClientRequest;
	float CurrentServerTime = (0.5f * RoundTripTime) + TimeServerReceivedClientRequest;
	ClientServerDelta = CurrentServerTime - GetWorld()->GetTimeSeconds();
}

float ACharacterPlayerController::GetServerTime()
{
	if (HasAuthority()) return GetWorld()->GetTimeSeconds();
	return GetWorld()->GetTimeSeconds() + ClientServerDelta;
}

void ACharacterPlayerController::ReceivedPlayer()
{
	Super::ReceivedPlayer();
	if (IsLocalController())
	{
		ServerRequestServerTime(GetWorld()->GetTimeSeconds());
	}
}

void ACharacterPlayerController::OnMatchStateSet(FName State)
{
	MatchState = State;
	
	if (MatchState == MatchState::InProgress)
	{
		HandleMatchHasStarted();
	}
	else if (MatchState == MatchState::Cooldown)
	{
		HandleCooldown();
	}
}

void ACharacterPlayerController::OnRep_MatchState()
{
	if (MatchState == MatchState::InProgress)
	{
		HandleMatchHasStarted();
	}
	else if (MatchState == MatchState::Cooldown)
	{
		HandleCooldown();
	}
}

void ACharacterPlayerController::HandleMatchHasStarted()
{
	CharacterHUD = CharacterHUD == nullptr ? Cast<ACharacterHUD>(GetHUD()) : CharacterHUD;
	if (CharacterHUD)
	{
		CharacterHUD->AddCharacterOverlay();
		if (CharacterHUD->Announcement)
		{
			CharacterHUD->Announcement->SetVisibility(ESlateVisibility::Hidden);
		}
	}
}

void ACharacterPlayerController::HandleCooldown()
{
	CharacterHUD = CharacterHUD == nullptr ? Cast<ACharacterHUD>(GetHUD()) : CharacterHUD;
	if (CharacterHUD)
	{
		CharacterHUD->CharacterOverlay->RemoveFromParent();
		if (CharacterHUD->Announcement && CharacterHUD->Announcement->AnnouncementText)
		{
			CharacterHUD->Announcement->SetVisibility(ESlateVisibility::Visible);
			FString AnnouncementText("New Match Starts in:");
			CharacterHUD->Announcement->AnnouncementText->SetText(FText::FromString(AnnouncementText));

			ACharacterGameState* GameState = Cast <ACharacterGameState>(UGameplayStatics::GetGameState(this));
			ACharacterPlayerState* PlayerState = GetPlayerState<ACharacterPlayerState>();
			if (GameState && PlayerState)
			{
				TArray<ACharacterPlayerState*> TopPlayers = GameState->TopScoringPlayers;
				FString InfoTextString;
				if (TopPlayers.Num() == 0)
				{
					InfoTextString = FString("No winner");
				}
				else if (TopPlayers.Num() == 1 && TopPlayers[0] == PlayerState)
				{
					InfoTextString = FString("You are the winner");
				}
				else if (TopPlayers.Num() == 1)
				{
					InfoTextString = FString::Printf(TEXT("Winner: \n%s"), *TopPlayers[0]->GetPlayerName());
				}
				else if (TopPlayers.Num() > 1)
				{
					InfoTextString = FString("Too many winners: \n");
					for (auto TiePlayers : TopPlayers)
					{
						InfoTextString.Append(FString::Printf(TEXT("%s"), *TiePlayers->GetPlayerName()));
					}
				}
				CharacterHUD->Announcement->InfoText->SetText(FText::FromString(InfoTextString));
			}
		}
	}
	AMainCharacter* MainCharacter = Cast<AMainCharacter>(GetPawn());
	if (MainCharacter && MainCharacter->GetCombatComponent())
	{
		MainCharacter->bDisableGameplay = true;
		MainCharacter->GetCombatComponent()->FireButtonPressed(false);
	}
}