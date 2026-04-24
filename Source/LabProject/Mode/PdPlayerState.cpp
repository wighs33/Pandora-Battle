// Fill out your copyright notice in the Description page of Project Settings.


#include "PdPlayerState.h"
#include "Components/GameFrameworkComponentManager.h"
#include "AbilitySystem/PdAbilitySystemComponent.h"
#include "AbilitySystem/PdAttributeSet.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(PdPlayerState)

APdPlayerState::APdPlayerState(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	// PlayerState의 기본 NetUpdateFrequency가 낮아서(1) ASC 데이터(Attribute, Tag, Effect) 동기화가 지연됩니다
	// ASC가 PlayerState에 있으므로 업데이트 빈도를 높여 즉각적인 동기화를 보장합니다
	SetNetUpdateFrequency(100.f);

	AbilitySystemComponent = CreateDefaultSubobject<UPdAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AttributeSet = CreateDefaultSubobject<UPdAttributeSet>(TEXT("AttributeSet"));
}

void APdPlayerState::PreInitializeComponents()
{
	Super::PreInitializeComponents();
	UGameFrameworkComponentManager::AddGameFrameworkComponentReceiver(this);
}

void APdPlayerState::BeginPlay()
{
	Super::BeginPlay();
	UGameFrameworkComponentManager::SendGameFrameworkComponentExtensionEvent(this, UGameFrameworkComponentManager::NAME_GameActorReady);
}

void APdPlayerState::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UGameFrameworkComponentManager::RemoveGameFrameworkComponentReceiver(this);
	Super::EndPlay(EndPlayReason);
}


UAbilitySystemComponent* APdPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}
