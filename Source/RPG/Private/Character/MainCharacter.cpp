// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/MainCharacter.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "Character/CharacterComponents/CombatComponent.h"
#include "Character/GameMode/MainGameMode.h"
#include "Character/PlayerController/CharacterPlayerController.h"
#include "Character/PlayerState/CharacterPlayerState.h"
#include "Character/Weapon/Weapon.h"
#include "Components/CapsuleComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Net/UnrealNetwork.h"
#include "RPG/RPG.h"

AMainCharacter::AMainCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetMesh());
	CameraBoom->TargetArmLength = 600.0f;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;

	OverheadWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("OverheadWidget"));
	OverheadWidget->SetupAttachment(RootComponent);

	CombatComponent = CreateDefaultSubobject<UCombatComponent>(TEXT("CombatComponent"));
	CombatComponent->SetIsReplicated(true);

	GetCharacterMovement()->NavAgentProps.bCanCrouch = true;
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetMesh()->SetCollisionObjectType(ECC_SkeletalMesh);
	GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	GetCharacterMovement()->RotationRate = FRotator(0.f, 0.f, 850.f);

	TurningInPlace = ETurningInPlace::ETIP_NotTurning;
	SetNetUpdateFrequency(66.f);
	SetMinNetUpdateFrequency(33.f);
}

void AMainCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(AMainCharacter, OverlappingWeapon, COND_OwnerOnly);
	DOREPLIFETIME(AMainCharacter, Health);
	DOREPLIFETIME(AMainCharacter, bDisableGameplay);
}

void AMainCharacter::HideCameraIfCharacterClose()
{
	if (!IsLocallyControlled()) return;
	if ((FollowCamera->GetComponentLocation() - GetActorLocation()).Size() < CameraThreshold)
	{
		GetMesh()->SetVisibility(false);
		if (CombatComponent && CombatComponent->EquippedWeapon && CombatComponent->EquippedWeapon->GetWeaponMesh())
		{
			CombatComponent->EquippedWeapon->GetWeaponMesh()->bOwnerNoSee = true;
		}
	}
	else
	{
		GetMesh()->SetVisibility(true);
		if (CombatComponent && CombatComponent->EquippedWeapon && CombatComponent->EquippedWeapon->GetWeaponMesh())
		{
			CombatComponent->EquippedWeapon->GetWeaponMesh()->bOwnerNoSee = false;
		}
	}
}

void AMainCharacter::SetOverlappingWeapon(AWeapon* Weapon)
{
	if (OverlappingWeapon)
	{
		OverlappingWeapon->ShowPickupWidget(false);
	}
	OverlappingWeapon = Weapon;
	if (IsLocallyControlled())
	{
		if (OverlappingWeapon)
		{
			OverlappingWeapon->ShowPickupWidget(true);
		}
	}
}

void AMainCharacter::OnRep_OverlappingWeapon(AWeapon* LastWeapon)
{
	if (OverlappingWeapon)
	{
		OverlappingWeapon->ShowPickupWidget(true);
	}
	if (LastWeapon)
	{
		LastWeapon->ShowPickupWidget(false);
	}
}

bool AMainCharacter::IsWeaponEquipped()
{
	return (CombatComponent && CombatComponent->EquippedWeapon);
}

bool AMainCharacter::IsAiming()
{
	return (CombatComponent && CombatComponent->bAiming);
}

AWeapon* AMainCharacter::GetEquippedWeapon()
{
	if (CombatComponent == nullptr) return nullptr;
	return CombatComponent->EquippedWeapon;
}

FVector AMainCharacter::GetHitTarget() const
{
	if (CombatComponent == nullptr) return FVector();
	return CombatComponent->HitTarget;
}

ECombatState AMainCharacter::GetCombatState() const
{
	if (CombatComponent == nullptr) return ECombatState::ECS_MAX;
	return CombatComponent->CombatState;
}

void AMainCharacter::OnRep_ReplicatedMovement()
{
	Super::OnRep_ReplicatedMovement();
	SimProxiesTurn();
	TimeSinceLastMovementReplication = 0.f;
}

void AMainCharacter::Destroyed()
{
	Super::Destroyed();

	AMainGameMode* GameMode = Cast<AMainGameMode>(UGameplayStatics::GetGameMode(this));
	bool bMatchNotInProgress = GameMode && GameMode->GetMatchState() != MatchState::InProgress;
	if (CombatComponent && CombatComponent->EquippedWeapon && bMatchNotInProgress)
	{
		CombatComponent->EquippedWeapon->Destroy();
	}
}

void AMainCharacter::BeginPlay()
{
	Super::BeginPlay();

	UpdateHUDHealth();
	if (HasAuthority())
	{
		OnTakeAnyDamage.AddDynamic(this, &AMainCharacter::ReceiveDamage);
	}
}

void AMainCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	RotateInPlace(DeltaTime);
	HideCameraIfCharacterClose();
	PollInit();
}

void AMainCharacter::RotateInPlace(float DeltaTime)
{
	if (bDisableGameplay)
	{
		bUseControllerRotationYaw = false;
		TurningInPlace = ETurningInPlace::ETIP_NotTurning;
		return;
	}
	
	if (GetLocalRole() > ROLE_SimulatedProxy && IsLocallyControlled())
	{
		AimOffset(DeltaTime);

		// if (CombatComponent->EquippedWeapon)
		// {
		// 	CombatComponent->EquippedWeapon->SetActorRotation(GetControlRotation());
		// }
	}
	else
	{
		TimeSinceLastMovementReplication += DeltaTime;
		if (TimeSinceLastMovementReplication > 0.25f)
		{
			OnRep_ReplicatedMovement();
		}
		CalculateAO_Pitch();
	}
}

void AMainCharacter::Move(const FInputActionValue& Value)
{
	if (bDisableGameplay) return;

	const FVector2d MovementVector = Value.Get<FVector2d>();

	const FRotator MovementRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);

	if (MovementVector.Y != 0.f)
	{
		const FVector ForwardDirection = MovementRotation.RotateVector(FVector::ForwardVector);
		AddMovementInput(ForwardDirection, MovementVector.Y);
	}

	if (MovementVector.X != 0.f)
	{
		const FVector RightDirection = MovementRotation.RotateVector(FVector::RightVector);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void AMainCharacter::Jump()
{
	if (bDisableGameplay) return;

	ACharacter::Jump();
}

void AMainCharacter::Look(const FInputActionValue& Value)
{
	const FVector2d LookAxisVector = Value.Get<FVector2d>();

	if (LookAxisVector.X != 0.f)
	{
		AddControllerYawInput(LookAxisVector.X);
	}

	if (LookAxisVector.Y != 0.f)
	{
		AddControllerPitchInput(LookAxisVector.Y);
	}
}

void AMainCharacter::Equip()
{
	if (bDisableGameplay) return;

	if (CombatComponent)
	{
		if (HasAuthority())
		{
			CombatComponent->EquipWeapon(OverlappingWeapon);
		}
		else
		{
			ServerEquipButtonPressed();
		}
	}
}

void AMainCharacter::ServerEquipButtonPressed_Implementation()
{
	if (CombatComponent)
	{
		CombatComponent->EquipWeapon(OverlappingWeapon);
	}
}

void AMainCharacter::Crouched()
{
	if (bDisableGameplay) return;

	if (bIsCrouched)
	{
		UnCrouch();
	}
	else
	{
		Crouch();
	}
}

void AMainCharacter::ReloadButtonPressed()
{
	if (bDisableGameplay) return;

	if (CombatComponent)
	{
		CombatComponent->Reload();
	}
}

void AMainCharacter::AimButtonPressed()
{
	if (bDisableGameplay) return;

	if (CombatComponent)
	{
		CombatComponent->SetAiming(true);
	}
}

void AMainCharacter::AimButtonReleased()
{
	if (bDisableGameplay) return;

	if (CombatComponent)
	{
		CombatComponent->SetAiming(false);
	}
}

void AMainCharacter::FireButtonPressed()
{
	if (bDisableGameplay) return;

	if (CombatComponent)
	{
		CombatComponent->FireButtonPressed(true);
	}
}

void AMainCharacter::FireButtonReleased()
{
	if (bDisableGameplay) return;

	if (CombatComponent)
	{
		CombatComponent->FireButtonPressed(false);
	}
}

void AMainCharacter::CalculateAO_Pitch()
{
	// AO_Pitch = GetBaseAimRotation().Pitch;
	// if (AO_Pitch > 90.f && !IsLocallyControlled())
	// {
	// 	// map pitch from [270, 360) to [-90, 0)
	// 	FVector2d InRange(270.f, 360.f);
	// 	FVector2d OutRange(-90.f, 0.f);
	// 	AO_Pitch = FMath::GetMappedRangeValueClamped(InRange, OutRange, AO_Pitch);
	// }
	
	AO_Pitch = GetBaseAimRotation().GetNormalized().Pitch;


}

float AMainCharacter::CalculateSpeed()
{
	FVector Velocity = GetVelocity();
	Velocity.Z = 0.f;
	return Velocity.Size();
}

void AMainCharacter::OnRep_Health()
{
	UpdateHUDHealth();
	PlayHitReactMontage();
}

void AMainCharacter::UpdateHUDHealth()
{
	MainCharacterPlayerController = MainCharacterPlayerController == nullptr
		                                ? Cast<ACharacterPlayerController>(Controller)
		                                : MainCharacterPlayerController;
	if (MainCharacterPlayerController)
	{
		MainCharacterPlayerController->SetHudHealth(Health, MaxHealth);
	}
}

void AMainCharacter::PollInit()
{
	if (PlayerState == nullptr)
	{
		PlayerState = GetPlayerState<ACharacterPlayerState>();
		if (PlayerState)
		{
			PlayerState->AddToScore(0.f);
		}
	}
}

void AMainCharacter::Elim(const FVector& HitDirection)
{
	if (CombatComponent && CombatComponent->EquippedWeapon)
	{
		CombatComponent->EquippedWeapon->Dropped();
	}
	MulticastElim(HitDirection);
	
	GetWorldTimerManager().SetTimer(
		ElimTimer,
		this,
		&AMainCharacter::ElimTimerFinished,
		ElimDelay
	);
}

void AMainCharacter::MulticastElim_Implementation(const FVector& HitDirection)
{
	if (MainCharacterPlayerController)
	{
		MainCharacterPlayerController->SetHudWeaponAmmo(0);
	}
	bElimmed = true;
	//PlayElimMontage();

	RagdollDeath(HitDirection);

	//Disable movement
	bDisableGameplay = true;
	if (CombatComponent)
	{
		CombatComponent->FireButtonPressed(false);
	}
	GetCharacterMovement()->StopMovementImmediately();

	//Disable collision
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
}

void AMainCharacter::ElimTimerFinished()
{
	AMainGameMode* MainGameMode = GetWorld()->GetAuthGameMode<AMainGameMode>();
	if (MainGameMode)
	{
		MainGameMode->RequestRespawn(this, Controller);
	}
}

void AMainCharacter::AimOffset(float DeltaTime)
{
	if (CombatComponent && CombatComponent->EquippedWeapon == nullptr) return;
	float Speed = CalculateSpeed();
	bool bIsInAir = GetCharacterMovement()->IsFalling();

	if (Speed == 0.f && !bIsInAir)
	{
		bRotateRootBone = true;
		FRotator CurrentAimRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.f);
		FRotator DeltaAimRotation = UKismetMathLibrary::NormalizedDeltaRotator(CurrentAimRotation, StartingAimRotation);
		AO_Yaw = DeltaAimRotation.Yaw;
		if (TurningInPlace == ETurningInPlace::ETIP_NotTurning)
		{
			InterpAO_Yaw = AO_Yaw;
		}
		bUseControllerRotationYaw = true;
		TurnInPlace(DeltaTime);
	}
	if (Speed > 0.f || bIsInAir)
	{
		bRotateRootBone = false;
		StartingAimRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.f);
		AO_Yaw = 0.f;
		bUseControllerRotationYaw = true;
		TurningInPlace = ETurningInPlace::ETIP_NotTurning;
	}
	CalculateAO_Pitch();
}

void AMainCharacter::SimProxiesTurn()
{
	if (CombatComponent == nullptr || CombatComponent->EquippedWeapon == nullptr) return;

	bRotateRootBone = false;
	float Speed = CalculateSpeed();
	if (Speed > 0.f)
	{
		TurningInPlace = ETurningInPlace::ETIP_NotTurning;
		return;
	}

	ProxyRotationLastFrame = ProxyRotation;
	ProxyRotation = GetActorRotation();
	ProxyYaw = UKismetMathLibrary::NormalizedDeltaRotator(ProxyRotation, ProxyRotationLastFrame).Yaw;

	if (FMath::Abs(ProxyYaw) > TurnThreshold)
	{
		if (ProxyYaw > TurnThreshold)
		{
			TurningInPlace = ETurningInPlace::ETIP_Right;
		}
		else if (ProxyYaw < -TurnThreshold)
		{
			TurningInPlace = ETurningInPlace::ETIP_Left;
		}
		else
		{
			TurningInPlace = ETurningInPlace::ETIP_NotTurning;
		}
		return;
	}
	TurningInPlace = ETurningInPlace::ETIP_NotTurning;
}

void AMainCharacter::TurnInPlace(float DeltaTime)
{
	if (AO_Yaw > 90.f)
	{
		TurningInPlace = ETurningInPlace::ETIP_Right;
	}
	else if (AO_Yaw < -90.f)
	{
		TurningInPlace = ETurningInPlace::ETIP_Left;
	}
	if (TurningInPlace != ETurningInPlace::ETIP_NotTurning)
	{
		InterpAO_Yaw = FMath::FInterpTo(InterpAO_Yaw, 0.f, DeltaTime, 2.f);
		AO_Yaw = InterpAO_Yaw;
		if (FMath::Abs(AO_Yaw) < 15.f)
		{
			TurningInPlace = ETurningInPlace::ETIP_NotTurning;
			StartingAimRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.f);
		}
	}
}

void AMainCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	APlayerController* PlayerController = Cast<APlayerController>(Controller);
	if (PlayerController)
	{
		UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(
			PlayerController->GetLocalPlayer());
		if (Subsystem)
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);

	Input->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AMainCharacter::Move);
	Input->BindAction(LookAction, ETriggerEvent::Triggered, this, &AMainCharacter::Look);
	Input->BindAction(JumpAction, ETriggerEvent::Triggered, this, &AMainCharacter::Jump);
	Input->BindAction(EquipAction, ETriggerEvent::Triggered, this, &AMainCharacter::Equip);
	Input->BindAction(CrouchAction, ETriggerEvent::Triggered, this, &AMainCharacter::Crouched);
	Input->BindAction(AimButtonPressedAction, ETriggerEvent::Triggered, this, &AMainCharacter::AimButtonPressed);
	Input->BindAction(AimButtonReleasedAction, ETriggerEvent::Triggered, this, &AMainCharacter::AimButtonReleased);
	Input->BindAction(FireButtonPressedAction, ETriggerEvent::Triggered, this, &AMainCharacter::FireButtonPressed);
	Input->BindAction(FireButtonReleasedAction, ETriggerEvent::Triggered, this, &AMainCharacter::FireButtonReleased);
	Input->BindAction(ReloadButtonPressedAction, ETriggerEvent::Triggered, this, &AMainCharacter::ReloadButtonPressed);
}

void AMainCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	CombatComponent->PrimaryComponentTick.bCanEverTick = true;
	if (CombatComponent)
	{
		CombatComponent->Character = this;
	}
}

void AMainCharacter::PlayFireMontage(bool bAiming)
{
	if (CombatComponent == nullptr || CombatComponent->EquippedWeapon == nullptr) return;

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && FireWeaponMontage)
	{
		AnimInstance->Montage_Play(FireWeaponMontage);
		FName SectionName = bAiming ? FName("RifleAim") : FName("RifleHip");
		AnimInstance->Montage_JumpToSection(SectionName);
	}
}

void AMainCharacter::PlayReloadMontage()
{
	if (CombatComponent == nullptr || CombatComponent->EquippedWeapon == nullptr) return;

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && ReloadMontage)
	{
		AnimInstance->Montage_Play(ReloadMontage);
		FName SectionName;

		switch (CombatComponent->EquippedWeapon->GetWeaponType())
		{
		case EWeaponType::EWT_AssaultRifle:
			SectionName = FName("Rifle");
			break;
		case EWeaponType::EWT_Pistol:
			SectionName = FName("Rifle");
			break;
		}


		AnimInstance->Montage_JumpToSection(SectionName);
	}
}

void AMainCharacter::PlayElimMontage()
{
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && ElimMontage && bElimmed)
	{
		AnimInstance->Montage_Play(ElimMontage);
	}
}

void AMainCharacter::RagdollDeath(const FVector& HitDirection)
{
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
	GetMesh()->SetSimulatePhysics(true);
	GetCharacterMovement()->DisableMovement();

  // Apply directional impulse to push the body
    FVector Impulse = -HitDirection * 10000.0f;
    GetMesh()->AddImpulse(Impulse, NAME_None, true);

    // Apply angular impulse (random torque)
    FVector AngularImpulse = FVector(FMath::FRandRange(-5000.0f, 5000.0f), 
                                     FMath::FRandRange(-5000.0f, 5000.0f), 
                                     FMath::FRandRange(-2500.0f, 2500.0f));
    GetMesh()->AddAngularImpulseInDegrees(AngularImpulse, NAME_None, true);

    // Adjust gravity scale per limb
    TMap<FName, float> LimbGravityScales = {
        {"head", 0.8f}, {"upperarm_l", 0.8f}, {"upperarm_r", 0.8f},
        {"lowerarm_l", 0.8f}, {"lowerarm_r", 0.8f}, {"hand_l", 0.8f}, {"hand_r", 0.8f},
        {"thigh_l", 1.2f}, {"thigh_r", 1.2f}, {"calf_l", 1.2f}, {"calf_r", 1.2f},
        {"foot_l", 1.2f}, {"foot_r", 1.2f}
    };

    for (const TPair<FName, float>& Limb : LimbGravityScales)
    {
        GetMesh()->SetMassScale(Limb.Key, Limb.Value); // Correct function
    }

    // Apply per-limb impulse (stronger effect on head and arms)
	TMap<FName, float> LimbImpulseScales = {
	    {"head", 12000.0f}, {"upperarm_l", 9000.0f}, {"upperarm_r", 9000.0f},
		{"lowerarm_l", 7000.0f}, {"lowerarm_r", 7000.0f}, {"hand_l", 5000.0f}, {"hand_r", 5000.0f},
		{"thigh_l", 8000.0f}, {"thigh_r", 8000.0f}, {"calf_l", 6000.0f}, {"calf_r", 6000.0f}
	};

    for (const TPair<FName, float>& Limb : LimbImpulseScales)
    {
        FVector RandomImpulse = FVector(FMath::FRandRange(-Limb.Value, Limb.Value), 
                                        FMath::FRandRange(-Limb.Value, Limb.Value), 
                                        FMath::FRandRange(0, Limb.Value / 3)); // Less upward force

        GetMesh()->AddImpulse(RandomImpulse, Limb.Key, true);
    }

    // Adjust damping per limb (controls how much limbs resist movement)
    TMap<FName, float> LimbDamping = {
        {"spine_01", 5.0f}, {"spine_02", 4.0f}, {"spine_03", 3.5f},  // Spine should be stable
        {"head", 1.0f}, {"upperarm_l", 1.2f}, {"upperarm_r", 1.2f},
        {"lowerarm_l", 1.0f}, {"lowerarm_r", 1.0f}, {"hand_l", 0.5f}, {"hand_r", 0.5f},
        {"thigh_l", 2.0f}, {"thigh_r", 2.0f}, {"calf_l", 1.5f}, {"calf_r", 1.5f}
    };

    for (const TPair<FName, float>& Limb : LimbDamping)
    {
        GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector, false, Limb.Key);
        GetMesh()->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector, false, Limb.Key);
        
        // Adjust damping values via the physics body instance
        FBodyInstance* BodyInstance = GetMesh()->GetBodyInstance(Limb.Key);
        if (BodyInstance)
        {
            BodyInstance->LinearDamping = Limb.Value;
            BodyInstance->AngularDamping = Limb.Value;
        }
    }
}

void AMainCharacter::PlayHitReactMontage()
{
	//if (Combat == nullptr || Combat->EquippedWeapon == nullptr) return;

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && HitReactMontage && !bElimmed)
	{
		AnimInstance->Montage_Play(HitReactMontage);
		FName SectionName("FromFront");
		AnimInstance->Montage_JumpToSection(SectionName);
	}
}

void AMainCharacter::ReceiveDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType,
                                   class AController* InstigatorController, AActor* DamageCauser)
{
	Health = FMath::Clamp(Health - Damage, 0.f, MaxHealth);
	UpdateHUDHealth();
	PlayHitReactMontage();

	if (Health == 0.f)
	{
		AMainGameMode* MainGameMode = GetWorld()->GetAuthGameMode<AMainGameMode>();
		if (MainGameMode && !IsElimmed())
		{
			FVector HitDirection = (DamageCauser->GetActorLocation() - GetActorLocation()).GetSafeNormal();

			MainCharacterPlayerController = MainCharacterPlayerController == nullptr
				                                ? Cast<ACharacterPlayerController>(Controller)
				                                : MainCharacterPlayerController;
			ACharacterPlayerController* AttackerController = Cast<ACharacterPlayerController>(InstigatorController);
			MainGameMode->PlayerEliminated(this, MainCharacterPlayerController, AttackerController, HitDirection);
		}
	}
}
