#include "Skill/Actors/SkillBlackHoleActor.h"
#include "Definition/AbilitySystem/SkillStaticSettings.h"

#include "ActiveGameplayEffectHandle.h"
#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "Definition/AbilitySystem/SkillDefinition.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Character/CharacterBase.h"
#include "Common/LabGameplayTags.h"
#include "Components/SceneComponent.h"
#include "Curves/CurveFloat.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"
#include "UObject/ObjectKey.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SkillBlackHoleActor)

namespace
{
	float GetBlackHolePandoraLoadoutDamageBonusPercent(
		const UBasicAttributeSet* AttributeSet,
		const EEnum_Direction LoadoutDirection)
	{
		if (!AttributeSet)
		{
			return 0.0f;
		}

		switch (LoadoutDirection)
		{
		case EEnum_Direction::Left:
			return FMath::Max(AttributeSet->GetFirstPandora(), 0.0f);
		case EEnum_Direction::Up:
			return FMath::Max(AttributeSet->GetSecondPandora(), 0.0f);
		case EEnum_Direction::Right:
			return FMath::Max(AttributeSet->GetThirdPandora(), 0.0f);
		case EEnum_Direction::Center:
		case EEnum_Direction::Down:
		default:
			return 0.0f;
		}
	}
}

ASkillBlackHoleActor::ASkillBlackHoleActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = true;
	SetReplicateMovement(true);
}

void ASkillBlackHoleActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ASkillBlackHoleActor, GrowthDuration, COND_InitialOnly);
}

void ASkillBlackHoleActor::BeginPlay()
{
	Super::BeginPlay();

	PullRootMotionSourceName = FName(*FString::Printf(TEXT("OmenOrbPull_%u"), GetUniqueID()));
	ResolveBlueprintComponents();
	CacheInitialComponentScales();
	StartBlackHoleSequence();
}

void ASkillBlackHoleActor::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (HasAuthority() && bPullEnemiesDuringGrowth && IsPullPhaseActive())
	{
		const float NormalizedGrowthTime = FMath::Clamp(
			GrowthElapsed / FMath::Max(GrowthDuration, UE_KINDA_SMALL_NUMBER),
			0.0f,
			1.0f);
		const float PullStrengthAlpha = bGrowthActive
			? FMath::Clamp(EvaluateCurveOrLinear(GrowthCurve, NormalizedGrowthTime), 0.0f, 1.0f)
			: 1.0f;
		PullEnemyCharacters(DeltaSeconds, PullStrengthAlpha);
	}

	TickGrowth(DeltaSeconds);
	TickCollapse(DeltaSeconds);
	TickBlackHoleVFX(DeltaSeconds);
	RefreshTickEnabledFromSequenceState();
}

void ASkillBlackHoleActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearPullRootMotionSources();

	Super::EndPlay(EndPlayReason);
}

void ASkillBlackHoleActor::StartBlackHoleSequence()
{
	ClearPullRootMotionSources();
	ResolveBlueprintComponents();
	CacheInitialComponentScales();

	GrowthElapsed = 0.0f;
	CollapseElapsed = 0.0f;
	BlackHoleFXElapsed = 0.0f;
	bGrowthActive = true;
	bCollapseActive = false;
	bBlackHoleFXActive = false;

	SetOrbAndFloorVisible(true);
	ApplyOrbScale(0.0f);
	ApplyBlackHoleFXValue(0.0f);

	RefreshTickEnabledFromSequenceState();
}

void ASkillBlackHoleActor::ConfigureFromStaticSettings(
	const FSkillStaticSettings& StaticSettings,
	const FSkillGameplayEffectConfig& FinishDamageConfig,
	const int32 InAbilityLevel,
	const EEnum_Direction InSourcePandoraLoadoutDirection)
{
	ConfiguredAbilityLevel = FMath::Max(InAbilityLevel, 1);
	SourcePandoraLoadoutDirection = InSourcePandoraLoadoutDirection;

	GrowthDuration = static_cast<float>(FMath::Max(StaticSettings.OmenOrbGrowthDuration, 0.01));
	bPullEnemiesDuringGrowth = StaticSettings.bOmenOrbPullEnemiesDuringGrowth;
	PullRadius = static_cast<float>(FMath::Max(StaticSettings.OmenOrbPullRadius, 0.0));
	PullSpeed = static_cast<float>(FMath::Max(StaticSettings.OmenOrbPullSpeed, 0.0));

	bApplyFinishAreaDamage = StaticSettings.bOmenOrbApplyFinishAreaDamage;
	FinishDamageRadius = static_cast<float>(FMath::Max(StaticSettings.OmenOrbFinishDamageRadius, 0.0));
	FinishDamageEffectClass = FinishDamageConfig.GameplayEffectClass;
	FinishDamageDataTag = FinishDamageConfig.MagnitudeDataTag.IsValid()
		? FinishDamageConfig.MagnitudeDataTag
		: LabGameplayTags::Data_Damage;
	FinishDamageMagnitude = FMath::Max(FinishDamageConfig.Magnitude, 0.0);
}

void ASkillBlackHoleActor::ResolveBlueprintComponents()
{
	OrbComponent = FindSceneComponentByName(OrbComponentName);
	FloorComponent = FindSceneComponentByName(FloorComponentName);
	BlackHoleNiagaraComponent = FindNiagaraComponentByName(BlackHoleNiagaraComponentName);
	AreaNiagaraComponent = FindNiagaraComponentByName(AreaNiagaraComponentName);
	SlashNiagaraComponent = FindNiagaraComponentByName(SlashNiagaraComponentName);
}

void ASkillBlackHoleActor::CacheInitialComponentScales()
{
	if (bInitialComponentScalesCached)
	{
		return;
	}

	bool bCachedAnyScale = false;
	if (OrbComponent)
	{
		InitialOrbScale = OrbComponent->GetComponentScale();
		bCachedAnyScale = true;
	}

	if (FloorComponent)
	{
		InitialFloorScale = FloorComponent->GetComponentScale();
		bCachedAnyScale = true;
	}

	if (bCachedAnyScale)
	{
		bInitialComponentScalesCached = true;
	}
}

void ASkillBlackHoleActor::TickGrowth(const float DeltaSeconds)
{
	if (!bGrowthActive)
	{
		return;
	}

	GrowthElapsed += FMath::Max(DeltaSeconds, 0.0f);
	const float NormalizedTime = FMath::Clamp(GrowthElapsed / FMath::Max(GrowthDuration, UE_KINDA_SMALL_NUMBER), 0.0f, 1.0f);
	const float GrowthAlpha = FMath::Clamp(EvaluateCurveOrLinear(GrowthCurve, NormalizedTime), 0.0f, 1.0f);

	ApplyOrbScale(GrowthAlpha);

	if (NormalizedTime >= 1.0f)
	{
		FinishGrowth();
	}
}

void ASkillBlackHoleActor::TickCollapse(const float DeltaSeconds)
{
	if (!bCollapseActive)
	{
		return;
	}

	CollapseElapsed += FMath::Max(DeltaSeconds, 0.0f);
	const float NormalizedTime = FMath::Clamp(CollapseElapsed / FMath::Max(CollapseDuration, UE_KINDA_SMALL_NUMBER), 0.0f, 1.0f);
	const float CollapseAlpha = FMath::Clamp(EvaluateCurveOrLinear(CollapseCurve, NormalizedTime), 0.0f, 1.0f);

	ApplyOrbScale(1.0f - CollapseAlpha);

	if (NormalizedTime >= 1.0f)
	{
		bCollapseActive = false;
	}
}

void ASkillBlackHoleActor::TickBlackHoleVFX(const float DeltaSeconds)
{
	if (!bBlackHoleFXActive)
	{
		return;
	}

	BlackHoleFXElapsed += FMath::Max(DeltaSeconds, 0.0f);
	const float NormalizedTime = FMath::Clamp(BlackHoleFXElapsed / FMath::Max(BlackHoleFXDuration, UE_KINDA_SMALL_NUMBER), 0.0f, 1.0f);
	const float FXAlpha = FMath::Clamp(EvaluateCurveOrLinear(BlackHoleFXCurve, NormalizedTime), 0.0f, 1.0f);
	const float FXValue = FMath::Lerp(BlackHoleFXStartValue, BlackHoleFXEndValue, FXAlpha);
	ApplyBlackHoleFXValue(FXValue);

	if (NormalizedTime >= 1.0f)
	{
		FinishBlackHoleVFX();
	}
}

void ASkillBlackHoleActor::FinishGrowth()
{
	if (!bGrowthActive)
	{
		return;
	}

	bGrowthActive = false;
	ApplyOrbScale(1.0f);
	SetOrbAndFloorVisible(false);

	BlackHoleFXElapsed = 0.0f;
	bBlackHoleFXActive = true;
	ApplyBlackHoleFXValue(BlackHoleFXStartValue);

	if (bCollapseOrbDuringBlackHoleFX)
	{
		CollapseElapsed = 0.0f;
		bCollapseActive = true;
	}
}

void ASkillBlackHoleActor::FinishBlackHoleVFX()
{
	if (!bBlackHoleFXActive)
	{
		return;
	}

	bBlackHoleFXActive = false;
	ApplyBlackHoleFXValue(0.0f);

	if (bActivateFinishNiagara)
	{
		ActivateFinishNiagara();
	}

	// The last pull update already ran at the start of this tick. Remove the
	// persistent root-motion source immediately before applying finish damage so
	// it cannot be re-applied during a longer collapse tail.
	ClearPullRootMotionSources();

	if (HasAuthority() && bApplyFinishAreaDamage)
	{
		ApplyFinishAreaDamage();
	}

	RefreshTickEnabledFromSequenceState();
	OnOrbSequenceFinished();
}

void ASkillBlackHoleActor::RefreshTickEnabledFromSequenceState()
{
	SetActorTickEnabled(bGrowthActive || bCollapseActive || bBlackHoleFXActive);
}

void ASkillBlackHoleActor::ApplyOrbScale(const float ScaleAlpha) const
{
	const float SafeScaleAlpha = FMath::Clamp(ScaleAlpha, 0.0f, 1.0f);
	const float ScaleMultiplier = FMath::Lerp(GrowthStartScaleMultiplier, GrowthEndScaleMultiplier, SafeScaleAlpha);

	if (OrbComponent)
	{
		OrbComponent->SetWorldScale3D(InitialOrbScale * ScaleMultiplier);
	}

	if (FloorComponent)
	{
		FloorComponent->SetWorldScale3D(InitialFloorScale * ScaleMultiplier);
	}
}

void ASkillBlackHoleActor::SetOrbAndFloorVisible(const bool bVisible) const
{
	if (OrbComponent)
	{
		OrbComponent->SetVisibility(bVisible, true);
	}

	if (FloorComponent)
	{
		FloorComponent->SetVisibility(bVisible, true);
	}
}

void ASkillBlackHoleActor::ApplyBlackHoleFXValue(const float FXValue) const
{
	if (BlackHoleNiagaraComponent && !BlackHoleFXActivateParameterName.IsNone())
	{
		BlackHoleNiagaraComponent->SetVariableFloat(BlackHoleFXActivateParameterName, FXValue);
	}
}

void ASkillBlackHoleActor::ActivateFinishNiagara() const
{
	if (AreaNiagaraComponent)
	{
		AreaNiagaraComponent->SetActive(true, bResetFinishNiagaraOnActivate);
	}

	if (SlashNiagaraComponent)
	{
		SlashNiagaraComponent->SetActive(true, bResetFinishNiagaraOnActivate);
	}
}

void ASkillBlackHoleActor::ApplyFinishAreaDamage()
{
	UWorld* World = GetWorld();
	AActor* SourceActor = ResolveSourceActor();
	UAbilitySystemComponent* SourceASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(SourceActor);
	const float DamageMagnitude = CalculateFinishAreaDamageMagnitude(SourceASC);
	if (!World || !SourceActor || !SourceActor->HasAuthority() || !SourceASC || !FinishDamageEffectClass || FinishDamageRadius <= 0.0f || DamageMagnitude <= 0.0f)
	{

		return;
	}

	FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
	EffectContext.AddInstigator(SourceActor, SourceActor);
	EffectContext.AddSourceObject(this);

	FGameplayEffectSpecHandle DamageSpecHandle = SourceASC->MakeOutgoingSpec(
		FinishDamageEffectClass,
		ConfiguredAbilityLevel,
		EffectContext);
	if (!DamageSpecHandle.IsValid() || !DamageSpecHandle.Data.IsValid())
	{

		return;
	}

	const FGameplayTag DamageDataTag = FinishDamageDataTag.IsValid() ? FinishDamageDataTag : LabGameplayTags::Data_Damage;
	DamageSpecHandle.Data->SetSetByCallerMagnitude(DamageDataTag, DamageMagnitude);

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(OmenOrbFinishDamage), false, this);
	QueryParams.AddIgnoredActor(this);
	QueryParams.AddIgnoredActor(SourceActor);

	TArray<FOverlapResult> OverlapResults;
	World->OverlapMultiByObjectType(
		OverlapResults,
		GetActorLocation(),
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(FinishDamageRadius),
		QueryParams);

	TSet<FObjectKey> DamagedCharacters;
	int32 AppliedCount = 0;
	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		ACharacterBase* TargetCharacter = Cast<ACharacterBase>(OverlapResult.GetActor());
		if (!IsValid(TargetCharacter))
		{
			continue;
		}

		const FObjectKey TargetKey(TargetCharacter);
		if (DamagedCharacters.Contains(TargetKey) || !ShouldPullCharacter(TargetCharacter))
		{
			continue;
		}

		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetCharacter);
		if (!TargetASC)
		{
			continue;
		}

		const FActiveGameplayEffectHandle AppliedHandle = SourceASC->ApplyGameplayEffectSpecToTarget(*DamageSpecHandle.Data.Get(), TargetASC);
		DamagedCharacters.Add(TargetKey);
		AppliedCount += AppliedHandle.WasSuccessfullyApplied() || AppliedHandle.IsValid() ? 1 : 0;
	}

}

float ASkillBlackHoleActor::CalculateFinishAreaDamageMagnitude(const UAbilitySystemComponent* SourceASC) const
{
	const double BaseMagnitude = FMath::Max(FinishDamageMagnitude, 0.0);
	const FGameplayTag DamageDataTag = FinishDamageDataTag.IsValid() ? FinishDamageDataTag : LabGameplayTags::Data_Damage;
	const UBasicAttributeSet* SourceAttributeSet = SourceASC ? SourceASC->GetSet<UBasicAttributeSet>() : nullptr;
	const float IntelligenceDamagePercent = DamageDataTag.MatchesTag(LabGameplayTags::Data_Damage) && SourceAttributeSet
		? FMath::Max(SourceAttributeSet->GetIntelligence(), 0.0f)
		: 0.0f;
	const float LoadoutDamagePercent = DamageDataTag.MatchesTag(LabGameplayTags::Data_Damage) && SourceAttributeSet
		? GetBlackHolePandoraLoadoutDamageBonusPercent(SourceAttributeSet, SourcePandoraLoadoutDirection)
		: 0.0f;
	const float AttackDamageBonusPercent = IntelligenceDamagePercent + LoadoutDamagePercent;
	const double IntelligenceMultiplier = 1.0 + (static_cast<double>(AttackDamageBonusPercent) * 0.01);
	return FMath::Max(static_cast<float>(BaseMagnitude * IntelligenceMultiplier), 0.0f);
}

void ASkillBlackHoleActor::PullEnemyCharacters(const float DeltaSeconds, const float GrowthAlpha)
{
	UWorld* World = GetWorld();
	if (!World || PullRadius <= 0.0f || PullSpeed <= 0.0f || DeltaSeconds <= 0.0f)
	{
		return;
	}

	const float EffectiveRadius = PullRadius * (bScalePullRadiusWithGrowthAlpha ? FMath::Clamp(GrowthAlpha, 0.0f, 1.0f) : 1.0f);
	const float EffectiveSpeed = PullSpeed * (bScalePullStrengthWithGrowthAlpha ? FMath::Clamp(GrowthAlpha, 0.0f, 1.0f) : 1.0f);
	if (EffectiveRadius <= UE_KINDA_SMALL_NUMBER || EffectiveSpeed <= UE_KINDA_SMALL_NUMBER)
	{
		return;
	}

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(OmenOrbPull), false, this);
	QueryParams.AddIgnoredActor(this);
	if (AActor* SourceActor = ResolveSourceActor())
	{
		QueryParams.AddIgnoredActor(SourceActor);
	}

	TArray<FOverlapResult> OverlapResults;
	World->OverlapMultiByObjectType(
		OverlapResults,
		GetActorLocation(),
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(EffectiveRadius),
		QueryParams);

	TSet<FObjectKey> PulledCharacters;
	TSet<UCharacterMovementComponent*> AffectedMovementComponents;
	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		ACharacterBase* TargetCharacter = Cast<ACharacterBase>(OverlapResult.GetActor());
		if (!IsValid(TargetCharacter) || PulledCharacters.Contains(FObjectKey(TargetCharacter)) || !ShouldPullCharacter(TargetCharacter))
		{
			continue;
		}

		FVector PullCenter = GetActorLocation();
		FVector TargetLocation = TargetCharacter->GetActorLocation();
		if (bPullOnHorizontalPlane)
		{
			PullCenter.Z = TargetLocation.Z;
		}

		const FVector ToCenter = PullCenter - TargetLocation;
		const double Distance = ToCenter.Size();
		if (Distance <= FMath::Max(PullStopDistance, 0.0f) || Distance <= UE_KINDA_SMALL_NUMBER)
		{
			continue;
		}

		UCharacterMovementComponent* MovementComponent = TargetCharacter->GetCharacterMovement();
		if (!MovementComponent)
		{
			continue;
		}

		AffectedMovementComponents.Add(MovementComponent);
		TSharedPtr<FRootMotionSource> ExistingSource =
			MovementComponent->GetRootMotionSource(PullRootMotionSourceName);
		TSharedPtr<FRootMotionSource_RadialForce> PullSource;
		if (ExistingSource.IsValid()
			&& ExistingSource->GetScriptStruct() == FRootMotionSource_RadialForce::StaticStruct())
		{
			PullSource = StaticCastSharedPtr<FRootMotionSource_RadialForce>(ExistingSource);
		}
		else
		{
			if (ExistingSource.IsValid())
			{
				MovementComponent->RemoveRootMotionSource(PullRootMotionSourceName);
			}

			PullSource = MakeShared<FRootMotionSource_RadialForce>();
			PullSource->InstanceName = PullRootMotionSourceName;
			PullSource->Priority = 5;
			PullSource->Duration = -1.0f;
			PullSource->AccumulateMode = ERootMotionAccumulateMode::Additive;
			PullSource->bIsPush = false;
			MovementComponent->ApplyRootMotionSource(PullSource);
		}

		PullSource->Location = GetActorLocation();
		PullSource->LocationActor = this;
		PullSource->Radius = EffectiveRadius;
		PullSource->Strength = EffectiveSpeed;
		PullSource->bNoZForce = bPullOnHorizontalPlane;

		if (!ActivePullMovementComponents.Contains(MovementComponent))
		{
			ActivePullMovementComponents.Add(MovementComponent);
		}

		PulledCharacters.Add(FObjectKey(TargetCharacter));
	}

	for (int32 Index = ActivePullMovementComponents.Num() - 1; Index >= 0; --Index)
	{
		UCharacterMovementComponent* MovementComponent = ActivePullMovementComponents[Index].Get();
		if (!MovementComponent || !AffectedMovementComponents.Contains(MovementComponent))
		{
			if (MovementComponent)
			{
				MovementComponent->RemoveRootMotionSource(PullRootMotionSourceName);
			}
			ActivePullMovementComponents.RemoveAtSwap(Index, 1, EAllowShrinking::No);
		}
	}
}

void ASkillBlackHoleActor::ClearPullRootMotionSources()
{
	for (const TWeakObjectPtr<UCharacterMovementComponent>& MovementComponentPtr : ActivePullMovementComponents)
	{
		if (UCharacterMovementComponent* MovementComponent = MovementComponentPtr.Get())
		{
			MovementComponent->RemoveRootMotionSource(PullRootMotionSourceName);
		}
	}
	ActivePullMovementComponents.Reset();
}

bool ASkillBlackHoleActor::IsPullPhaseActive() const
{
	return bGrowthActive || bBlackHoleFXActive;
}

bool ASkillBlackHoleActor::ShouldPullCharacter(ACharacterBase* TargetCharacter) const
{
	if (!TargetCharacter || TargetCharacter == ResolveSourceActor())
	{
		return false;
	}

	const UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetCharacter);
	if (TargetASC && TargetASC->HasMatchingGameplayTag(LabGameplayTags::State_Dead))
	{
		return false;
	}

	const ACharacterBase* SourceCharacter = Cast<ACharacterBase>(ResolveSourceActor());
	if (!SourceCharacter)
	{
		return !bRequireSourceCharacterForTeamFilter;
	}

	if (!SourceCharacter->CanDamageCharacterByTeam(TargetCharacter))
	{
		return false;
	}

	const int32 SourceFactionId = SourceCharacter->GetFactionId();
	return SourceFactionId == 0 || SourceFactionId != TargetCharacter->GetFactionId();
}

AActor* ASkillBlackHoleActor::ResolveSourceActor() const
{
	if (AActor* OwnerActor = GetOwner())
	{
		return OwnerActor;
	}

	return GetInstigator();
}

USceneComponent* ASkillBlackHoleActor::FindSceneComponentByName(const FName ComponentName) const
{
	if (ComponentName.IsNone())
	{
		return nullptr;
	}

	TArray<USceneComponent*> SceneComponents;
	GetComponents<USceneComponent>(SceneComponents);
	for (USceneComponent* SceneComponent : SceneComponents)
	{
		if (!SceneComponent)
		{
			continue;
		}

		if (SceneComponent->GetFName() == ComponentName || SceneComponent->ComponentHasTag(ComponentName))
		{
			return SceneComponent;
		}
	}

	for (USceneComponent* SceneComponent : SceneComponents)
	{
		if (SceneComponent && SceneComponent->GetName().Contains(ComponentName.ToString()))
		{
			return SceneComponent;
		}
	}

	return nullptr;
}

UNiagaraComponent* ASkillBlackHoleActor::FindNiagaraComponentByName(const FName ComponentName) const
{
	return Cast<UNiagaraComponent>(FindSceneComponentByName(ComponentName));
}

float ASkillBlackHoleActor::EvaluateCurveOrLinear(const UCurveFloat* Curve, const float NormalizedTime) const
{
	const float ClampedTime = FMath::Clamp(NormalizedTime, 0.0f, 1.0f);
	return Curve ? Curve->GetFloatValue(ClampedTime) : ClampedTime;
}
