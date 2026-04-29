#include "HealthBarViewModel.h"

#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/PdAttributeSet.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HealthBarViewModel)

DEFINE_LOG_CATEGORY(HealthBarViewModelLog);

const FName UHealthBarViewModel::ViewModelName = TEXT("HealthBarViewModel");

namespace HealthBarViewModel
{
	/** ASC에서 Attribute 값을 읽습니다. */
	float GetAttributeValue(UAbilitySystemComponent* ASC, const FGameplayAttribute& Attribute)
	{
		bool bFound = false;
		return ASC ? ASC->GetGameplayAttributeValue(Attribute, bFound) : 0.f;
	}

	/** 체력 텍스트를 만듭니다. */
	FText MakeHealthText(const float InHealth, const float InMaxHealth)
	{
		return FText::Format(
			NSLOCTEXT("HealthBarViewModel", "HealthTextFormat", "{0}/{1}"),
			FText::AsNumber(FMath::RoundToInt(InHealth)),
			FText::AsNumber(FMath::RoundToInt(InMaxHealth)));
	}
}

/** 체력바 ViewModel 기본 상태를 초기화합니다. */
UHealthBarViewModel::UHealthBarViewModel()
{
	ResetViewData();
}

/** SourceObject로부터 ASC를 찾아 ViewModel을 초기화합니다. */
void UHealthBarViewModel::InitializeViewModel(UObject* SourceObject)
{
	// =================================================================================================================
	// === ASC 해석

	UAbilitySystemComponent* InASC = ResolveAbilitySystemComponent(SourceObject);
	if (!InASC)
	{
		if (IsViewModelInitialized())
		{
			UninitializeViewModel();
		}

		UE_LOG(HealthBarViewModelLog, Warning, TEXT("InitializeViewModel failed: SourceObject '%s' does not provide an AbilitySystemComponent."), *GetNameSafe(SourceObject));
		ResetViewData();
		return;
	}

	// =================================================================================================================
	// === 같은 ASC 재사용

	if (ASC.Get() == InASC && IsViewModelInitialized())
	{
		UpdateAllData();
		return;
	}

	// =================================================================================================================
	// === 기존 바인딩 정리

	if (IsViewModelInitialized())
	{
		UninitializeViewModel();
	}

	ASC = InASC;

	// =================================================================================================================
	// === 변경 델리게이트 바인딩

	InASC->GetGameplayAttributeValueChangeDelegate(UPdAttributeSet::GetHealthAttribute()).AddUObject(this, &ThisClass::OnHealthChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UPdAttributeSet::GetMaxHealthAttribute()).AddUObject(this, &ThisClass::OnMaxHealthChanged);

	// =================================================================================================================
	// === 초기 값 갱신

	UpdateAllData();

	Super::InitializeViewModel(SourceObject);
}

/** ASC 바인딩을 해제하고 ViewModel을 초기화합니다. */
void UHealthBarViewModel::UninitializeViewModel()
{
	// =================================================================================================================
	// === 델리게이트 해제

	if (UAbilitySystemComponent* ASCPtr = ASC.Get())
	{
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UPdAttributeSet::GetHealthAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UPdAttributeSet::GetMaxHealthAttribute()).RemoveAll(this);
	}

	// =================================================================================================================
	// === 상태 초기화

	ASC.Reset();
	ResetViewData();

	Super::UninitializeViewModel();
}

/** SourceObject에서 ASC를 찾습니다. */
UAbilitySystemComponent* UHealthBarViewModel::ResolveAbilitySystemComponent(UObject* SourceObject) const
{
	// =================================================================================================================
	// === ASC 직접 캐스팅

	if (UAbilitySystemComponent* InASC = Cast<UAbilitySystemComponent>(SourceObject))
	{
		return InASC;
	}

	// =================================================================================================================
	// === 인터페이스 경유 조회

	if (const IAbilitySystemInterface* AbilitySystemInterface = Cast<IAbilitySystemInterface>(SourceObject))
	{
		return AbilitySystemInterface->GetAbilitySystemComponent();
	}

	// =================================================================================================================
	// === 컴포넌트 Owner 경유 조회

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

/** 표시값을 기본값으로 초기화합니다. */
void UHealthBarViewModel::ResetViewData()
{
	UE_MVVM_SET_PROPERTY_VALUE(Health, 0.f);
	UE_MVVM_SET_PROPERTY_VALUE(MaxHealth, 0.f);
	UE_MVVM_SET_PROPERTY_VALUE(HealthPercent, 0.f);
	UE_MVVM_SET_PROPERTY_VALUE(HealthText, FText::GetEmpty());
	UE_MVVM_SET_PROPERTY_VALUE(bIsAlive, false);
}

/** 체력 관련 값만 갱신합니다. */
void UHealthBarViewModel::UpdateHealthData()
{
	// =================================================================================================================
	// === ASC 확인

	UAbilitySystemComponent* ASCPtr = ASC.Get();
	if (!ASCPtr)
	{
		ResetViewData();
		return;
	}

	// =================================================================================================================
	// === 현재 값 계산

	const float CurrentHealth = HealthBarViewModel::GetAttributeValue(ASCPtr, UPdAttributeSet::GetHealthAttribute());
	const float CurrentMaxHealth = HealthBarViewModel::GetAttributeValue(ASCPtr, UPdAttributeSet::GetMaxHealthAttribute());
	const float CurrentHealthPercent = CurrentMaxHealth > 0.f ? FMath::Clamp(CurrentHealth / CurrentMaxHealth, 0.f, 1.f) : 0.f;

	// =================================================================================================================
	// === ViewModel 값 반영

	UE_MVVM_SET_PROPERTY_VALUE(Health, CurrentHealth);
	UE_MVVM_SET_PROPERTY_VALUE(MaxHealth, CurrentMaxHealth);
	UE_MVVM_SET_PROPERTY_VALUE(HealthPercent, CurrentHealthPercent);
	UE_MVVM_SET_PROPERTY_VALUE(HealthText, HealthBarViewModel::MakeHealthText(CurrentHealth, CurrentMaxHealth));
	UE_MVVM_SET_PROPERTY_VALUE(bIsAlive, CurrentHealth > 0.f);
}

/** 모든 표시값을 갱신합니다. */
void UHealthBarViewModel::UpdateAllData()
{
	UpdateHealthData();
}

/** 체력 변경 시 데이터를 갱신합니다. */
void UHealthBarViewModel::OnHealthChanged(const FOnAttributeChangeData& Data)
{
	static_cast<void>(Data);
	UpdateHealthData();
}

/** 최대 체력 변경 시 데이터를 갱신합니다. */
void UHealthBarViewModel::OnMaxHealthChanged(const FOnAttributeChangeData& Data)
{
	static_cast<void>(Data);
	UpdateHealthData();
}