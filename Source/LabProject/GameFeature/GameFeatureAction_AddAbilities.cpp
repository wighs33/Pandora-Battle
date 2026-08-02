#include "GameFeature/GameFeatureAction_AddAbilities.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Ability/PdGameplayAbility.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/GameplayAbility.h"
#include "AssetRegistry/AssetBundleData.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFeature/ActorExtensionWorldSubsystem.h"
#include "GameFeaturesSubsystemSettings.h"
#include "TimerManager.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameFeatureAction_AddAbilities)

DEFINE_LOG_CATEGORY(PdGameFeatureAction_AddAbilitiesLog);

namespace
{
	void TryActivateGameFeatureGrantedAbilityNextTick(UAbilitySystemComponent* AbilitySystemComponent, FGameplayAbilitySpecHandle AbilityHandle)
	{
		if (!AbilitySystemComponent || !AbilityHandle.IsValid())
		{
			return;
		}

		if (UWorld* World = AbilitySystemComponent->GetWorld())
		{
			TWeakObjectPtr<UAbilitySystemComponent> WeakASC = AbilitySystemComponent;
			World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([WeakASC, AbilityHandle]()
			{
				if (UAbilitySystemComponent* ASC = WeakASC.Get())
				{
					ASC->TryActivateAbility(AbilityHandle);
				}
			}));
			return;
		}

		AbilitySystemComponent->TryActivateAbility(AbilityHandle);
	}
}

UGameFeatureAction_AddAbilities::UGameFeatureAction_AddAbilities()
{
}

//----------------------------------------------------------------------------------------------------------------------
//--- Game Feature Events
void UGameFeatureAction_AddAbilities::OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context)
{
	Super::OnGameFeatureDeactivating(Context);

	const FGameFeatureStateChangeContext ChangeContext(Context);
	FPdGameFeatureAbilityGrantHandles* Handles = ContextHandles.Find(ChangeContext);
	if (!Handles)
	{
		return;
	}

	RemoveAllGrantedAbilities(*Handles);
	Handles->ExtensionRequestHandles.Reset();
	ContextHandles.Remove(ChangeContext);
}

#if WITH_EDITOR
EDataValidationResult UGameFeatureAction_AddAbilities::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);

	if (TargetClass.IsNull())
	{
		Result = EDataValidationResult::Invalid;
		Context.AddError(NSLOCTEXT("PdGameFeatureAction_AddAbilities", "MissingTargetClass", "TargetClass is required."));
	}

	if (!bClientAction && !bServerAction)
	{
		Result = EDataValidationResult::Invalid;
		Context.AddError(NSLOCTEXT("PdGameFeatureAction_AddAbilities", "MissingNetwork", "At least one Network option is required."));
	}

	if (Abilities.IsEmpty())
	{
		Result = EDataValidationResult::Invalid;
		Context.AddError(NSLOCTEXT("PdGameFeatureAction_AddAbilities", "EmptyAbilities", "At least one ability entry is required."));
	}

	TSet<FSoftObjectPath> AbilityPaths;
	for (int32 EntryIndex = 0; EntryIndex < Abilities.Num(); ++EntryIndex)
	{
		if (Abilities[EntryIndex].Ability.IsNull())
		{
			Result = EDataValidationResult::Invalid;
			Context.AddError(FText::Format(
				NSLOCTEXT("PdGameFeatureAction_AddAbilities", "MissingAbility", "Ability entry {0} has no Ability."),
				FText::AsNumber(EntryIndex)));
			continue;
		}

		if (Abilities[EntryIndex].Level < 1)
		{
			Result = EDataValidationResult::Invalid;
			Context.AddError(FText::Format(
				NSLOCTEXT("PdGameFeatureAction_AddAbilities", "InvalidAbilityLevel", "Ability entry {0} must have Level >= 1."),
				FText::AsNumber(EntryIndex)));
		}

		const FGameplayTag& InputTag = Abilities[EntryIndex].InputTag;
		if (InputTag.IsValid()
			&& !InputTag.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Input.Ability"))))
		{
			Result = EDataValidationResult::Invalid;
			Context.AddError(FText::Format(
				NSLOCTEXT("PdGameFeatureAction_AddAbilities", "InvalidInputTag", "Ability entry {0} InputTag must be under Input.Ability."),
				FText::AsNumber(EntryIndex)));
		}

		const FSoftObjectPath AbilityPath = Abilities[EntryIndex].Ability.ToSoftObjectPath();
		if (AbilityPaths.Contains(AbilityPath))
		{
			Context.AddWarning(FText::Format(
				NSLOCTEXT("PdGameFeatureAction_AddAbilities", "DuplicateAbility", "Ability entry {0} duplicates '{1}'. It will be granted only once at runtime."),
				FText::AsNumber(EntryIndex),
				FText::FromString(AbilityPath.ToString())));
			continue;
		}

		AbilityPaths.Add(AbilityPath);
	}

	return Result;
}
#endif

#if WITH_EDITORONLY_DATA
void UGameFeatureAction_AddAbilities::AddAdditionalAssetBundleData(FAssetBundleData& AssetBundleData)
{
	if (!TargetClass.IsNull())
	{
		if (bClientAction)
		{
			AssetBundleData.AddBundleAsset(
				UGameFeaturesSubsystemSettings::LoadStateClient,
				TargetClass.ToSoftObjectPath().GetAssetPath());
		}

		if (bServerAction)
		{
			AssetBundleData.AddBundleAsset(
				UGameFeaturesSubsystemSettings::LoadStateServer,
				TargetClass.ToSoftObjectPath().GetAssetPath());
		}
	}

	for (const FPdGameFeatureAbilityEntry& Entry : Abilities)
	{
		if (!Entry.Ability.IsNull())
		{
			AssetBundleData.AddBundleAsset(
				UGameFeaturesSubsystemSettings::LoadStateServer,
				Entry.Ability.ToSoftObjectPath().GetAssetPath());
		}
	}
}
#endif

//----------------------------------------------------------------------------------------------------------------------
//--- Activation
void UGameFeatureAction_AddAbilities::AddToWorld(const FWorldContext& WorldContext,
	const FGameFeatureStateChangeContext& ChangeContext)
{
	UWorld* World = WorldContext.World();
	if (!World || !World->IsGameWorld() || TargetClass.IsNull())
	{
		return;
	}

	RegisterAbilityExtension(World, ChangeContext);
}

void UGameFeatureAction_AddAbilities::RegisterAbilityExtension(
	UWorld* World,
	FGameFeatureStateChangeContext ChangeContext)
{
	if (!World || TargetClass.IsNull())
	{
		return;
	}

	UActorExtensionWorldSubsystem* ExtensionSubsystem = World->GetSubsystem<UActorExtensionWorldSubsystem>();
	if (!ExtensionSubsystem)
	{
		TWeakObjectPtr<UWorld> WeakWorld = World;
		TWeakObjectPtr<ThisClass> WeakThis = this;
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([WeakThis, WeakWorld, ChangeContext]()
		{
			if (ThisClass* This = WeakThis.Get())
			{
				This->RegisterAbilityExtension(WeakWorld.Get(), ChangeContext);
			}
		}));
		return;
	}

	TSubclassOf<AActor> LoadedTargetClass = TargetClass.Get();
	if (!LoadedTargetClass)
	{
		UE_LOG(PdGameFeatureAction_AddAbilitiesLog, Error, TEXT("AddAbilities skipped '%s': failed to load target class."),
			*TargetClass.ToString());
		return;
	}

	FPdGameFeatureAbilityGrantHandles& Handles = ContextHandles.FindOrAdd(ChangeContext);

	FPdActorExtensionSpec ExtensionSpec;
	ExtensionSpec.CanActivate = FPdActorExtensionCanActivate::CreateWeakLambda(this, [this](AActor* Actor)
	{
		return Actor && Actor->HasAuthority() && GetAbilitySystemComponent(Actor) != nullptr;
	});
	ExtensionSpec.OnActivate = FPdActorExtensionExecute::CreateWeakLambda(this, [this, ChangeContext](AActor* Actor)
	{
		if (FPdGameFeatureAbilityGrantHandles* FoundHandles = ContextHandles.Find(ChangeContext))
		{
			GrantAbilitiesToActor(Actor, *FoundHandles);
		}
	});
	ExtensionSpec.OnDeactivate = FPdActorExtensionExecute::CreateWeakLambda(this, [this, ChangeContext](AActor* Actor)
	{
		if (FPdGameFeatureAbilityGrantHandles* FoundHandles = ContextHandles.Find(ChangeContext))
		{
			RemoveAbilitiesFromActor(Actor, *FoundHandles);
		}
	});

	if (TSharedPtr<FActorExtensionHandle> ExtensionHandle = ExtensionSubsystem->RegisterExtensionForClass(LoadedTargetClass, MoveTemp(ExtensionSpec)))
	{
		Handles.ExtensionRequestHandles.Add(ExtensionHandle);
	}
}

//----------------------------------------------------------------------------------------------------------------------
//--- Ability Grants
void UGameFeatureAction_AddAbilities::GrantAbilitiesToActor(AActor* Actor, FPdGameFeatureAbilityGrantHandles& Handles)
{
	if (!Actor || !Actor->HasAuthority() || Handles.AbilitySpecHandles.Contains(Actor))
	{
		return;
	}

	UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponent(Actor);
	if (!AbilitySystemComponent)
	{
		return;
	}

	TArray<FGameplayAbilitySpecHandle>& ActorHandles = Handles.AbilitySpecHandles.Add(Actor);
	for (const FPdGameFeatureAbilityEntry& Entry : Abilities)
	{
		TSubclassOf<UGameplayAbility> AbilityClass = Entry.Ability.Get();
		if (!AbilityClass)
		{
			continue;
		}

		if (HasAbilityClass(AbilitySystemComponent, AbilityClass))
		{
			continue;
		}

		const UPdGameplayAbility* AbilityCDO = Cast<UPdGameplayAbility>(AbilityClass->GetDefaultObject());
		const FGameplayTag EffectiveInputTag = Entry.InputTag.IsValid()
			? Entry.InputTag
			: AbilityCDO
				? AbilityCDO->GetDefaultInputTag()
				: FGameplayTag();

		const int32 SafeLevel = FMath::Max(1, Entry.Level);
		FGameplayAbilitySpec AbilitySpec(AbilityClass, SafeLevel, INDEX_NONE, Actor);
		if (EffectiveInputTag.IsValid())
		{
			AbilitySpec.GetDynamicSpecSourceTags().AddTag(EffectiveInputTag);
		}
		const bool bAutoActivateWhenGranted = AbilityCDO && AbilityCDO->ShouldAutoActivateWhenGranted();
		const FGameplayAbilitySpecHandle GrantedHandle = AbilitySystemComponent->GiveAbility(AbilitySpec);
		if (GrantedHandle.IsValid())
		{
			ActorHandles.Add(GrantedHandle);
			if (bAutoActivateWhenGranted)
			{
				TryActivateGameFeatureGrantedAbilityNextTick(AbilitySystemComponent, GrantedHandle);
			}
		}
	}

	if (ActorHandles.IsEmpty())
	{
		Handles.AbilitySpecHandles.Remove(Actor);
	}
}

void UGameFeatureAction_AddAbilities::RemoveAbilitiesFromActor(AActor* Actor, FPdGameFeatureAbilityGrantHandles& Handles) const
{
	if (!Actor)
	{
		return;
	}

	TArray<FGameplayAbilitySpecHandle> ActorHandles;
	if (!Handles.AbilitySpecHandles.RemoveAndCopyValue(Actor, ActorHandles))
	{
		return;
	}

	UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponent(Actor);
	if (!AbilitySystemComponent)
	{
		return;
	}

	for (const FGameplayAbilitySpecHandle& AbilityHandle : ActorHandles)
	{
		if (AbilityHandle.IsValid())
		{
			AbilitySystemComponent->ClearAbility(AbilityHandle);
		}
	}
}

void UGameFeatureAction_AddAbilities::RemoveAllGrantedAbilities(FPdGameFeatureAbilityGrantHandles& Handles) const
{
	TArray<TWeakObjectPtr<AActor>> Actors;
	Handles.AbilitySpecHandles.GetKeys(Actors);

	for (const TWeakObjectPtr<AActor>& Actor : Actors)
	{
		RemoveAbilitiesFromActor(Actor.Get(), Handles);
	}

	Handles.AbilitySpecHandles.Reset();
}

UAbilitySystemComponent* UGameFeatureAction_AddAbilities::GetAbilitySystemComponent(AActor* Actor) const
{
	return Actor ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor) : nullptr;
}

bool UGameFeatureAction_AddAbilities::HasAbilityClass(const UAbilitySystemComponent* AbilitySystemComponent, TSubclassOf<UGameplayAbility> AbilityClass) const
{
	if (!AbilitySystemComponent || !AbilityClass)
	{
		return false;
	}

	for (const FGameplayAbilitySpec& AbilitySpec : AbilitySystemComponent->GetActivatableAbilities())
	{
		if (AbilitySpec.Ability && AbilitySpec.Ability->GetClass() == AbilityClass)
		{
			return true;
		}
	}

	return false;
}
