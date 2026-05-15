#include "Weapon/Sword.h"

#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(Sword)

ASword::ASword()
{
	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	Box->SetupAttachment(WeaponMesh);
	InitializeCollisionBox(Box);
}

UBoxComponent* ASword::GetCollisionBox() const
{
	return Box;
}
