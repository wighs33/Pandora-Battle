#include "GameFeature/GameFeatureAction_AddAttributes.h"

#include "AbilitySystemGlobals.h"
#include "AssetRegistry/AssetBundleData.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFeature/PdActorExtensionWorldSubsystem.h"
#include "GameFeaturesSubsystemSettings.h"
#include "TimerManager.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameFeatureAction_AddAttributes)

DEFINE_LOG_CATEGORY(PdGameFeatureAction_AddAttributesLog);

namespace
{
	template<typename AssetType>
	void AddBundleClass(FAssetBundleData& AssetBundleData, FName BundleName, const TSoftClassPtr<AssetType>& AssetClass)
	{
		if (!AssetClass.IsNull())
		{
			AssetBundleData.AddBundleAsset(BundleName, AssetClass.ToSoftObjectPath().GetAssetPath());
		}
	}
}

UGameFeatureAction_AddAttributes::UGameFeatureAction_AddAttributes()
{
}

//----------------------------------------------------------------------------------------------------------------------
//--- Game Feature Events
void UGameFeatureAction_AddAttributes::OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context)
{
	Super::OnGameFeatureDeactivating(Context);

	const FGameFeatureStateChangeContext ChangeContext(Context);
	FPdGameFeatureAttributeHandles* Handles = ContextHandles.Find(ChangeContext);
	if (!Handles)
	{
		return;
	}

	RemoveAllAttributes(*Handles);
	Handles->ExtensionRequestHandles.Reset();
	ContextHandles.Remove(ChangeContext);
}

#if WITH_EDITOR
EDataValidationResult UGameFeatureAction_AddAttributes::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);

	if (TargetClass.IsNull())
	{
		Result = EDataValidationResult::Invalid;
		Context.AddError(NSLOCTEXT("PdGameFeatureAction_AddAttributes", "MissingTargetClass", "TargetClass is required."));
	}

	if (!bClientAction && !bServerAction)
	{
		Result = EDataValidationResult::Invalid;
		Context.AddError(NSLOCTEXT("PdGameFeatureAction_AddAttributes", "MissingNetwork", "At least one Network option is required."));
	}

	if (AttributeSetClasses.IsEmpty() && !AttributeConfig.HasAnyData())
	{
		Result = EDataValidationResult::Invalid;
		Context.AddError(NSLOCTEXT("PdGameFeatureAction_AddAttributes", "EmptyAttributeAction", "At least one AttributeSetClass or AttributeConfig entry is required."));
	}

	for (int32 EntryIndex = 0; EntryIndex < AttributeSetClasses.Num(); ++EntryIndex)
	{
		if (AttributeSetClasses[EntryIndex].IsNull())
		{
			Result = EDataValidationResult::Invalid;
			Context.AddError(FText::Format(
				NSLOCTEXT("PdGameFeatureAction_AddAttributes", "MissingAttributeSet", "AttributeSetClasses entry {0} has no class."),
				FText::AsNumber(EntryIndex)));
		}
	}

	for (int32 EntryIndex = 0; EntryIndex < AttributeConfig.AttributeMappings.Num(); ++EntryIndex)
	{
		if (!AttributeConfig.AttributeMappings[EntryIndex].IsValid())
		{
			Result = EDataValidationResult::Invalid;
			Context.AddError(FText::Format(
				NSLOCTEXT("PdGameFeatureAction_AddAttributes", "InvalidMapping", "AttributeMappings entry {0} requires both StatTag and Attribute."),
				FText::AsNumber(EntryIndex)));
		}
	}

	TSet<FGameplayTag> StatTags;
	TSet<FString> AttributeNames;
	for (int32 EntryIndex = 0; EntryIndex < AttributeConfig.AttributeMappings.Num(); ++EntryIndex)
	{
		const FPdAttributeTagMapping& Entry = AttributeConfig.AttributeMappings[EntryIndex];
		if (!Entry.IsValid())
		{
			continue;
		}

		if (StatTags.Contains(Entry.StatTag))
		{
			Result = EDataValidationResult::Invalid;
			Context.AddError(FText::Format(
				NSLOCTEXT("PdGameFeatureAction_AddAttributes", "DuplicateStatTag", "AttributeMappings entry {0} duplicates StatTag '{1}'."),
				FText::AsNumber(EntryIndex),
				FText::FromString(Entry.StatTag.ToString())));
		}
		StatTags.Add(Entry.StatTag);

		const FString AttributeName = Entry.Attribute.GetName();
		if (AttributeNames.Contains(AttributeName))
		{
			Result = EDataValidationResult::Invalid;
			Context.AddError(FText::Format(
				NSLOCTEXT("PdGameFeatureAction_AddAttributes", "DuplicateAttribute", "AttributeMappings entry {0} duplicates Attribute '{1}'."),
				FText::AsNumber(EntryIndex),
				FText::FromString(AttributeName)));
		}
		AttributeNames.Add(AttributeName);
	}

	return Result;
}
#endif

#if WITH_EDITORONLY_DATA
void UGameFeatureAction_AddAttributes::AddAdditionalAssetBundleData(FAssetBundleData& AssetBundleData)
{
	for (const TSoftClassPtr<UAttributeSet>& AttributeSetClass : AttributeSetClasses)
	{
		AddBundleClass(AssetBundleData, UGameFeaturesSubsystemSettings::LoadStateClient, AttributeSetClass);
		AddBundleClass(AssetBundleData, UGameFeaturesSubsystemSettings::LoadStateServer, AttributeSetClass);
	}
}
#endif

//----------------------------------------------------------------------------------------------------------------------
//--- Activation
void UGameFeatureAction_AddAttributes::AddToWorld(const FWorldContext& WorldContext,
	const FGameFeatureStateChangeContext& ChangeContext)
{
	UWorld* World = WorldContext.World();
	if (!World || !World->IsGameWorld())
	{
		return;
	}

	RegisterAttributeExtension(World, ChangeContext);
}

void UGameFeatureAction_AddAttributes::RegisterAttributeExtension(
	UWorld* World,
	FGameFeatureStateChangeContext ChangeContext)
{
	if (!World)
	{
		return;
	}

	UPdActorExtensionWorldSubsystem* ExtensionSubsystem = World->GetSubsystem<UPdActorExtensionWorldSubsystem>();
	if (!ExtensionSubsystem)
	{
		TWeakObjectPtr<UWorld> WeakWorld = World;
		TWeakObjectPtr<ThisClass> WeakThis = this;
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([WeakThis, WeakWorld, ChangeContext]()
		{
			if (ThisClass* This = WeakThis.Get())
			{
				This->RegisterAttributeExtension(WeakWorld.Get(), ChangeContext);
			}
		}));
		return;
	}

	TSubclassOf<AActor> LoadedTargetClass = TargetClass.LoadSynchronous();
	if (!LoadedTargetClass)
	{
		UE_LOG(PdGameFeatureAction_AddAttributesLog, Error, TEXT("AddAttributes skipped '%s': failed to load target class."),
			*TargetClass.ToString());
		return;
	}

	FPdGameFeatureAttributeHandles& Handles = ContextHandles.FindOrAdd(ChangeContext);

	FPdActorExtensionSpec ExtensionSpec;
	ExtensionSpec.DebugName = GetFName();
	ExtensionSpec.CanActivate = FPdActorExtensionCanActivate::CreateWeakLambda(this, [this](AActor* Actor)
	{
		UPdAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponent(Actor);
		return AbilitySystemComponent && AbilitySystemComponent->IsRegistered()
			&& AbilitySystemComponent->HasAbilityActorInfoAllocated();
	});
	ExtensionSpec.OnActivate = FPdActorExtensionExecute::CreateWeakLambda(this, [this, ChangeContext](AActor* Actor)
	{
		if (FPdGameFeatureAttributeHandles* FoundHandles = ContextHandles.Find(ChangeContext))
		{
			AddAttributesToActor(Actor, *FoundHandles);
		}
	});
	ExtensionSpec.OnDeactivate = FPdActorExtensionExecute::CreateWeakLambda(this, [this, ChangeContext](AActor* Actor)
	{
		if (FPdGameFeatureAttributeHandles* FoundHandles = ContextHandles.Find(ChangeContext))
		{
			RemoveAttributesFromActor(Actor, *FoundHandles);
		}
	});

	if (TSharedPtr<FPdActorExtensionHandle> ExtensionHandle = ExtensionSubsystem->RegisterExtensionForClass(LoadedTargetClass, MoveTemp(ExtensionSpec)))
	{
		Handles.ExtensionRequestHandles.Add(ExtensionHandle);
	}
}

//----------------------------------------------------------------------------------------------------------------------
//--- Attribute Setup
void UGameFeatureAction_AddAttributes::AddAttributesToActor(AActor* Actor, FPdGameFeatureAttributeHandles& Handles)
{
	if (!Actor || Handles.AttributeConfigHandles.Contains(Actor))
	{
		return;
	}

	UPdAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponent(Actor);
	if (!AbilitySystemComponent)
	{
		UE_LOG(PdGameFeatureAction_AddAttributesLog, Warning, TEXT("AddAttributes skipped '%s': no PdAbilitySystemComponent."),
			*GetNameSafe(Actor));
		return;
	}

	if (Actor->HasAuthority())
	{
		AddAttributeSetsToActor(Actor, AbilitySystemComponent, Handles);
	}

	if (AttributeConfig.HasAnyData())
	{
		const int32 AttributeConfigHandle = AbilitySystemComponent->AddAttributeConfig(AttributeConfig);
		Handles.AttributeConfigHandles.Add(Actor, AttributeConfigHandle);

		if (Actor->HasAuthority())
		{
			AbilitySystemComponent->ApplyAttributeDefaultValues(AttributeConfig);
		}
	}
}

void UGameFeatureAction_AddAttributes::RemoveAttributesFromActor(AActor* Actor, FPdGameFeatureAttributeHandles& Handles) const
{
	if (!Actor)
	{
		return;
	}

	int32 AttributeConfigHandle = INDEX_NONE;
	if (Handles.AttributeConfigHandles.RemoveAndCopyValue(Actor, AttributeConfigHandle))
	{
		if (UPdAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponent(Actor))
		{
			AbilitySystemComponent->RemoveAttributeConfig(AttributeConfigHandle);
		}
	}

	TArray<TWeakObjectPtr<UAttributeSet>> AttributeSets;
	if (!Handles.AttributeSets.RemoveAndCopyValue(Actor, AttributeSets))
	{
		return;
	}

	UPdAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponent(Actor);
	if (!AbilitySystemComponent)
	{
		return;
	}

	for (const TWeakObjectPtr<UAttributeSet>& AttributeSetPtr : AttributeSets)
	{
		if (UAttributeSet* AttributeSet = AttributeSetPtr.Get())
		{
			AbilitySystemComponent->RemoveSpawnedAttribute(AttributeSet);
		}
	}
}

void UGameFeatureAction_AddAttributes::RemoveAllAttributes(FPdGameFeatureAttributeHandles& Handles) const
{
	TArray<TWeakObjectPtr<AActor>> Actors;
	Handles.AttributeConfigHandles.GetKeys(Actors);

	for (const TWeakObjectPtr<AActor>& Actor : Actors)
	{
		RemoveAttributesFromActor(Actor.Get(), Handles);
	}

	Handles.AttributeConfigHandles.Reset();
	Handles.AttributeSets.Reset();
}

void UGameFeatureAction_AddAttributes::AddAttributeSetsToActor(AActor* Actor, UPdAbilitySystemComponent* AbilitySystemComponent,
	FPdGameFeatureAttributeHandles& Handles) const
{
	if (!Actor || !AbilitySystemComponent)
	{
		return;
	}

	TArray<TWeakObjectPtr<UAttributeSet>>& ActorAttributeSets = Handles.AttributeSets.FindOrAdd(Actor);
	TArray<TSubclassOf<UAttributeSet>> RequiredAttributeSetClasses;
	CollectAttributeSetClasses(RequiredAttributeSetClasses);

	for (TSubclassOf<UAttributeSet> AttributeSetClass : RequiredAttributeSetClasses)
	{
		if (!AttributeSetClass || AbilitySystemComponent->GetAttributeSet(AttributeSetClass))
		{
			continue;
		}

		UAttributeSet* AttributeSet = FindExistingAttributeSet(Actor, AttributeSetClass);
		if (!AttributeSet)
		{
			AttributeSet = NewObject<UAttributeSet>(AbilitySystemComponent->GetOwner(), AttributeSetClass);
		}

		if (!AttributeSet)
		{
			UE_LOG(PdGameFeatureAction_AddAttributesLog, Warning, TEXT("AddAttributes failed to create AttributeSet '%s'."),
				*GetNameSafe(AttributeSetClass.Get()));
			continue;
		}

		AbilitySystemComponent->AddSpawnedAttribute(AttributeSet);
		ActorAttributeSets.Add(AttributeSet);
	}

	if (ActorAttributeSets.IsEmpty())
	{
		Handles.AttributeSets.Remove(Actor);
	}
}

void UGameFeatureAction_AddAttributes::CollectAttributeSetClasses(TArray<TSubclassOf<UAttributeSet>>& OutAttributeSetClasses) const
{
	for (const TSoftClassPtr<UAttributeSet>& AttributeSetClassPtr : AttributeSetClasses)
	{
		if (TSubclassOf<UAttributeSet> AttributeSetClass = AttributeSetClassPtr.LoadSynchronous())
		{
			OutAttributeSetClasses.AddUnique(AttributeSetClass);
		}
	}

	for (const FPdAttributeTagMapping& Entry : AttributeConfig.AttributeMappings)
	{
		if (!Entry.Attribute.IsValid())
		{
			continue;
		}

		TSubclassOf<UAttributeSet> AttributeSetClass = const_cast<UClass*>(Entry.Attribute.GetAttributeSetClass());
		if (AttributeSetClass)
		{
			OutAttributeSetClasses.AddUnique(AttributeSetClass);
		}
	}
}

UAttributeSet* UGameFeatureAction_AddAttributes::FindExistingAttributeSet(AActor* Actor, TSubclassOf<UAttributeSet> AttributeSetClass) const
{
	if (!Actor || !AttributeSetClass)
	{
		return nullptr;
	}

	TArray<UObject*> ChildObjects;
	GetObjectsWithOuter(Actor, ChildObjects, false);

	for (UObject* ChildObject : ChildObjects)
	{
		UAttributeSet* AttributeSet = Cast<UAttributeSet>(ChildObject);
		if (AttributeSet && AttributeSet->IsA(AttributeSetClass))
		{
			return AttributeSet;
		}
	}

	return nullptr;
}

UPdAbilitySystemComponent* UGameFeatureAction_AddAttributes::GetAbilitySystemComponent(AActor* Actor) const
{
	return Actor ? Cast<UPdAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor)) : nullptr;
}
