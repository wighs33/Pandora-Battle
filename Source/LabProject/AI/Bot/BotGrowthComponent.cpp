#include "AI/Bot/BotGrowthComponent.h"

#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "AI/Monster/MonsterCharacter.h"
#include "Algo/Sort.h"
#include "Character/PdPlayer.h"
#include "Component/AbilitySystem/PandoraTreeComponent.h"
#include "Component/Item/InventoryComponent.h"
#include "Component/Pandora/PandoraComponent.h"
#include "Component/Player/EquipmentComponent.h"
#include "Component/Player/PlayerRewardComponent.h"
#include "Component/Player/SelectingPandoraAndWeaponComponent.h"
#include "Definition/Item/ItemDefinition.h"
#include "Definition/Pandora/PandoraDefinition.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameplayEffect.h"
#include "Interface/InteractableInterface.h"
#include "Item/ItemInstance.h"
#include "Item/RewardChest.h"
#include "Mode/PdPlayerState.h"
#include "NavigationSystem.h"
#include "Pandora/PandoraLoadoutTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BotGrowthComponent)

namespace
{
	/** 다가가지 못한 대상을 다시 고르기까지의 시간. */
	constexpr double PostponeSeconds = 30.0;

	/** 상자나 몬스터 둘레에서 길 찾기 영역을 찾는 범위. 봇은 관중석처럼 영역이 성긴 곳에 설 수도 있어 더 넓게 찾는다. */
	const FVector TargetNavigationExtent(150.0, 150.0, 200.0);
	const FVector SelfNavigationExtent(500.0, 500.0, 500.0);

	/** 체력·마나가 최대치의 이 비율보다 적으면 물약을 마신다. 봇은 화면이 없어 물약을 늘 첫 퀵슬롯에 올려 쓴다. */
	constexpr float LowHealthRatio = 0.4f;
	constexpr float LowManaRatio = 0.3f;
	constexpr int32 PotionQuickSlot = 0;

	constexpr int32 LoadoutCount = UInventoryComponent::WeaponLoadoutSlotCount;

	bool IsWeapon(const UItemInstance* Item)
	{
		return Item && Item->ItemDefinition && Item->ItemDefinition->HasFragment<FItemFragment_Weapon>();
	}

	bool RestoresAttribute(const FItemFragment_Consumable& Consumable, const FGameplayAttribute& Attribute)
	{
		const UGameplayEffect* Effect = Consumable.ConsumeGameplayEffectClass.GetDefaultObject();
		return Effect && Effect->Modifiers.ContainsByPredicate([&Attribute](const FGameplayModifierInfo& Modifier)
		{
			return Modifier.Attribute == Attribute;
		});
	}

	// 판도라가 없는 칸(판도라 정의를 다 불러오기 전에 경기가 시작된 경우)에는 아무 무기나 짝으로 본다.
	bool IsMatchingWeapon(const UPandoraDefinition* Pandora, const UItemInstance* Item)
	{
		return IsWeapon(Item) && (!Pandora || Pandora->IsCompatibleWithWeaponDefinition(Item->ItemDefinition));
	}
}

UBotGrowthComponent::UBotGrowthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UBotGrowthComponent::ApplyGrowth()
{
	EquipWeapons();
	SpendSoulDust();
	DrinkPotionIfLow(UBasicAttributeSet::GetHealthAttribute(), UBasicAttributeSet::GetMaxHealthAttribute(), LowHealthRatio);
	DrinkPotionIfLow(UBasicAttributeSet::GetManaAttribute(), UBasicAttributeSet::GetMaxManaAttribute(), LowManaRatio);
	SelectLoadout();
}

APdPlayerState* UBotGrowthComponent::GetBotPlayerState() const
{
	return CastChecked<AController>(GetOwner())->GetPlayerState<APdPlayerState>();
}

const UPandoraDefinition* UBotGrowthComponent::GetPandora(const int32 LoadoutNumber) const
{
	const APdPlayerState* Bot = GetBotPlayerState();
	return Bot ? Bot->GetPandoraComponent()->GetPandoraLoadoutDefinition(PandoraLoadout::GetDirectionFromLoadoutNumber(LoadoutNumber)) : nullptr;
}

int32 UBotGrowthComponent::GetPandoraLevel(const int32 LoadoutNumber) const
{
	const UPandoraDefinition* Pandora = GetPandora(LoadoutNumber);
	return Pandora ? GetBotPlayerState()->GetPandoraTreeComponent()->GetCurrentPandoraLevel(Pandora) : 0;
}

UItemInstance* UBotGrowthComponent::GetWeapon(const int32 LoadoutNumber) const
{
	const APdPlayerState* Bot = GetBotPlayerState();
	return Bot ? Bot->GetInventoryComponent()->FindWeaponForLoadoutSlot(PandoraLoadout::GetDirectionFromLoadoutNumber(LoadoutNumber)) : nullptr;
}

bool UBotGrowthComponent::HasMatchingWeapon(const int32 LoadoutNumber) const
{
	return IsMatchingWeapon(GetPandora(LoadoutNumber), GetWeapon(LoadoutNumber));
}

bool UBotGrowthComponent::IsUsable(const int32 LoadoutNumber) const
{
	return GetPandoraLevel(LoadoutNumber) > 0 && HasMatchingWeapon(LoadoutNumber);
}

bool UBotGrowthComponent::HasUsableLoadout() const
{
	for (int32 Number = 1; Number <= LoadoutCount; ++Number)
	{
		if (IsUsable(Number))
		{
			return true;
		}
	}
	return false;
}

int32 UBotGrowthComponent::FindNextUsableLoadout() const
{
	const APdPlayerState* Bot = GetBotPlayerState();
	const int32 Current = Bot ? Bot->GetSelectingPandoraAndWeaponComponent()->GetSelectedPandoraAndWeaponNumber() : 0;
	for (int32 Offset = 0; Offset < LoadoutCount; ++Offset)
	{
		const int32 Number = (Current + Offset) % LoadoutCount + 1;
		if (Number != Current && IsUsable(Number))
		{
			return Number;
		}
	}
	return INDEX_NONE;
}

// 짝이 맞는 칸은 그대로 두고, 남은 무기로 짝이 없는 칸을 채운다. 그래도 빈 칸에는 남은 무기를 아무것이나 끼워 맨손으로 싸우지 않게 한다.
void UBotGrowthComponent::EquipWeapons() const
{
	const APdPlayerState* Bot = GetBotPlayerState();
	UInventoryComponent* Inventory = Bot ? Bot->GetInventoryComponent() : nullptr;
	if (!Inventory)
	{
		return;
	}

	TSet<const UItemInstance*> MatchedWeapons;
	for (int32 Number = 1; Number <= LoadoutCount; ++Number)
	{
		if (HasMatchingWeapon(Number))
		{
			MatchedWeapons.Add(GetWeapon(Number));
		}
	}
	for (int32 Number = 1; Number <= LoadoutCount; ++Number)
	{
		if (HasMatchingWeapon(Number))
		{
			continue;
		}
		const UPandoraDefinition* Pandora = GetPandora(Number);
		for (UItemInstance* Item : Inventory->GetAllItems().Items)
		{
			if (!MatchedWeapons.Contains(Item) && IsMatchingWeapon(Pandora, Item)
				&& Inventory->AssignWeaponToLoadoutSlot(PandoraLoadout::GetDirectionFromLoadoutNumber(Number), Item))
			{
				MatchedWeapons.Add(Item);
				break;
			}
		}
	}

	for (int32 Number = 1; Number <= LoadoutCount; ++Number)
	{
		if (GetWeapon(Number))
		{
			continue;
		}
		for (UItemInstance* Item : Inventory->GetAllItems().Items)
		{
			if (IsWeapon(Item) && Item != GetWeapon(1) && Item != GetWeapon(2) && Item != GetWeapon(3)
				&& Inventory->AssignWeaponToLoadoutSlot(PandoraLoadout::GetDirectionFromLoadoutNumber(Number), Item))
			{
				break;
			}
		}
	}
}

// 짝 무기를 끼운 칸 가운데 레벨이 가장 낮은 판도라부터 키운다. 쓸 수 있는 스킬이 빨리 늘어난다.
void UBotGrowthComponent::SpendSoulDust() const
{
	const APdPlayerState* Bot = GetBotPlayerState();
	UPandoraTreeComponent* Tree = Bot ? Bot->GetPandoraTreeComponent() : nullptr;
	while (Tree)
	{
		const UPandoraDefinition* Lowest = nullptr;
		for (int32 Number = 1; Number <= LoadoutCount; ++Number)
		{
			const UPandoraDefinition* Pandora = GetPandora(Number);
			if (Pandora && HasMatchingWeapon(Number) && Tree->CanSpendPointOnPandora(Pandora)
				&& (!Lowest || Tree->GetCurrentPandoraLevel(Pandora) < Tree->GetCurrentPandoraLevel(Lowest)))
			{
				Lowest = Pandora;
			}
		}
		if (!Lowest)
		{
			return;
		}
		Tree->SpendPointOnPandora(Lowest);
	}
}

// 마나는 저절로 차지 않으므로, 물약이 없으면 스킬도 곧 끊긴다.
void UBotGrowthComponent::DrinkPotionIfLow(const FGameplayAttribute& Attribute, const FGameplayAttribute& MaxAttribute, const float LowRatio) const
{
	const APdPlayerState* Bot = GetBotPlayerState();
	const UAbilitySystemComponent* AbilitySystem = Bot ? Bot->GetAbilitySystemComponent() : nullptr;
	if (!AbilitySystem || AbilitySystem->GetNumericAttribute(Attribute) >= AbilitySystem->GetNumericAttribute(MaxAttribute) * LowRatio)
	{
		return;
	}
	UInventoryComponent* Inventory = Bot->GetInventoryComponent();
	for (UItemInstance* Item : Inventory->GetAllItems().Items)
	{
		const FItemFragment_Consumable* Consumable = FindItemFragment<FItemFragment_Consumable>(Item ? Item->ItemDefinition.Get() : nullptr);
		if (Consumable && RestoresAttribute(*Consumable, Attribute))
		{
			if (Inventory->SetConsumableQuickSlot(PotionQuickSlot, Item))
			{
				Inventory->UseConsumableQuickSlot(PotionQuickSlot);
			}
			return;
		}
	}
}

// 지금 칸으로 스킬을 쓸 수 없으면 쓸 수 있는 칸을, 그런 칸이 없으면 무기라도 있는 칸을 든다.
// 무기를 바꾸는 동작 중이나 재사용 대기 중의 요청은 버려지므로, 칸의 무기를 손에 들 때까지 다시 요청한다.
void UBotGrowthComponent::SelectLoadout() const
{
	const APdPlayerState* Bot = GetBotPlayerState();
	USelectingPandoraAndWeaponComponent* Selection = Bot ? Bot->GetSelectingPandoraAndWeaponComponent() : nullptr;
	if (!Selection)
	{
		return;
	}
	const int32 Current = Selection->GetSelectedPandoraAndWeaponNumber();
	int32 Desired = IsUsable(Current) ? Current : FindNextUsableLoadout();
	if (Desired == INDEX_NONE && GetWeapon(Current))
	{
		Desired = Current;
	}
	for (int32 Number = 1; Desired == INDEX_NONE && Number <= LoadoutCount; ++Number)
	{
		Desired = GetWeapon(Number) ? Number : INDEX_NONE;
	}
	if (Desired != INDEX_NONE && (Desired != Current || !IsHoldingWeapon(Desired)))
	{
		Selection->RequestSelectPandoraAndWeapon(Desired);
	}
}

bool UBotGrowthComponent::IsHoldingWeapon(const int32 LoadoutNumber) const
{
	const APdPlayerState* Bot = GetBotPlayerState();
	const APdPlayer* Character = Bot ? Cast<APdPlayer>(Bot->GetPawn()) : nullptr;
	const UItemInstance* Weapon = GetWeapon(LoadoutNumber);
	return Character && Weapon && Character->GetEquipmentComponent()->GetCurrentWeaponDefinition() == Weapon->ItemDefinition;
}

bool UBotGrowthComponent::IsGrowthTarget(const APdPlayer& Self, AActor* Candidate) const
{
	if (!IsValid(Candidate) || IsPostponed(Candidate))
	{
		return false;
	}
	if (const AMonsterCharacter* Monster = Cast<AMonsterCharacter>(Candidate))
	{
		return Monster->IsMonsterReadyForAI() && !Monster->IsDead() && Self.CanDamageCharacterByTeam(Monster);
	}
	return Candidate->IsA<ARewardChest>() && IInteractableInterface::Execute_CanInteract(Candidate, const_cast<APdPlayer*>(&Self));
}

// 가까운 것부터 길이 이어졌는지 보고, 갈 수 없는 것은 미뤄 두어 매번 다시 살피지 않는다.
template <typename ActorType>
AActor* UBotGrowthComponent::FindNearestGrowthTarget(const APdPlayer& Self)
{
	TArray<AActor*> Candidates;
	for (TActorIterator<ActorType> It(GetWorld()); It; ++It)
	{
		if (IsGrowthTarget(Self, *It))
		{
			Candidates.Add(*It);
		}
	}
	Algo::SortBy(Candidates, [&Self](const AActor* Candidate) { return Self.GetSquaredDistanceTo(Candidate); });
	for (AActor* Candidate : Candidates)
	{
		if (IsReachable(Self, *Candidate))
		{
			return Candidate;
		}
		PostponeTarget(*Candidate);
	}
	return nullptr;
}

// 길 찾기 영역이 없는 맵이거나 봇 둘레에 영역이 없으면(공중) 알 수 없으므로 갈 수 있다고 보고 곧장 가 본다.
bool UBotGrowthComponent::IsReachable(const APdPlayer& Self, const AActor& Target) const
{
	const UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	const ANavigationData* NavigationData = Navigation ? Navigation->GetNavDataForProps(Self.GetNavAgentPropertiesRef()) : nullptr;
	FNavLocation Start;
	if (!NavigationData || !Navigation->ProjectPointToNavigation(Self.GetNavAgentLocation(), Start, SelfNavigationExtent))
	{
		return true;
	}
	FNavLocation End;
	return Navigation->ProjectPointToNavigation(Target.GetActorLocation(), End, TargetNavigationExtent)
		&& Navigation->TestPathSync(FPathFindingQuery(&Self, *NavigationData, Start.Location, End.Location));
}

// 무기가 하나도 없으면 상자부터 찾는다. 그 뒤로는 모자란 것을 주는 상자와 몬스터 가운데 가까운 쪽으로 간다.
AActor* UBotGrowthComponent::FindGrowthTarget(const APdPlayer& Self)
{
	const APdPlayerState* Bot = GetBotPlayerState();
	const UPandoraTreeComponent* Tree = Bot ? Bot->GetPandoraTreeComponent() : nullptr;
	if (!Tree)
	{
		return nullptr;
	}
	bool bNeedsWeapon = false;
	bool bUnarmed = true;
	int32 NeededSoulDust = 0;
	for (int32 Number = 1; Number <= LoadoutCount; ++Number)
	{
		bNeedsWeapon |= !HasMatchingWeapon(Number);
		bUnarmed &= GetWeapon(Number) == nullptr;
		const UPandoraDefinition* Pandora = GetPandora(Number);
		for (int32 Level = GetPandoraLevel(Number) + 1; Pandora && Level <= Pandora->GetMaxLevel(); ++Level)
		{
			NeededSoulDust += Pandora->GetRequiredPointsForLevel(Level);
		}
	}
	const bool bNeedsSoulDust = NeededSoulDust > Tree->GetPointsAvailable();

	AActor* Chest = bNeedsWeapon ? FindNearestGrowthTarget<ARewardChest>(Self) : nullptr;
	if (bUnarmed && Chest)
	{
		return Chest;
	}
	AActor* Monster = bNeedsSoulDust ? FindNearestGrowthTarget<AMonsterCharacter>(Self) : nullptr;
	if (!Chest || !Monster)
	{
		return Chest ? Chest : Monster;
	}
	return Self.GetSquaredDistanceTo(Chest) < Self.GetSquaredDistanceTo(Monster) ? Chest : Monster;
}

void UBotGrowthComponent::PostponeTarget(const AActor& Target)
{
	for (auto It = PostponedTargets.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}
	PostponedTargets.Add(&Target, GetWorld()->GetTimeSeconds() + PostponeSeconds);
}

bool UBotGrowthComponent::IsPostponed(const AActor* Target) const
{
	const double* PostponedUntil = PostponedTargets.Find(Target);
	return PostponedUntil && GetWorld()->GetTimeSeconds() < *PostponedUntil;
}

void UBotGrowthComponent::OpenChest(ARewardChest& Chest) const
{
	if (const APdPlayerState* Bot = GetBotPlayerState())
	{
		Bot->GetPlayerRewardComponent()->ApplyInteractRewards(&Chest);
	}
}
