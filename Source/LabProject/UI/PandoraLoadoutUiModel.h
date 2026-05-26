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
	static EEnum_Direction GetDirectionFromSelectSlotNumber(int32 SlotNumber);
	static int32 GetSelectSlotNumberFromDirection(EEnum_Direction Direction);
	static TArray<FPandoraSelectSlotUiData> BuildSelectSlots(
		const UPandoraComponent* PandoraComponent,
		const UItemInstance* LeftWeapon,
		const UItemInstance* UpWeapon,
		const UItemInstance* RightWeapon);
	static bool IsPandoraCompatibleWithWeapon(const UPandoraDefinition* PandoraDefinition, const UItemInstance* WeaponInstance);

private:
	static const UItemInstance* GetWeaponForDirection(
		EEnum_Direction Direction,
		const UItemInstance* LeftWeapon,
		const UItemInstance* UpWeapon,
		const UItemInstance* RightWeapon);
};
