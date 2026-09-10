#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystemInterface.h"
#include "Engine/NetSerialization.h"
#include "GameFramework/Character.h"
#include "GameplayCueInterface.h"
#include "GameplayTagContainer.h"
#include "CharacterBase.generated.h"

class APlayerController;
class UAbilitySystemComponent;
class UAnimInstance;
class UAbilityStateComponent;
class UCharacterBaseDefinition;
class UCharacterDeathComponent;
class UCharacterHealthBarComponent;
class UCharacterMovementComponent;
class UCharacterPresentationComponent;
class UCombatComponent;
class UDamageIndicatorComponent;
class UEquipmentComponent;
class UMaterialInterface;
class UNiagaraComponent;
class UNiagaraSystem;
class UPdAbilitySystemComponent;
class USkinEquipmentComponent;
class UStatusEffectReplicationComponent;
class UUserWidget;
class UWidgetClassDefinition;
class UWidgetComponent;
struct FStreamableHandle;

/**
 * 캐릭터 공통 컴포넌트를 구성하고 생명주기를 연결한다.
 *
 * 플레이어와 적의 ASC 소유 방식은 파생 클래스가 정하며,
 * 사망·이동 상태·외형의 실제 처리는 각 컴포넌트에 맡긴다.
 */
UCLASS()
class LABPROJECT_API ACharacterBase : public ACharacter, public IAbilitySystemInterface, public IGameplayCueInterface
{
	GENERATED_BODY()

public:
	ACharacterBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// 엔진 생명주기와 조종자·이동 상태 변경.
	virtual void PreInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;
	virtual void NotifyControllerChanged() override;
	virtual void OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode = 0) override;
	virtual void UnPossessed() override;

	// ASC 소유 관계와 능력 상태 연결.
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual void HandleGameplayCue(
		AActor* Self, FGameplayTag GameplayCueTag, EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters) override;

	UFUNCTION(BlueprintPure, Category = "!AbilitySystem")
	UPdAbilitySystemComponent* GetPdAbilitySystemComponent() const;

	void InitializeAbilitySystemActorInfo();
	void ClearAbilitySystemActorInfo();

	// 캐릭터 공통 기능을 실제로 처리하는 컴포넌트.
	UFUNCTION(BlueprintPure, Category = "!Character|Runtime")
	UAbilityStateComponent* GetAbilityStateComponent() const { return AbilityStateComponent; }

	UFUNCTION(BlueprintPure, Category = "!Character|Death")
	UCharacterDeathComponent* GetCharacterDeathComponent() const { return CharacterDeathComponent; }

	UFUNCTION(BlueprintPure, Category = "!Character|Presentation")
	UCharacterPresentationComponent* GetCharacterPresentationComponent() const { return CharacterPresentationComponent; }

	UFUNCTION(BlueprintPure, Category = "!AbilitySystem|StatusEffect")
	UStatusEffectReplicationComponent* GetStatusEffectReplicationComponent() const { return StatusEffectReplicationComponent; }

	UFUNCTION(BlueprintPure, Category = "!Character|Health Bar")
	UCharacterHealthBarComponent* GetCharacterHealthBarComponent() const;

	UFUNCTION(BlueprintPure, Category = "!Equipment")
	UEquipmentComponent* GetEquipmentComponent() const;

	UFUNCTION(BlueprintPure, Category = "!Combat")
	UCombatComponent* GetCombatComponent() const;

	UFUNCTION(BlueprintPure, Category = "!Skin")
	USkinEquipmentComponent* GetSkinEquipmentComponent() const { return SkinEquipmentComponent; }

	UFUNCTION(BlueprintPure, Category = "!DamageIndicator")
	UDamageIndicatorComponent* GetDamageIndicatorComponent() const;

	// 체력바 데이터 연결과 관찰자별 표시.
	UFUNCTION(BlueprintCallable, Category = "!ViewModel")
	void RefreshHealthBarViewModel();

	void UpdateHealthBarVisibilityForLocalViewer(
		APlayerController* LocalPlayerController, const FVector& CameraLocation, const FRotator& CameraRotation, float MaxDistanceSquared);
	void SetHealthBarVisibleForLocalViewer(bool bVisible);

	// 조준 애니메이션·장비 레이어·스킬 외형.
	UFUNCTION(BlueprintPure, Category = "!Animation|Aim")
	float GetAimYawForAnimation() const;

	UFUNCTION(BlueprintPure, Category = "!Animation|Aim")
	float GetAimPitchForAnimation() const;

	UFUNCTION(BlueprintCallable, Category = "!Animation")
	void ResetAnimationToDefault();

	UFUNCTION(BlueprintCallable, Category = "!Animation")
	void SetCurrentAnimLayer(TSubclassOf<UAnimInstance> AnimLayerClass);

	void ApplySkillPresentationOverlay(UObject* PresentationSource, UMaterialInterface* OverlayMaterial);
	void ClearSkillPresentationOverlay(UObject* PresentationSource);

	UFUNCTION(BlueprintCallable, Category = "!Ability|Aura")
	void ApplyBodyAuraNiagaraWithOffset(FName ComponentName, UNiagaraSystem* NiagaraSystem, bool bActivate, bool bResetSystem,
		FVector RelativeLocationOffset, FVector RelativeScale);

	void ClearBodyAuraNiagaraIfMatching(FName ComponentName, const UNiagaraSystem* ExpectedNiagaraSystem);

	// 경기 팀과 진영, 빙결·사망 상태.
	UFUNCTION()
	int32 GetMatchTeamColorIndex() const;

	UFUNCTION()
	bool IsSameTeam(const ACharacterBase* OtherCharacter) const;

	UFUNCTION(BlueprintPure, Category = "!Team")
	bool CanDamageCharacterByTeam(const ACharacterBase* OtherCharacter) const;

	UFUNCTION(BlueprintPure, Category = "!Faction")
	int32 GetFactionId() const;

	UFUNCTION(BlueprintPure, Category = "!Status")
	bool IsStatusFrozen() const;

	// 스킬이나 빙결이 임시 회전 제어를 해제하면 캐릭터의 평상시 회전 정책을 복구한다.
	void ReapplyCurrentRotationPolicy();

	bool IsDeathHandled() const;

	// GAS의 사망 상태를 조회한다. 사망 연출 처리 여부와는 별개다.
	bool IsDead() const;

	// 피격·사망·리스폰의 캐릭터 진입점. 서버의 확정 결과는 아래 Multicast RPC로 전달한다.
	UFUNCTION(BlueprintImplementableEvent, Category = "!Damage", meta = (DisplayName = "On Damage Taken"))
	void OnDamageTaken(float DamageAmount, bool bCriticalHit, FVector WorldLocation);

	virtual void HandleDamageTaken(float DamageAmount, bool bCriticalHit = false, bool bAllowHitReact = true,
		AActor* DamageInstigator = nullptr, AActor* DamageCauser = nullptr);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "!Damage")
	void HandleDeath();

	UFUNCTION(BlueprintCallable, Category = "!Damage|Respawn")
	virtual void ResetDeathStateForRespawn();

	void ResetDeathStateForRespawnAtTransform(const FTransform& RespawnTransform);

	UFUNCTION(BlueprintCallable, Category = "!Damage|Death|Dissolve")
	void StartDeathDissolve(float DurationSeconds);

	UFUNCTION(BlueprintCallable, Category = "!Presentation")
	void ClearCharacterOverlayMaterial();

	UFUNCTION(BlueprintImplementableEvent, Category = "!AbilitySystem|Cue", meta = (DisplayName = "On Dash Cue Activated"))
	void OnDashCueActivated(const FGameplayCueParameters& Parameters);

	UFUNCTION(BlueprintImplementableEvent, Category = "!AbilitySystem|Cue", meta = (DisplayName = "On Dash Cue Removed"))
	void OnDashCueRemoved(const FGameplayCueParameters& Parameters);

protected:
	// 서버가 확정한 피격·사망 연출과 리스폰 위치를 각 클라이언트에 적용한다.
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastHandleDamageTaken(float DamageAmount, bool bCriticalHit, FVector_NetQuantize WorldLocation);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastHandleDeath();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastStartDeathDissolve(float DurationSeconds);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastResetDeathStateForRespawnAtTransform(const FTransform& RespawnTransform);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastClearCharacterOverlayMaterial();

	// 플레이어와 적의 소유 방식·전용 설정·회전·UI 차이를 유지하는 확장 지점.
	virtual AActor* GetAbilitySystemOwnerActor() const;
	virtual AActor* GetAbilitySystemAvatarActor() const;
	virtual void RestoreRotationSettingsAfterFrozen(UCharacterMovementComponent* MovementComponent);
	virtual void ApplyCurrentRotationPolicy(UCharacterMovementComponent* MovementComponent);
	virtual TSubclassOf<UUserWidget> ResolveHealthBarWidgetClass(const UWidgetClassDefinition* WidgetDefinition) const;
	virtual bool ShouldApplyResolvedHealthBarWidgetClass(UClass* CurrentWidgetClass, TSubclassOf<UUserWidget> ResolvedWidgetClass) const;
	virtual bool ShouldUseContinuousCharacterTick() const;
	virtual FVector GetDamageIndicatorWorldLocation() const;
	virtual bool IsAdditionalCharacterRuntimeContentReady() const;
	virtual void HandleCharacterRuntimeInitialized();

	// BeginPlay와 비동기 콘텐츠 준비를 합류시켜 공통 시스템을 초기화한다.
	void RefreshAbilitySystemAndTeamBindings();
	void ApplyCharacterDefinition();
	void BeginCharacterDefinitionPreload();
	void HandleCharacterDefinitionPreloaded(uint32 RequestGeneration);
	void CancelCharacterDefinitionPreload();
	void TryInitializeCharacterRuntime();
	bool IsCharacterRuntimeInitialized() const { return bCharacterRuntimeInitialized; }
	void ApplyCameraCollisionIgnoreToCharacterComponents() const;
	void ApplySkillDamageCollisionToCharacterComponents() const;
	void RefreshCharacterTickEnabled();
	void ApplyMovementSpeedFromAttribute();
	void UpdateAimOffsetForAnimation();
	void SetAimOffsetForAnimation(float AimYaw, float AimPitch);

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Character|Definition")
	TSoftObjectPtr<UCharacterBaseDefinition> CharacterDefinition;

	UPROPERTY(Transient)
	TObjectPtr<UCharacterBaseDefinition> LoadedCharacterDefinition;

	// 공통 기본 서브오브젝트. 표시·사망·장비 로직은 각 컴포넌트가 소유한다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Character|Runtime", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAbilityStateComponent> AbilityStateComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Character|Death", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCharacterDeathComponent> CharacterDeathComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Character|Presentation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCharacterPresentationComponent> CharacterPresentationComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!AbilitySystem|StatusEffect", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStatusEffectReplicationComponent> StatusEffectReplicationComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Equipment", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UEquipmentComponent> EquipmentComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Skin", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkinEquipmentComponent> SkinEquipmentComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Ability|Aura", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNiagaraComponent> BodyAuraNiagaraComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Widget", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWidgetComponent> HealthBarWidget;

	// 진영은 정의 데이터가 아니라 배치된 캐릭터의 식별 정보다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Faction")
	int32 FactionId = 0;

	// 로딩 중 애셋의 수명을 유지하고, 취소·재요청 이전의 완료 콜백을 구분한다.
	TSharedPtr<FStreamableHandle> CharacterDefinitionLoadHandle;
	uint32 CharacterDefinitionLoadGeneration = 0;
	// Super::BeginPlay 내부의 엔진 상태와 구분해, 공통 BeginPlay 설정 적용 이후만 초기화를 허용한다.
	bool bCharacterBeginPlayCalled = false;
	// 로드 성공뿐 아니라 미지정·실패 후 기본 설정으로 진행할 수 있는 상태도 포함한다.
	bool bCharacterDefinitionReady = false;
	// 공통 컴포넌트의 초기화 시작 여부다. ASC ActorInfo 준비는 별도 재시도가 이어질 수 있다.
	bool bCharacterRuntimeInitialized = false;

private:
	friend class UAbilityStateComponent;
	friend class UCharacterDeathComponent;
	friend class UCharacterHealthBarComponent;
	friend class UCharacterPresentationComponent;
};
