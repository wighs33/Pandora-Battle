#include "HealthBarViewModel.h"

#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "Mode/PdPlayerState.h"
#include "Component/Player/LevelingComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HealthBarViewModel)

DEFINE_LOG_CATEGORY(HealthBarViewModelLog);

const FName UHealthBarViewModel::ViewModelName = TEXT("HealthBarViewModel");

namespace HealthBarViewModel
{
	float GetAttributeValue(UAbilitySystemComponent* ASC, const FGameplayAttribute& Attribute)
	{
		bool bFound = false;
		return ASC ? ASC->GetGameplayAttributeValue(Attribute, bFound) : 0.f;
	}

	FText MakeHealthText(const float InHealth, const float InMaxHealth)
	{
		return FText::Format(
			NSLOCTEXT("HealthBarViewModel", "HealthTextFormat", "{0}/{1}"),
			FText::AsNumber(FMath::RoundToInt(InHealth)),
			FText::AsNumber(FMath::RoundToInt(InMaxHealth)));
	}

	float GetRequiredExperienceForNextLevel(UAbilitySystemComponent* ASC)
	{
		const APdPlayerState* PlayerState = ASC ? Cast<APdPlayerState>(ASC->GetOwner()) : nullptr;
		const ULevelingComponent* LevelingComponent = PlayerState ? PlayerState->GetLevelingComponent() : nullptr;
		return LevelingComponent ? LevelingComponent->GetRequiredExperienceForNextLevel() : 0.f;
	}

	FText MakeExperienceText(const float InExperience, const float InMaxExperience)
	{
		return FText::Format(
			NSLOCTEXT("HealthBarViewModel", "ExperienceTextFormat", "{0}/{1}"),
			FText::AsNumber(FMath::RoundToInt(InExperience)),
			FText::AsNumber(FMath::RoundToInt(InMaxExperience)));
	}
}

UHealthBarViewModel::UHealthBarViewModel()
{
	ResetViewData();
}

void UHealthBarViewModel::InitializeViewModel(UObject* SourceObject)
{
	// =================================================================================================================

	UAbilitySystemComponent* InASC = ResolveAbilitySystemComponent(SourceObject);
	if (!InASC)
	{
		if (IsViewModelInitialized())
		{
			UninitializeViewModel();
		}


		ResetViewData();
		return;
	}

	// =================================================================================================================
	if (ASC.Get() == InASC && IsViewModelInitialized())
	{
		UpdateAllData();
		return;
	}

	// =================================================================================================================

	if (IsViewModelInitialized())
	{
		UninitializeViewModel();
	}

	ASC = InASC;

	// =================================================================================================================
	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetLevelAttribute()).AddUObject(this, &ThisClass::OnLevelChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetExperienceAttribute()).AddUObject(this, &ThisClass::OnExperienceChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetMaxExperienceAttribute()).AddUObject(this, &ThisClass::OnMaxExperienceChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetHealthAttribute()).AddUObject(this, &ThisClass::OnHealthChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetMaxHealthAttribute()).AddUObject(this, &ThisClass::OnMaxHealthChanged);

	// =================================================================================================================

	UpdateAllData();

	Super::InitializeViewModel(SourceObject);
}

void UHealthBarViewModel::UninitializeViewModel()
{
	// =================================================================================================================

	if (UAbilitySystemComponent* ASCPtr = ASC.Get())
	{
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetLevelAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetExperienceAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetMaxExperienceAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetHealthAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetMaxHealthAttribute()).RemoveAll(this);
	}

	// =================================================================================================================
	ASC.Reset();
	ResetViewData();

	Super::UninitializeViewModel();
}

UAbilitySystemComponent* UHealthBarViewModel::ResolveAbilitySystemComponent(UObject* SourceObject) const
{
	// =================================================================================================================
	if (UAbilitySystemComponent* InASC = Cast<UAbilitySystemComponent>(SourceObject))
	{
		return InASC;
	}

	// =================================================================================================================

	if (const IAbilitySystemInterface* AbilitySystemInterface = Cast<IAbilitySystemInterface>(SourceObject))
	{
		return AbilitySystemInterface->GetAbilitySystemComponent();
	}

	// =================================================================================================================

	if (const UActorComponent* ActorComponent = Cast<UActorComponent>(SourceObject))
	{
		if (const AActor* OwnerActor = ActorComponent->GetOwner())
		{
			if (const IAbilitySystemInterface* AbilitySystemInterface = Cast<IAbilitySystemInterface>(OwnerActor))
			{
				return AbilitySystemInterface->GetAbilitySystemComponent();
			}
		}
	}

	return nullptr;
}

void UHealthBarViewModel::ResetViewData()
{
	UE_MVVM_SET_PROPERTY_VALUE(Health, 0.f);
	UE_MVVM_SET_PROPERTY_VALUE(MaxHealth, 0.f);
	UE_MVVM_SET_PROPERTY_VALUE(HealthPercent, 0.f);
	UE_MVVM_SET_PROPERTY_VALUE(HealthText, FText::GetEmpty());
	UE_MVVM_SET_PROPERTY_VALUE(bIsAlive, false);
	UE_MVVM_SET_PROPERTY_VALUE(CurrentExperience, 0.f);
	UE_MVVM_SET_PROPERTY_VALUE(MaxExperience, 0.f);
	UE_MVVM_SET_PROPERTY_VALUE(ExperiencePercent, 0.f);
	UE_MVVM_SET_PROPERTY_VALUE(ExperienceText, FText::GetEmpty());
}

void UHealthBarViewModel::UpdateHealthData()
{
	// =================================================================================================================

	UAbilitySystemComponent* ASCPtr = ASC.Get();
	if (!ASCPtr)
	{
		ResetViewData();
		return;
	}

	// =================================================================================================================

	const float CurrentHealth = HealthBarViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetHealthAttribute());
	const float CurrentMaxHealth = HealthBarViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetMaxHealthAttribute());
	const float CurrentHealthPercent = CurrentMaxHealth > 0.f ? FMath::Clamp(CurrentHealth / CurrentMaxHealth, 0.f, 1.f) : 0.f;

	// =================================================================================================================

	UE_MVVM_SET_PROPERTY_VALUE(Health, CurrentHealth);
	UE_MVVM_SET_PROPERTY_VALUE(MaxHealth, CurrentMaxHealth);
	UE_MVVM_SET_PROPERTY_VALUE(HealthPercent, CurrentHealthPercent);
	UE_MVVM_SET_PROPERTY_VALUE(HealthText, HealthBarViewModel::MakeHealthText(CurrentHealth, CurrentMaxHealth));
	UE_MVVM_SET_PROPERTY_VALUE(bIsAlive, CurrentHealth > 0.f);
}

void UHealthBarViewModel::UpdateExperienceData()
{
	UAbilitySystemComponent* ASCPtr = ASC.Get();
	if (!ASCPtr)
	{
		ResetViewData();
		return;
	}

	const float NewCurrentExperience = HealthBarViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetExperienceAttribute());
	const float NewMaxExperience = HealthBarViewModel::GetRequiredExperienceForNextLevel(ASCPtr);
	const float NewExperiencePercent = NewMaxExperience > 0.f
		? FMath::Clamp(NewCurrentExperience / NewMaxExperience, 0.f, 1.f)
		: 0.f;

	UE_MVVM_SET_PROPERTY_VALUE(CurrentExperience, NewCurrentExperience);
	UE_MVVM_SET_PROPERTY_VALUE(MaxExperience, NewMaxExperience);
	UE_MVVM_SET_PROPERTY_VALUE(ExperiencePercent, NewExperiencePercent);
	UE_MVVM_SET_PROPERTY_VALUE(ExperienceText, HealthBarViewModel::MakeExperienceText(NewCurrentExperience, NewMaxExperience));
}

void UHealthBarViewModel::UpdateAllData()
{
	UpdateHealthData();
	UpdateExperienceData();
}

void UHealthBarViewModel::OnLevelChanged(const FOnAttributeChangeData& Data)
{
	static_cast<void>(Data);
	UpdateExperienceData();
}

void UHealthBarViewModel::OnExperienceChanged(const FOnAttributeChangeData& Data)
{
	static_cast<void>(Data);
	UpdateExperienceData();
}

void UHealthBarViewModel::OnMaxExperienceChanged(const FOnAttributeChangeData& Data)
{
	static_cast<void>(Data);
	UpdateExperienceData();
}

void UHealthBarViewModel::OnHealthChanged(const FOnAttributeChangeData& Data)
{
	static_cast<void>(Data);
	UpdateHealthData();
}

void UHealthBarViewModel::OnMaxHealthChanged(const FOnAttributeChangeData& Data)
{
	static_cast<void>(Data);
	UpdateHealthData();
}
