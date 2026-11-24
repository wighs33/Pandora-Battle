#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BotGrowthComponent.generated.h"

class APdPlayer;
class APdPlayerState;
class ARewardChest;
class UItemInstance;
class UPandoraDefinition;
struct FGameplayAttribute;

/**
 * 경기 중 봇의 성장(서버). 봇도 사람처럼 무기 없이 0레벨 판도라로 시작해, 상자를 열어 무기를 얻고 몬스터에게서 얻은 소울 더스트로 판도라를 키운다.
 * 어디로 갈지 고르고 얻은 것을 쓰는 일을 맡는다. 움직이고 싸우는 일은 APdBotController가 한다.
 */
UCLASS()
class LABPROJECT_API UBotGrowthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Public API ------------------------------------------------------------------------------------------------------
	UBotGrowthComponent();

	/** 얻은 무기를 짝이 맞는 칸에 끼우고, 소울 더스트와 물약을 쓰고, 스킬을 쓸 수 있는 칸을 든다. */
	void ApplyGrowth();

	/** 스킬을 쓸 수 있는 칸이 있는지. 칸의 판도라가 1레벨 이상이고 짝 무기를 끼운 칸이다. */
	bool HasUsableLoadout() const;

	/** 지금 든 칸 다음으로 스킬을 쓸 수 있는 칸의 번호. 없으면 INDEX_NONE. */
	int32 FindNextUsableLoadout() const;

	/** 모자란 무기나 소울 더스트를 얻으러 갈 가장 가까운 상자나 몬스터. 다 키웠거나 갈 곳이 없으면 nullptr. */
	AActor* FindGrowthTarget(const APdPlayer& Self);

	/** 지금 얻을 것이 있는 상자나 몬스터인지. 열린 상자, 죽은 몬스터, 다가가지 못해 미뤄 둔 대상은 아니다. */
	bool IsGrowthTarget(const APdPlayer& Self, AActor* Candidate) const;

	/** 다가가지 못한 대상(적도 포함)을 한동안 고르지 않는다. */
	void PostponeTarget(const AActor& Target);
	bool IsPostponed(const AActor* Target) const;

	/** 상자를 열어 안의 보상을 받는다. 사람의 상호작용과 같은 서버 경로다. */
	void OpenChest(ARewardChest& Chest) const;

private:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	APdPlayerState* GetBotPlayerState() const;
	const UPandoraDefinition* GetPandora(int32 LoadoutNumber) const;
	int32 GetPandoraLevel(int32 LoadoutNumber) const;
	UItemInstance* GetWeapon(int32 LoadoutNumber) const;
	bool HasMatchingWeapon(int32 LoadoutNumber) const;
	bool IsUsable(int32 LoadoutNumber) const;
	bool IsHoldingWeapon(int32 LoadoutNumber) const;

	void EquipWeapons() const;
	void SpendSoulDust() const;
	void DrinkPotionIfLow(const FGameplayAttribute& Attribute, const FGameplayAttribute& MaxAttribute, float LowRatio) const;
	void SelectLoadout() const;

	template <typename ActorType>
	AActor* FindNearestGrowthTarget(const APdPlayer& Self);
	/** 걸어서 갈 수 있는지. 봇은 갈고리나 포탈을 쓰지 않으므로 길이 이어지지 않은 위층에는 가지 못한다. */
	bool IsReachable(const APdPlayer& Self, const AActor& Target) const;

	/** 다가가지 못해 미뤄 둔 대상과 다시 골라도 되는 시각. */
	TMap<TWeakObjectPtr<const AActor>, double> PostponedTargets;
};
