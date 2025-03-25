// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/Weapon/HitScanWeapon.h"

#include "Character/MainCharacter.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Kismet/GameplayStatics.h"

void AHitScanWeapon::Fire(const FVector& HitTarget)
{
	Super::Fire(HitTarget);

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (OwnerPawn == nullptr) return;
	AController* InstigatorController = OwnerPawn->GetController();

	const USkeletalMeshSocket* MuzzleFlashSocket = GetWeaponMesh()->GetSocketByName("MuzzleFlash");
	if (MuzzleFlashSocket && InstigatorController)
	{
		FTransform SocketTransform = MuzzleFlashSocket->GetSocketTransform(GetWeaponMesh());
		FVector Start = SocketTransform.GetLocation();
		FVector End = Start + (HitTarget - Start) * 1.25f;

		FHitResult FireHit;
		UWorld* World = GetWorld();
		if (World)
		{
			World->LineTraceSingleByChannel(
				FireHit,
				Start,
				End,
				ECC_Visibility);
			if (FireHit.bBlockingHit)
			{
				AMainCharacter* MainCharacter = Cast<AMainCharacter>(FireHit.GetActor());
				if (MainCharacter)
				{
					if (HasAuthority())
					{
						FVector HitDirection = (FireHit.ImpactPoint - Start).GetSafeNormal();

						UGameplayStatics::ApplyPointDamage(
							MainCharacter,
							Damage,
							HitDirection,
							FireHit,
							InstigatorController,
							this,
							UDamageType::StaticClass()
							);
					}
				}
				if (ImpactParticles)
				{
					UGameplayStatics::SpawnEmitterAtLocation(
						World,
						ImpactParticles,
						FireHit.ImpactPoint,
						FireHit.ImpactNormal.Rotation()
						);
				}
			}
		}
	}
}
