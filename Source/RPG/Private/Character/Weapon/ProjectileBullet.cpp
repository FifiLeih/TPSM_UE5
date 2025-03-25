// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/Weapon/ProjectileBullet.h"

#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"

void AProjectileBullet::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
                              FVector NormalImpulse, const FHitResult& Hit)
{

	ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (OwnerCharacter)
	{
		AController* OwnerController = OwnerCharacter->Controller;
		if (OwnerController)
		{
			FVector HitDirection = (Hit.ImpactPoint - GetActorLocation()).GetSafeNormal();

			UGameplayStatics::ApplyPointDamage(
					OtherActor,
					Damage,
					HitDirection,
					Hit,
					OwnerController,
					this,
					UDamageType::StaticClass()
				);
		}
	}
	Super::OnHit(HitComp, OtherActor, OtherComp, NormalImpulse, Hit);
}
