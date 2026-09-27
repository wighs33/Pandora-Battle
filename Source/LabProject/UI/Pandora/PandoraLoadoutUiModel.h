#pragma once

#include "Common/Enum_Direction.h"
#include "CoreMinimal.h"

class UItemInstance;
class UPandoraComponent;
class UPandoraDefinition;
class UTexture2D;

struct LABPROJECT_API FPandoraSelectSlotUiData
{
	EEnum_Direction Direction = EEnum_Direction::Center;
	int32 SlotNumber = 0;
	const UPandoraDefinition* PandoraDefinition = nullptr;
	UTexture2D* IconTexture = nullptr;
	bool bCompatibleWithWeapon = true;
};

class LABPROJECT_API FPandoraLoadoutUiModel
{

public:
	static TArray<FPandoraSelectSlotUiData> BuildSelectSlots(
		const UPandoraComponent* PandoraComponent,
		const UItemInstance* LeftWeapon,
		const UItemInstance* UpWeapon,
		const UItemInstance* RightWeapon);

private:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	static bool IsPandoraCompatibleWithWeapon(const UPandoraDefinition* PandoraDefinition, const UItemInstance* WeaponInstance);
	static const UItemInstance* GetWeaponForDirection(
		EEnum_Direction Direction,
		const UItemInstance* LeftWeapon,
		const UItemInstance* UpWeapon,
		const UItemInstance* RightWeapon);
};
