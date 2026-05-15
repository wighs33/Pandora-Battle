#include "PlayerComponent/StatUpgradeComponent.h"

#include "AbilitySystem/PdAbilitySystemComponent.h"
#include "GameFramework/Actor.h"
#include "GameplayEffect.h"
#include "Mode/PdPlayerState.h"
#include "PlayerComponent/StatUpgradeDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StatUpgradeComponent)

DEFINE_LOG_CATEGORY(StatUpgradeComponentLog);

UStatUpgradeComponent::UStatUpgradeComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);
}

bool UStatUpgradeComponent::RequestStatUp(FGameplayTag StatTag)
{
	AActor* OwnerActor = GetOwner();
	UE_LOG(StatUpgradeComponentLog, Log, TEXT("[StatUpgrade] RequestStatUp: component=%s owner=%s authority=%s tag=%s valid=%s"),
		*GetNameSafe(this),
		*GetNameSafe(OwnerActor),
		OwnerActor && OwnerActor->HasAuthority() ? TEXT("true") : TEXT("false"),
		*StatTag.ToString(),
		StatTag.IsValid() ? TEXT("true") : TEXT("false"));

	if (!OwnerActor || !StatTag.IsValid())
	{
		UE_LOG(StatUpgradeComponentLog, Warning, TEXT("[StatUpgrade] RequestStatUp rejected: owner=%s tag=%s"),
			*GetNameSafe(OwnerActor),
			*StatTag.ToString());
		return false;
	}

	if (!OwnerActor->HasAuthority())
	{
		UE_LOG(StatUpgradeComponentLog, Log, TEXT("[StatUpgrade] RequestStatUp sending server RPC: owner=%s tag=%s"),
			*GetNameSafe(OwnerActor),
			*StatTag.ToString());
		ServerRequestStatUp(StatTag);
		return true;
	}

	return ApplyStatUpInternal(StatTag);
}

void UStatUpgradeComponent::ServerRequestStatUp_Implementation(FGameplayTag StatTag)
{
	UE_LOG(StatUpgradeComponentLog, Log, TEXT("[StatUpgrade] ServerRequestStatUp_Implementation: component=%s owner=%s tag=%s"),
		*GetNameSafe(this),
		*GetNameSafe(GetOwner()),
		*StatTag.ToString());
	ApplyStatUpInternal(StatTag);
}

bool UStatUpgradeComponent::ApplyStatUpInternal(FGameplayTag StatTag)
{
	AActor* OwnerActor = GetOwner();
	UE_LOG(StatUpgradeComponentLog, Log, TEXT("[StatUpgrade] ApplyStatUpInternal started: component=%s owner=%s authority=%s tag=%s"),
		*GetNameSafe(this),
		*GetNameSafe(OwnerActor),
		OwnerActor && OwnerActor->HasAuthority() ? TEXT("true") : TEXT("false"),
		*StatTag.ToString());

	if (!OwnerActor || !OwnerActor->HasAuthority() || !StatTag.IsValid())
	{
		UE_LOG(StatUpgradeComponentLog, Warning, TEXT("[StatUpgrade] ApplyStatUpInternal rejected: owner=%s authority=%s tag=%s valid=%s"),
			*GetNameSafe(OwnerActor),
			OwnerActor && OwnerActor->HasAuthority() ? TEXT("true") : TEXT("false"),
			*StatTag.ToString(),
			StatTag.IsValid() ? TEXT("true") : TEXT("false"));
		return false;
	}

	UStatUpgradeDefinition* LoadedDefinition = LoadStatUpgradeDefinition();
	UE_LOG(StatUpgradeComponentLog, Log, TEXT("[StatUpgrade] Loaded definition: component=%s definition=%s soft=%s"),
		*GetNameSafe(this),
		*GetNameSafe(LoadedDefinition),
		*StatUpgradeDefinition.ToString());

	TSubclassOf<UGameplayEffect> StatUpGameplayEffectClass = LoadedDefinition ? LoadedDefinition->GetStatUpGameplayEffectClass() : nullptr;
	if (!StatUpGameplayEffectClass)
	{
		UE_LOG(StatUpgradeComponentLog, Warning, TEXT("[StatUpgrade] RequestStatUp failed: missing StatUpGameplayEffectClass."));
		return false;
	}

	float Magnitude = 0.f;
	EEnum_Operation Operation = EEnum_Operation::Add;
	if (!ResolveStatUpButtonSettings(StatTag, Magnitude, Operation))
	{
		UE_LOG(StatUpgradeComponentLog, Warning, TEXT("[StatUpgrade] RequestStatUp skipped unsupported stat tag '%s'."), *StatTag.ToString());
		return false;
	}
	UE_LOG(StatUpgradeComponentLog, Log, TEXT("[StatUpgrade] Stat rule resolved: tag=%s magnitude=%.3f operation=%d effect=%s"),
		*StatTag.ToString(),
		Magnitude,
		static_cast<int32>(Operation),
		*GetNameSafe(StatUpGameplayEffectClass.Get()));

	if (!ApplyStatUpEffectByTag(StatUpGameplayEffectClass, StatTag, Magnitude, Operation))
	{
		UE_LOG(StatUpgradeComponentLog, Warning, TEXT("[StatUpgrade] RequestStatUp failed to apply StatUp effect for tag '%s'."), *StatTag.ToString());
		return false;
	}
	UE_LOG(StatUpgradeComponentLog, Log, TEXT("[StatUpgrade] Stat up primary effect applied: tag=%s magnitude=%.3f operation=%d"),
		*StatTag.ToString(),
		Magnitude,
		static_cast<int32>(Operation));

	FGameplayTag PairedCurrentResourceStatTag;
	if (ResolvePairedCurrentResourceStatTag(StatTag, PairedCurrentResourceStatTag)
		&& !ApplyStatUpEffectByTag(StatUpGameplayEffectClass, PairedCurrentResourceStatTag, Magnitude, Operation))
	{
		UE_LOG(
			StatUpgradeComponentLog,
			Warning,
			TEXT("[StatUpgrade] RequestStatUp failed to apply paired resource StatUp effect for tag '%s'."),
			*PairedCurrentResourceStatTag.ToString());
	}
	else if (PairedCurrentResourceStatTag.IsValid())
	{
		UE_LOG(StatUpgradeComponentLog, Log, TEXT("[StatUpgrade] Paired resource effect applied: sourceTag=%s pairedTag=%s magnitude=%.3f operation=%d"),
			*StatTag.ToString(),
			*PairedCurrentResourceStatTag.ToString(),
			Magnitude,
			static_cast<int32>(Operation));
	}
	else
	{
		UE_LOG(StatUpgradeComponentLog, Log, TEXT("[StatUpgrade] No paired resource for tag=%s"), *StatTag.ToString());
	}

	return true;
}

UStatUpgradeDefinition* UStatUpgradeComponent::LoadStatUpgradeDefinition()
{
	if (LoadedStatUpgradeDefinition)
	{
		UE_LOG(StatUpgradeComponentLog, Log, TEXT("[StatUpgrade] LoadStatUpgradeDefinition using cached definition: %s"),
			*GetNameSafe(LoadedStatUpgradeDefinition));
		return LoadedStatUpgradeDefinition;
	}

	if (StatUpgradeDefinition.IsNull())
	{
		UE_LOG(StatUpgradeComponentLog, Warning, TEXT("[StatUpgrade] RequestStatUp failed: '%s' has no StatUpgradeDefinition."), *GetNameSafe(this));
		return nullptr;
	}

	UE_LOG(StatUpgradeComponentLog, Log, TEXT("[StatUpgrade] Loading StatUpgradeDefinition: component=%s soft=%s"),
		*GetNameSafe(this),
		*StatUpgradeDefinition.ToString());
	LoadedStatUpgradeDefinition = StatUpgradeDefinition.LoadSynchronous();
	if (!LoadedStatUpgradeDefinition)
	{
		UE_LOG(StatUpgradeComponentLog, Warning, TEXT("[StatUpgrade] RequestStatUp failed: '%s' failed to load StatUpgradeDefinition '%s'."),
			*GetNameSafe(this),
			*StatUpgradeDefinition.ToString());
	}

	return LoadedStatUpgradeDefinition;
}

bool UStatUpgradeComponent::ResolveStatUpButtonSettings(const FGameplayTag& StatTag, float& OutMagnitude, EEnum_Operation& OutOperation) const
{
	OutMagnitude = 0.f;
	OutOperation = EEnum_Operation::Add;

	if (!StatTag.IsValid())
	{
		UE_LOG(StatUpgradeComponentLog, Warning, TEXT("[StatUpgrade] ResolveStatUpButtonSettings failed: invalid tag."));
		return false;
	}

	const UStatUpgradeDefinition* LoadedDefinition = LoadedStatUpgradeDefinition;
	if (!LoadedDefinition || LoadedDefinition->GetUpgradeRules().IsEmpty())
	{
		UE_LOG(StatUpgradeComponentLog, Warning, TEXT("[StatUpgrade] ResolveStatUpButtonSettings failed: definition=%s ruleCount=%d"),
			*GetNameSafe(LoadedDefinition),
			LoadedDefinition ? LoadedDefinition->GetUpgradeRules().Num() : 0);
		return false;
	}

	UE_LOG(StatUpgradeComponentLog, Log, TEXT("[StatUpgrade] Resolving stat rule: tag=%s ruleCount=%d"),
		*StatTag.ToString(),
		LoadedDefinition->GetUpgradeRules().Num());

	for (const FPdStatUpgradeRule& Setting : LoadedDefinition->GetUpgradeRules())
	{
		if (!Setting.RootTag.IsValid() || FMath::IsNearlyZero(Setting.Magnitude))
		{
			UE_LOG(StatUpgradeComponentLog, Warning, TEXT("[StatUpgrade] Invalid stat rule skipped: root=%s magnitude=%.3f operation=%d"),
				*Setting.RootTag.ToString(),
				Setting.Magnitude,
				static_cast<int32>(Setting.Operation));
			continue;
		}

		if (StatTag.MatchesTag(Setting.RootTag))
		{
			OutMagnitude = Setting.Magnitude;
			OutOperation = Setting.Operation;
			return true;
		}
	}

	return false;
}

bool UStatUpgradeComponent::ResolvePairedCurrentResourceStatTag(const FGameplayTag& StatTag, FGameplayTag& OutPairedStatTag) const
{
	OutPairedStatTag = FGameplayTag();

	if (!StatTag.IsValid())
	{
		UE_LOG(StatUpgradeComponentLog, Warning, TEXT("[StatUpgrade] ResolvePairedCurrentResourceStatTag failed: invalid tag."));
		return false;
	}

	const UStatUpgradeDefinition* LoadedDefinition = LoadedStatUpgradeDefinition;
	if (!LoadedDefinition || LoadedDefinition->GetPairedResourceStatTags().IsEmpty())
	{
		return false;
	}

	for (const FPdPairedResourceStatTag& Pair : LoadedDefinition->GetPairedResourceStatTags())
	{
		if (Pair.MaxStatTag.IsValid() && Pair.CurrentStatTag.IsValid() && StatTag == Pair.MaxStatTag)
		{
			OutPairedStatTag = Pair.CurrentStatTag;
			UE_LOG(StatUpgradeComponentLog, Log, TEXT("[StatUpgrade] Paired resource resolved: maxTag=%s currentTag=%s"),
				*Pair.MaxStatTag.ToString(),
				*Pair.CurrentStatTag.ToString());
			return true;
		}
	}

	return false;
}

bool UStatUpgradeComponent::ApplyStatUpEffectByTag(TSubclassOf<UGameplayEffect> GameplayEffectClass, FGameplayTag StatTag, float Magnitude, EEnum_Operation Operation, float Level)
{
	APdPlayerState* PlayerState = Cast<APdPlayerState>(GetOwner());
	UE_LOG(StatUpgradeComponentLog, Log, TEXT("[StatUpgrade] ApplyStatUpEffectByTag requested: playerState=%s effect=%s tag=%s magnitude=%.3f operation=%d level=%.3f"),
		*GetNameSafe(PlayerState),
		*GetNameSafe(GameplayEffectClass.Get()),
		*StatTag.ToString(),
		Magnitude,
		static_cast<int32>(Operation),
		Level);

	if (!PlayerState || !GameplayEffectClass || !StatTag.IsValid())
	{
		UE_LOG(StatUpgradeComponentLog, Warning, TEXT("[StatUpgrade] ApplyStatUpEffectByTag rejected: playerState=%s effect=%s tag=%s valid=%s"),
			*GetNameSafe(PlayerState),
			*GetNameSafe(GameplayEffectClass.Get()),
			*StatTag.ToString(),
			StatTag.IsValid() ? TEXT("true") : TEXT("false"));
		return false;
	}

	UPdAbilitySystemComponent* ASC = PlayerState->GetPdAbilitySystemComponent();
	if (!ASC)
	{
		UE_LOG(StatUpgradeComponentLog, Warning, TEXT("[StatUpgrade] ApplyStatUpEffectByTag rejected: ASC is null on %s."),
			*GetNameSafe(PlayerState));
		return false;
	}

	const bool bApplied = ASC->ApplyStatUpEffectByTag(GameplayEffectClass, StatTag, Magnitude, Operation, Level);
	UE_LOG(StatUpgradeComponentLog, Log, TEXT("[StatUpgrade] ApplyStatUpEffectByTag result: asc=%s tag=%s result=%s"),
		*GetNameSafe(ASC),
		*StatTag.ToString(),
		bApplied ? TEXT("true") : TEXT("false"));
	return bApplied;
}
