// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "AbilitySystemInterface.h"
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "PdCharacterBase.generated.h"

class UAbilitySystemComponent;
class UAnimInstance;
class UCombatComponent;
class UGameplayAbility;
class UEquipmentComponent;
class UHealthBarViewModel;
class UPdAbilitySystemComponent;
class UUserWidget;
class UWidgetComponent;

/** 캐릭터 로그 카테고리입니다. */
DECLARE_LOG_CATEGORY_EXTERN(PdCharacterBaseLog, Log, All);

/**
 * <공용 캐릭터 베이스>
 * - ASC 초기화를 담당합니다.
 * - 기본 Ability 지급을 담당합니다.
 * - 체력바 ViewModel과 애님 레이어를 관리합니다.
 */
UCLASS()
class LABPROJECT_API APdCharacterBase : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	/** 캐릭터 기본 상태를 초기화합니다. */
	APdCharacterBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 컴포넌트 초기화 전에 리시버를 등록합니다. */
	virtual void PreInitializeComponents() override;

	/** 시작 시 초기 상태를 구성합니다. */
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** 종료 시 바인딩과 리시버를 정리합니다. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 복제 프로퍼티를 등록합니다. */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 빙의 시 ASC를 초기화하고 기본 Ability를 지급합니다. */
	virtual void PossessedBy(AController* NewController) override;

	/** PlayerState 복제 후 ASC를 다시 초기화합니다. */
	virtual void OnRep_PlayerState() override;

	/** 빙의 해제 시 ASC와 UI 바인딩을 정리합니다. */
	virtual void UnPossessed() override;

	/** 현재 캐릭터의 ASC를 반환합니다. */
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	/** 프로젝트 전용 ASC를 반환합니다. */
	UFUNCTION(BlueprintPure, Category = "!AbilitySystem")
	UPdAbilitySystemComponent* GetPdAbilitySystemComponent() const;

	/** 진영 ID를 반환합니다. */
	UFUNCTION(BlueprintPure, Category = "!Faction")
	int32 GetFactionId() const;

	/** 장비 컴포넌트를 반환합니다. */
	UFUNCTION(BlueprintPure, Category = "!Equipment")
	UEquipmentComponent* GetEquipmentComponent() const { return EquipmentComponent; }

	/** 전투 컴포넌트를 반환합니다. */
	UFUNCTION(BlueprintPure, Category = "!Combat")
	UCombatComponent* GetCombatComponent() const { return CombatComponent; }

	/** 체력바 ViewModel을 새로 고칩니다. */
	UFUNCTION(BlueprintCallable, Category = "!ViewModel")
	void RefreshHealthBarViewModel();

	/** 지정 위젯에 체력바 ViewModel을 적용합니다. */
	UFUNCTION(BlueprintCallable, Category = "!ViewModel")
	void ApplyHealthBarViewModelToWidget(UUserWidget* InWidget);

	/** 체력바 ViewModel을 반환합니다. */
	UFUNCTION(BlueprintPure, Category = "!ViewModel")
	UHealthBarViewModel* GetHealthBarViewModel() const { return HealthBarViewModel; }

	/** 기본 Ability를 지급합니다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "!AbilitySystem|Grant")
	int32 GiveDefaultAbilities();

	/** 기본 애님 레이어로 되돌립니다. */
	UFUNCTION(BlueprintCallable, Category = "!Animation")
	void ResetAnimationToDefault();

	/** 현재 애님 레이어를 설정합니다. */
	UFUNCTION(BlueprintCallable, Category = "!Animation")
	void SetCurrentAnimLayer(TSubclassOf<UAnimInstance> AnimLayerClass);

	/** 지정 애님 레이어를 메시에 연결합니다. */
	void LinkAnimLayer(TSubclassOf<UAnimInstance> AnimLayerClass) const;

	/** 서버에서 사망 처리를 수행합니다. */
	void HandleDeathAuth();

protected:
	/** ASC ActorInfo를 초기화합니다. */
	virtual void InitializeAbilitySystemActorInfo();

	/** ASC ActorInfo를 정리합니다. */
	virtual void ClearAbilitySystemActorInfo();

	/** ASC OwnerActor를 반환합니다. */
	virtual AActor* GetAbilitySystemOwnerActor() const;

	/** ASC AvatarActor를 반환합니다. */
	virtual AActor* GetAbilitySystemAvatarActor() const;

	/** 체력바 위젯 컴포넌트에 ViewModel을 적용합니다. */
	void ApplyHealthBarViewModelToWidget();

	/** 체력바 ViewModel을 ASC에 바인딩합니다. */
	void BindHealthBarViewModelToASC(UAbilitySystemComponent* InASC);
	void UpdateHealthBarFacing();

	/** 현재 애님 레이어 복제 후처리를 수행합니다. */
	UFUNCTION()
	void OnRep_CurrentAnimLayer();

protected:
	/** 시작 시 지급할 기본 Ability 목록입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!AbilitySystem|Grant")
	TArray<TSubclassOf<UGameplayAbility>> DefaultAbilities;

	/** 기본 애님 레이어입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Animation")
	TSubclassOf<UAnimInstance> DefaultAnimLayer;

	/** 현재 애님 레이어입니다. */
	UPROPERTY(ReplicatedUsing = OnRep_CurrentAnimLayer, Transient, BlueprintReadOnly, Category = "!Animation")
	TSubclassOf<UAnimInstance> CurrentAnimLayer;

	/** 장비 컴포넌트입니다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Equipment", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UEquipmentComponent> EquipmentComponent;

	/** 전투 컴포넌트입니다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Combat", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCombatComponent> CombatComponent;

	/** 체력바 위젯 컴포넌트입니다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Widget", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWidgetComponent> HealthBarWidget;

	/** 체력바 ViewModel입니다. */
	UPROPERTY(Transient)
	TObjectPtr<UHealthBarViewModel> HealthBarViewModel;

	/** 진영 ID입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Faction")
	int32 FactionId = 0;
};
