#include "GameFeature/GameFeatureAction_AddAttributes.h"

#include "AbilitySystemGlobals.h"
#include "AssetRegistry/AssetBundleData.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFeature/ActorExtensionWorldSubsystem.h"
#include "GameFeaturesSubsystemSettings.h"
#include "GameFramework/PlayerState.h"
#include "Mode/ExperienceGameMode.h"
#include "Component/Player/StatUpgradeComponent.h"
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
void UGameFeatureAction_AddAttributes::PostLoad()
{
	Super::PostLoad();

	if (TargetClasses.IsEmpty() && !TargetClass.IsNull())
	{
		TargetClasses.Add(TargetClass);
	}
}

void UGameFeatureAction_AddAttributes::OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context)
{
	Super::OnGameFeatureDeactivating(Context);

	const FGameFeatureStateChangeContext ChangeContext(Context);
	FGameFeatureAttributeHandles* Handles = ContextHandles.Find(ChangeContext);
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

	if (TargetClasses.IsEmpty() && TargetClass.IsNull())
	{
		Result = EDataValidationResult::Invalid;
		Context.AddError(NSLOCTEXT("PdGameFeatureAction_AddAttributes", "MissingTargetClass", "At least one TargetClasses entry is required."));
	}

	for (int32 EntryIndex = 0; EntryIndex < TargetClasses.Num(); ++EntryIndex)
	{
		if (TargetClasses[EntryIndex].IsNull())
		{
			Result = EDataValidationResult::Invalid;
			Context.AddError(FText::Format(
				NSLOCTEXT("PdGameFeatureAction_AddAttributes", "MissingTargetClassEntry", "TargetClasses entry {0} has no class."),
				FText::AsNumber(EntryIndex)));
		}
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
		const FAttributeTagMapping& Entry = AttributeConfig.AttributeMappings[EntryIndex];
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
	const TArray<TSoftClassPtr<AActor>>* SourceTargetClasses = &TargetClasses;
	TArray<TSoftClassPtr<AActor>> DeprecatedTargetClasses;
	if (SourceTargetClasses->IsEmpty() && !TargetClass.IsNull())
	{
		DeprecatedTargetClasses.Add(TargetClass);
		SourceTargetClasses = &DeprecatedTargetClasses;
	}

	for (const TSoftClassPtr<AActor>& TargetClassPtr : *SourceTargetClasses)
	{
		AddBundleClass(AssetBundleData, UGameFeaturesSubsystemSettings::LoadStateClient, TargetClassPtr);
		AddBundleClass(AssetBundleData, UGameFeaturesSubsystemSettings::LoadStateServer, TargetClassPtr);
	}

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

	UActorExtensionWorldSubsystem* ExtensionSubsystem = World->GetSubsystem<UActorExtensionWorldSubsystem>();
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

	TArray<TSubclassOf<AActor>> LoadedTargetClasses;
	CollectTargetClasses(LoadedTargetClasses);
	if (LoadedTargetClasses.IsEmpty())
	{
		UE_LOG(PdGameFeatureAction_AddAttributesLog, Error, TEXT("AddAttributes skipped: failed to load any target class."));
		return;
	}

	FGameFeatureAttributeHandles& Handles = ContextHandles.FindOrAdd(ChangeContext);

	for (const TSubclassOf<AActor>& LoadedTargetClass : LoadedTargetClasses)
	{
		if (!LoadedTargetClass)
		{
			continue;
		}

		FActorExtensionSpec ExtensionSpec;
		ExtensionSpec.CanActivate = FPdActorExtensionCanActivate::CreateWeakLambda(this, [this](AActor* Actor)
		{
			UPdAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponent(Actor);
			return AbilitySystemComponent && AbilitySystemComponent->IsRegistered()
				&& AbilitySystemComponent->HasAbilityActorInfoAllocated();
		});
		ExtensionSpec.OnActivate = FPdActorExtensionExecute::CreateWeakLambda(this, [this, ChangeContext](AActor* Actor)
		{
			if (FGameFeatureAttributeHandles* FoundHandles = ContextHandles.Find(ChangeContext))
			{
				AddAttributesToActor(Actor, *FoundHandles);
			}
		});
		ExtensionSpec.OnDeactivate = FPdActorExtensionExecute::CreateWeakLambda(this, [this, ChangeContext](AActor* Actor)
		{
			if (FGameFeatureAttributeHandles* FoundHandles = ContextHandles.Find(ChangeContext))
			{
				RemoveAttributesFromActor(Actor, *FoundHandles);
			}
		});

		if (TSharedPtr<FActorExtensionHandle> ExtensionHandle = ExtensionSubsystem->RegisterExtensionForClass(LoadedTargetClass, MoveTemp(ExtensionSpec)))
		{
			Handles.ExtensionRequestHandles.Add(ExtensionHandle);
		}
	}
}

//----------------------------------------------------------------------------------------------------------------------
//--- Attribute Setup
void UGameFeatureAction_AddAttributes::AddAttributesToActor(AActor* Actor, FGameFeatureAttributeHandles& Handles)
{
	if (!Actor || Handles.AttributeConfigHandles.Contains(Actor))
	{
		return;
	}

	UPdAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponent(Actor);
	if (!AbilitySystemComponent)
	{
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
			if (APlayerState* PlayerState = Cast<APlayerState>(Actor))
			{
				if (UStatUpgradeComponent* StatUpgradeComponent = PlayerState->FindComponentByClass<UStatUpgradeComponent>())
				{
					StatUpgradeComponent->ApplyConfiguredAttributeDefaults();
				}

				if (UWorld* World = Actor->GetWorld())
				{
					if (AExperienceGameMode* ExperienceGameMode = World->GetAuthGameMode<AExperienceGameMode>())
					{
						ExperienceGameMode->ApplyConfiguredStatusPointsForPlayerState(PlayerState);
					}
				}
			}
		}
	}
}

void UGameFeatureAction_AddAttributes::RemoveAttributesFromActor(AActor* Actor, FGameFeatureAttributeHandles& Handles) const
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

void UGameFeatureAction_AddAttributes::RemoveAllAttributes(FGameFeatureAttributeHandles& Handles) const
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
	FGameFeatureAttributeHandles& Handles) const
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
		if (!AttributeSetClass)
		{
			continue;
		}

		if (AbilitySystemComponent->GetAttributeSet(AttributeSetClass))
		{
			continue;
		}

		UAttributeSet* AttributeSet = FindExistingAttributeSet(Actor, AttributeSetClass);
		const bool bCreatedByFeature = AttributeSet == nullptr;
		if (!AttributeSet)
		{
			AttributeSet = NewObject<UAttributeSet>(AbilitySystemComponent->GetOwner(), AttributeSetClass);
		}

		if (!AttributeSet)
		{
			continue;
		}

		AbilitySystemComponent->AddSpawnedAttribute(AttributeSet);
		// PlayerState가 소유한 기본 속성은 피처 해제 시에도 유지한다.
		if (bCreatedByFeature)
		{
			ActorAttributeSets.Add(AttributeSet);
		}
	}

	if (ActorAttributeSets.IsEmpty())
	{
		Handles.AttributeSets.Remove(Actor);
	}
}

void UGameFeatureAction_AddAttributes::CollectTargetClasses(TArray<TSubclassOf<AActor>>& OutTargetClasses) const
{
	OutTargetClasses.Reset();

	const TArray<TSoftClassPtr<AActor>>* SourceTargetClasses = &TargetClasses;
	TArray<TSoftClassPtr<AActor>> DeprecatedTargetClasses;
	if (SourceTargetClasses->IsEmpty() && !TargetClass.IsNull())
	{
		DeprecatedTargetClasses.Add(TargetClass);
		SourceTargetClasses = &DeprecatedTargetClasses;
	}

	for (const TSoftClassPtr<AActor>& TargetClassPtr : *SourceTargetClasses)
	{
		if (TSubclassOf<AActor> LoadedTargetClass = TargetClassPtr.Get())
		{
			OutTargetClasses.AddUnique(LoadedTargetClass);
		}
	}
}

void UGameFeatureAction_AddAttributes::CollectAttributeSetClasses(TArray<TSubclassOf<UAttributeSet>>& OutAttributeSetClasses) const
{
	for (const TSoftClassPtr<UAttributeSet>& AttributeSetClassPtr : AttributeSetClasses)
	{
		if (TSubclassOf<UAttributeSet> AttributeSetClass = AttributeSetClassPtr.Get())
		{
			OutAttributeSetClasses.AddUnique(AttributeSetClass);
		}
	}

	for (const FAttributeTagMapping& Entry : AttributeConfig.AttributeMappings)
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
