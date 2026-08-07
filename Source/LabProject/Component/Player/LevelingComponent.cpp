#include "Component/Player/LevelingComponent.h"

#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "Common/Enum_Operation.h"
#include "GameplayEffect.h"
#include "GameFramework/PlayerState.h"
#include "Definition/Item/RewardDefinition.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Component/Player/PlayerNotificationComponent.h"
#include "Mode/PdPlayerState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LevelingComponent)

DEFINE_LOG_CATEGORY(LogLevelingComponent);

ULevelingComponent::ULevelingComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);
}

void ULevelingComponent::BeginPlay()
{
	Super::BeginPlay();
	BeginPlayerKillRewardPreload();
}

void ULevelingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ReleasePlayerKillRewardPreload();
	Super::EndPlay(EndPlayReason);
}

bool ULevelingComponent::GrantRewardExperience(const int32 ExperienceAmount)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority() || ExperienceAmount <= 0)
	{
		return false;
	}

	return GrantExperienceInternal(static_cast<float>(ExperienceAmount));
}

bool ULevelingComponent::GrantKillExperience(APlayerState* VictimPlayerState)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{

		return false;
	}

	if (!IsValid(VictimPlayerState)
		|| VictimPlayerState == OwnerActor
		|| VictimPlayerState->GetWorld() != OwnerActor->GetWorld())
	{

		return false;
	}

	const URewardDefinition* RewardDefinition = GetPlayerKillRewardDefinition();
	const float KillExperienceReward = RewardDefinition
		? static_cast<float>(RewardDefinition->RollPlayerKillExperienceReward())
		: PlayerKillExperienceReward;
	if (KillExperienceReward <= 0.f)
	{
		return false;
	}

	if (!GrantExperienceInternal(KillExperienceReward))
	{
		return false;
	}

	if (const APdPlayerState* PdPlayerState = Cast<APdPlayerState>(OwnerActor))
	{
		if (UPlayerNotificationComponent* NotificationComponent = PdPlayerState->GetPlayerNotificationComponent())
		{
			NotificationComponent->SendExperienceRewardNotification(
				KillExperienceReward,
				RewardDefinition ? RewardDefinition->Notification.ExperienceIcon.Get() : nullptr);
		}
	}

	return true;
}

float ULevelingComponent::GetRequiredExperienceForNextLevel() const
{
	const UPdAbilitySystemComponent* ASC = GetPdAbilitySystemComponent();
	if (ASC)
	{
		const float AttributeMaxExperience = ASC->GetNumericAttribute(UBasicAttributeSet::GetMaxExperienceAttribute());
		if (AttributeMaxExperience > 0.f)
		{
			return AttributeMaxExperience;
		}
	}

	return RequiredExperienceForNextLevel;
}

bool ULevelingComponent::GrantExperienceInternal(const float ExperienceAmount)
{
	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor
		|| !OwnerActor->HasAuthority()
		|| !FMath::IsFinite(ExperienceAmount)
		|| ExperienceAmount <= 0.f
		|| !LevelingGameplayEffectClass
		|| !ExperienceStatTag.IsValid())
	{
		return false;
	}

	UPdAbilitySystemComponent* ASC = GetPdAbilitySystemComponent();
	if (!ASC)
	{

		return false;
	}

	const bool bApplied = ASC->ApplyStatUpEffectByTag(LevelingGameplayEffectClass, ExperienceStatTag, ExperienceAmount, EEnum_Operation::Add);

const float RequiredExperience = GetRequiredExperienceForNextLevel();
	if (!bApplied || !bAutoLevelUpWhenExperienceReached || RequiredExperience <= 0.f)
	{
		return bApplied;
	}

	ProcessAutoLevelUps();

	return true;
}

bool ULevelingComponent::ApplyLevelUpInternal()
{
	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		return false;
	}

	UPdAbilitySystemComponent* ASC = GetPdAbilitySystemComponent();
	if (!ASC || !LevelingGameplayEffectClass || !LevelStatTag.IsValid())
	{

		return false;
	}

	TMap<FGameplayTag, float> LevelUpMagnitudes;
	LevelUpMagnitudes.Add(LevelStatTag, 1.f);

	if (bResetExperienceOnLevelUp && ExperienceStatTag.IsValid())
	{
		const float RequiredExperience = GetRequiredExperienceForNextLevel();
		if (RequiredExperience <= 0.f)
		{

			return false;
		}

		float CurrentExperience = 0.f;
		if (!GetCurrentAttributeValue(ExperienceStatTag, CurrentExperience))
		{

			return false;
		}

		if (CurrentExperience < RequiredExperience)
		{

			return false;
		}

		LevelUpMagnitudes.Add(ExperienceStatTag, -RequiredExperience);
	}

	const float PointGrant = FMath::Max(PointsPerCategoryOnLevelUp, 0.f);
	if (PointGrant > 0.f)
	{
		for (const FGameplayTag& CategoryPointTag : CategoryPointStatTags)
		{
			if (CategoryPointTag.IsValid())
			{
				LevelUpMagnitudes.FindOrAdd(CategoryPointTag) += PointGrant;
			}
		}
	}

	return ASC->ApplyStatUpEffectByTags(LevelingGameplayEffectClass, LevelUpMagnitudes, EEnum_Operation::Add);
}

bool ULevelingComponent::ProcessAutoLevelUps()
{
	if (!bAutoLevelUpWhenExperienceReached || !ExperienceStatTag.IsValid())
	{
		return false;
	}

	bool bLeveledUp = false;
	constexpr int32 MaxLevelUpsPerGrant = 50;

	for (int32 LevelUpCount = 0; LevelUpCount < MaxLevelUpsPerGrant; ++LevelUpCount)
	{
		const float RequiredExperience = GetRequiredExperienceForNextLevel();
		if (RequiredExperience <= 0.f)
		{
			break;
		}

		float CurrentExperience = 0.f;
		if (!GetCurrentAttributeValue(ExperienceStatTag, CurrentExperience) || CurrentExperience < RequiredExperience)
		{
			break;
		}

		if (!ApplyLevelUpInternal())
		{
			break;
		}

		bLeveledUp = true;
		if (!bResetExperienceOnLevelUp)
		{
			break;
		}
	}

	if (bLeveledUp)
	{
		float RemainingExperience = 0.f;
		GetCurrentAttributeValue(ExperienceStatTag, RemainingExperience);

	}

	return bLeveledUp;
}

UPdAbilitySystemComponent* ULevelingComponent::GetPdAbilitySystemComponent() const
{
	const APdPlayerState* PlayerState = Cast<APdPlayerState>(GetOwner());
	return PlayerState ? PlayerState->GetPdAbilitySystemComponent() : nullptr;
}

bool ULevelingComponent::GetCurrentAttributeValue(const FGameplayTag StatTag, float& OutValue) const
{
	OutValue = 0.f;

	const UPdAbilitySystemComponent* ASC = GetPdAbilitySystemComponent();
	if (!ASC || !StatTag.IsValid())
	{
		return false;
	}

	FGameplayAttribute Attribute;
	if (!ASC->ResolveAttributeFromTag(StatTag, Attribute))
	{

		return false;
	}

	OutValue = ASC->GetNumericAttribute(Attribute);
	return true;
}

const URewardDefinition* ULevelingComponent::GetPlayerKillRewardDefinition() const
{
	if (PlayerKillRewardDefinition.IsNull())
	{
		return nullptr;
	}

	return LoadedPlayerKillRewardDefinition
		? LoadedPlayerKillRewardDefinition.Get()
		: PlayerKillRewardDefinition.Get();
}

void ULevelingComponent::BeginPlayerKillRewardPreload()
{
	ReleasePlayerKillRewardPreload();
	LoadedPlayerKillRewardDefinition = PlayerKillRewardDefinition.Get();
	if (LoadedPlayerKillRewardDefinition || PlayerKillRewardDefinition.IsNull())
	{
		return;
	}

	PlayerKillRewardPreloadHandle =
		UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(
			PlayerKillRewardDefinition.ToSoftObjectPath(),
			FStreamableDelegate::CreateUObject(
				this,
				&ThisClass::HandlePlayerKillRewardPreloadComplete));
}

void ULevelingComponent::HandlePlayerKillRewardPreloadComplete()
{
	LoadedPlayerKillRewardDefinition = PlayerKillRewardDefinition.Get();
}

void ULevelingComponent::ReleasePlayerKillRewardPreload()
{
	if (PlayerKillRewardPreloadHandle.IsValid())
	{
		PlayerKillRewardPreloadHandle->CancelHandle();
		PlayerKillRewardPreloadHandle->ReleaseHandle();
		PlayerKillRewardPreloadHandle.Reset();
	}
	LoadedPlayerKillRewardDefinition = nullptr;
}
