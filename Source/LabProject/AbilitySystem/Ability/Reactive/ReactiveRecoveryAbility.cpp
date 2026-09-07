#include "AbilitySystem/Ability/Reactive/ReactiveRecoveryAbility.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "Common/LabGameplayTags.h"
#include "Definition/Settings/GameSettingDefinition.h"
#include "GameplayEffect.h"
#include "Settings/GameSettingsSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ReactiveRecoveryAbility)

UReactiveRecoveryAbility::UReactiveRecoveryAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bAutoActivateWhenGranted = true;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	ActivationOwnedTags.Reset();
}

// 서버에서 회복에 영향을 주는 속성을 구독하고 현재 회복량을 적용한다.
void UReactiveRecoveryAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC || !ASC->IsOwnerActorAuthoritative())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
		return;
	}

	BoundASC = ASC;
	for (const FGameplayAttribute& Attribute : { UBasicAttributeSet::GetRecoveryAttribute(), UBasicAttributeSet::GetMaxHealthAttribute(),
		UBasicAttributeSet::GetMaxManaAttribute(), UBasicAttributeSet::GetHealthAttribute() })
	{
		AttributeChangedHandles.Add(Attribute, ASC->GetGameplayAttributeValueChangeDelegate(Attribute)
			.AddUObject(this, &ThisClass::HandleRecoveryAttributeChanged));
	}
	RefreshRecoveryEffect();
}

// 사망·캐릭터 해제 시 회복 효과와 구독을 함께 제거한다.
void UReactiveRecoveryAbility::OnAbilityEnding()
{
	Super::OnAbilityEnding();
	if (UAbilitySystemComponent* ASC = BoundASC.Get())
	{
		for (const TPair<FGameplayAttribute, FDelegateHandle>& Entry : AttributeChangedHandles)
		{
			ASC->GetGameplayAttributeValueChangeDelegate(Entry.Key).Remove(Entry.Value);
		}
		ASC->RemoveActiveGameplayEffect(RecoveryEffectHandle);
	}
	AttributeChangedHandles.Reset();
	RecoveryEffectHandle.Invalidate();
	BoundASC.Reset();
}

// 일반적인 피해·회복마다 GE를 갱신하지 않고, 생존 여부가 달라질 때만 체력 변화를 반영한다.
void UReactiveRecoveryAbility::HandleRecoveryAttributeChanged(const FOnAttributeChangeData& Data)
{
	if (Data.Attribute == UBasicAttributeSet::GetHealthAttribute() && (Data.OldValue > 0.f) == (Data.NewValue > 0.f))
	{
		return;
	}
	RefreshRecoveryEffect();
}

// 기존 GE의 SetByCaller만 갱신하므로 최대 자원이나 회복력이 바뀌어도 주기와 시작 시각은 유지된다.
void UReactiveRecoveryAbility::RefreshRecoveryEffect()
{
	UAbilitySystemComponent* ASC = BoundASC.Get();
	const UBasicAttributeSet* Attributes = ASC ? ASC->GetSet<UBasicAttributeSet>() : nullptr;
	if (!Attributes || bRefreshingRecovery)
	{
		return;
	}
	TGuardValue<bool> RefreshingRecovery(bRefreshingRecovery, true);

	const bool bAlive = Attributes->GetHealth() > 0.f && !ASC->HasMatchingGameplayTag(LabGameplayTags::State_Dead);
	const float Recovery = bAlive ? FMath::Max(Attributes->GetRecovery(), 0.f) * 0.01f : 0.f;
	TMap<FGameplayTag, float> Magnitudes;
	Magnitudes.Add(LabGameplayTags::Data_Heal, FMath::Max(Attributes->GetMaxHealth(), 0.f) * Recovery);
	Magnitudes.Add(LabGameplayTags::Data_Mana, FMath::Max(Attributes->GetMaxMana(), 0.f) * Recovery);
	if (ASC->GetActiveGameplayEffect(RecoveryEffectHandle))
	{
		ASC->UpdateActiveGameplayEffectSetByCallerMagnitudes(RecoveryEffectHandle, Magnitudes);
		return;
	}
	if (Recovery <= 0.f)
	{
		return;
	}

	const UGameSettingDefinition* Settings = UGameSettingsSubsystem::ResolveGameSettingDefinition(this);
	if (!Settings || !Settings->RecoveryHealGameplayEffectClass)
	{
		return;
	}

	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddSourceObject(this);
	FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(Settings->RecoveryHealGameplayEffectClass, 1.f, Context);
	if (!Spec.IsValid())
	{
		return;
	}
	for (const TPair<FGameplayTag, float>& Entry : Magnitudes)
	{
		Spec.Data->SetSetByCallerMagnitude(Entry.Key, Entry.Value);
	}
	RecoveryEffectHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
}
