#include "Weapon/ImpactDecalPool.h"

#include "Components/DecalComponent.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ImpactDecalPool)

namespace
{
	/**
	 * 한꺼번에 남겨 둘 탄흔 수. 한 명이 연사 간격(0.12초)으로 수명(10초) 내내 쏘면 약 83개가 남으므로 그보다 넉넉하게 두고,
	 * 여럿이 함께 쏠 때만 가장 오래된 탄흔부터 다시 쓴다.
	 */
	constexpr int32 MaxImpactDecals = 128;
}

bool UImpactDecalPool::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
}

void UImpactDecalPool::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		for (FImpactDecalSlot& Slot : Slots)
		{
			World->GetTimerManager().ClearTimer(Slot.HideTimer);
		}
	}
	Slots.Reset();
	Super::Deinitialize();
}

void UImpactDecalPool::PlaceDecal(UMaterialInterface& Material, const FVector& Size, const FVector& Location, const FRotator& Rotation,
	const float LifeSpan)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const int32 SlotIndex = NextSlotIndex;
	NextSlotIndex = (NextSlotIndex + 1) % MaxImpactDecals;
	if (!Slots.IsValidIndex(SlotIndex))
	{
		UDecalComponent* NewDecal = CreateDecal();
		if (!NewDecal)
		{
			return;
		}
		Slots.Add({NewDecal});
	}

	// SetDecalMaterial이 렌더 상태를 다시 만들므로, 크기를 먼저 바꿔 두면 함께 반영된다.
	FImpactDecalSlot& Slot = Slots[SlotIndex];
	Slot.Decal->DecalSize = Size;
	Slot.Decal->SetDecalMaterial(&Material);
	Slot.Decal->SetWorldLocationAndRotation(Location, Rotation);
	Slot.Decal->SetVisibility(true);

	if (LifeSpan > 0.0f)
	{
		World->GetTimerManager().SetTimer(Slot.HideTimer,
			FTimerDelegate::CreateUObject(this, &ThisClass::HideDecal, SlotIndex), LifeSpan, false);
	}
	else
	{
		World->GetTimerManager().ClearTimer(Slot.HideTimer);
	}
}

// 엔진의 SpawnDecalAtLocation과 같은 방식(월드 설정 액터 소유, 절대 크기)으로 만든다.
UDecalComponent* UImpactDecalPool::CreateDecal() const
{
	UWorld* World = GetWorld();
	AWorldSettings* WorldSettings = World ? World->GetWorldSettings() : nullptr;
	if (!WorldSettings)
	{
		return nullptr;
	}

	UDecalComponent* Decal = NewObject<UDecalComponent>(WorldSettings);
	Decal->SetUsingAbsoluteScale(true);
	Decal->RegisterComponentWithWorld(World);
	return Decal;
}

void UImpactDecalPool::HideDecal(const int32 SlotIndex)
{
	if (Slots.IsValidIndex(SlotIndex) && Slots[SlotIndex].Decal)
	{
		Slots[SlotIndex].Decal->SetVisibility(false);
	}
}
