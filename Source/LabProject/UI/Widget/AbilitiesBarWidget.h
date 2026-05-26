#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayTagContainer.h"
#include "AbilitiesBarWidget.generated.h"

class UAbilitySystemComponent;
class UGameplayAbility;
class UHorizontalBox;
class UItemDefinition;
class UPandoraDefinition;
class UPandoraTreeComponent;
class UPdAbilitySystemComponent;
class UUserWidget;
struct FSkill;
struct FGameplayEventData;

UCLASS(BlueprintType, Blueprintable)
class LABPROJECT_API UAbilitiesBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "!UI|Abilities")
	TArray<FGameplayAbilitySpecHandle> GetAbilitiesToShowInBar() const;

	UFUNCTION(BlueprintCallable, Category = "!UI|Abilities")
	void FillAbilitiesBar();

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	struct FAbilityBarSlotData
	{
		FGameplayAbilitySpecHandle AbilitySpecHandle;
		FText DisplayNameOverride;
		UObject* IconOverride = nullptr;
		bool bHasDisplayOverride = false;
		bool bEnabled = true;
	};

	void InitializeAbilitySystemBinding();
	void RebuildAbilitiesBar();
	void AddAbilitySlot(const FGameplayAbilitySpecHandle& AbilitySpecHandle);
	void AddAbilitySlot(const FAbilityBarSlotData& SlotData);
	void AddEmptySlot(bool bApplyPadding);
	UUserWidget* CreateBarWidget(TSubclassOf<UUserWidget> WidgetClass) const;
	void AddWidgetToBar(UUserWidget* Widget, bool bApplyPadding) const;
	void SetAbilitySpecHandleOnWidget(UUserWidget* Widget, const FGameplayAbilitySpecHandle& AbilitySpecHandle) const;
	bool ShouldShowAbilityHandle(UAbilitySystemComponent* AbilitySystemComponent, const FGameplayAbilitySpecHandle& AbilitySpecHandle) const;
	UAbilitySystemComponent* GetOwningAbilitySystemComponent() const;
	const UPandoraDefinition* GetSelectedPandoraDefinition() const;
	UPandoraTreeComponent* GetPandoraTreeComponent() const;
	int32 GetSelectedPandoraLevel(const UPandoraDefinition* PandoraDefinition) const;
	bool IsSelectedPandoraCompatibleWithCurrentWeapon() const;
	const UItemDefinition* GetCurrentWeaponDefinition() const;
	FGameplayAbilitySpecHandle FindAbilitySpecHandleForSkill(
		UAbilitySystemComponent* AbilitySystemComponent,
		const UPandoraDefinition* PandoraDefinition,
		int32 SkillIndex) const;
	bool IsConfiguredPandoraSkill(const FSkill& Skill) const;

	void BindAbilitiesChangedEvents();
	void UnbindAbilitiesChangedEvents();
	void HandleAbilitiesChanged();
	void HandleAbilitiesChangedEvent(const FGameplayEventData* Payload);

	static FProperty* FindPropertyByExactNameOrPrefix(UStruct* Struct, FName ExactName, const FString& Prefix);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Abilities", meta = (AllowPrivateAccess = "true", ClampMin = "0"))
	int32 MinimumSlots = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Abilities", meta = (AllowPrivateAccess = "true", ClampMin = "0"))
	int32 PandoraSkillSlots = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Abilities|Classes", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UUserWidget> AbilityWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Abilities|Classes", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UUserWidget> EmptyAbilityWidgetClass;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> ContainerHorizontalBox;

	TWeakObjectPtr<UAbilitySystemComponent> CachedAbilitySystemComponent;
	FDelegateHandle AbilitiesChangedEventHandle;
	FDelegateHandle AbilitiesChangedNativeHandle;
	FTimerHandle RebuildBarTimerHandle;
	FTimerHandle RetryInitializeTimerHandle;
};
