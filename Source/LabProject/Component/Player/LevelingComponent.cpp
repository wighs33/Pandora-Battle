#include "Component/Player/LevelingComponent.h"

#include "AbilitySystemGlobals.h"
#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "Common/Enum_Operation.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "GameplayEffect.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LevelingComponent)

// 서버에서 획득 경험치에 맞는 상승 레벨 수와 남은 경험치, 카테고리별 포인트를 계산해 한 번에 지급한다.
bool ULevelingComponent::GrantRewardExperience(const int32 ExperienceAmount)
{
	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority() || ExperienceAmount <= 0 || !LevelingGameplayEffectClass)
	{
		return false;
	}

	UPdAbilitySystemComponent* ASC = Cast<UPdAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwnerActor));
	FGameplayAttribute ExperienceAttribute;
	FGameplayAttribute LevelAttribute;
	if (!ASC || !ASC->ResolveAttributeFromTag(ExperienceStatTag, ExperienceAttribute)
		|| !ASC->ResolveAttributeFromTag(LevelStatTag, LevelAttribute))
	{
		return false;
	}

	const float CurrentExperience = ASC->GetNumericAttribute(ExperienceAttribute);
	const float RequiredExperience = ASC->GetNumericAttribute(UBasicAttributeSet::GetMaxExperienceAttribute());
	if (!FMath::IsFinite(CurrentExperience) || !FMath::IsFinite(RequiredExperience) || RequiredExperience <= 0.f)
	{
		return false;
	}

	// 현재 규칙은 레벨마다 같은 요구량을 소비한다. 중간 경험치 합산은 double로 계산해 큰 보상의 오차를 줄인다.
	const double TotalExperience = static_cast<double>(CurrentExperience) + ExperienceAmount;
	const float LevelsGained = static_cast<float>(FMath::FloorToDouble(TotalExperience / RequiredExperience));
	const float RemainingExperience = static_cast<float>(FMath::Fmod(TotalExperience, static_cast<double>(RequiredExperience)));
	const float PointsGained = LevelsGained * FMath::Max(PointsPerCategoryOnLevelUp, 0.f);
	if (!FMath::IsFinite(LevelsGained) || !FMath::IsFinite(PointsGained))
	{
		return false;
	}

	TMap<FGameplayTag, float> StatMagnitudes;
	StatMagnitudes.Add(ExperienceStatTag, RemainingExperience - CurrentExperience);
	if (LevelsGained > 0.f)
	{
		StatMagnitudes.Add(LevelStatTag, LevelsGained);
		for (const FGameplayTag& CategoryPointTag : CategoryPointStatTags)
		{
			if (CategoryPointTag.IsValid())
			{
				StatMagnitudes.FindOrAdd(CategoryPointTag) += PointsGained;
			}
		}
	}

	return ASC->ApplyStatUpEffectByTags(LevelingGameplayEffectClass, StatMagnitudes, EEnum_Operation::Add);
}

// 경험치 바와 레벨업 계산이 같은 GAS 요구 경험치를 사용하도록 현재 값을 조회한다.
float ULevelingComponent::GetRequiredExperienceForNextLevel() const
{
	const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
	return ASC ? ASC->GetNumericAttribute(UBasicAttributeSet::GetMaxExperienceAttribute()) : 0.f;
}
