#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "UObject/PrimaryAssetId.h"
#include "ControllerInputDefinition.generated.h"

class UInputAction;
class UInputMappingContext;

USTRUCT(BlueprintType)
struct FPdAbilityInputAction
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Input", meta = (Categories = "Input"))
	FGameplayTag InputTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Input", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<UInputAction> InputAction;

	bool IsValid() const
	{
		return InputTag.IsValid() && !InputAction.IsNull();
	}
};

UCLASS(BlueprintType, Const)
class LABPROJECT_API UControllerInputDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	const TSoftObjectPtr<UInputMappingContext>& GetInputMapping() const { return InputMapping; }
	int32 GetPriority() const { return Priority; }
	const TSoftObjectPtr<UInputAction>& GetMoveInputAction() const { return MoveInputAction; }
	const TSoftObjectPtr<UInputAction>& GetLookInputAction() const { return LookInputAction; }
	const TSoftObjectPtr<UInputAction>& GetJumpInputAction() const { return JumpInputAction; }
	const TSoftObjectPtr<UInputAction>& GetCrouchInputAction() const { return CrouchInputAction; }
	const TSoftObjectPtr<UInputAction>& GetInteractInputAction() const { return InteractInputAction; }
	const TSoftObjectPtr<UInputAction>& GetAttackInputAction() const { return AttackInputAction; }
	const TSoftObjectPtr<UInputAction>& GetAimInputAction() const { return AimInputAction; }
	const TSoftObjectPtr<UInputAction>& GetOpenInfoUiInputAction() const { return OpenInfoUiInputAction; }
	const TSoftObjectPtr<UInputAction>& GetSelectPandoraInputAction() const { return SelectPandoraInputAction; }
	const TArray<FPdAbilityInputAction>& GetAbilityInputActions() const { return AbilityInputActions; }
	const FGameplayTag& GetMovementBlockStateTag() const { return MovementBlockStateTag; }

private:
	//------------------------------------------------------------------------------------------------------------------
	//--- Input Mapping
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Input|Mapping", meta = (AssetBundles = "Client", AllowPrivateAccess = "true"))
	TSoftObjectPtr<UInputMappingContext> InputMapping;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Input|Mapping", meta = (AllowPrivateAccess = "true"))
	int32 Priority = 0;

	//------------------------------------------------------------------------------------------------------------------
	//--- Native Input Actions
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Input|Native Actions", meta = (AssetBundles = "Client", AllowPrivateAccess = "true"))
	TSoftObjectPtr<UInputAction> MoveInputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Input|Native Actions", meta = (AssetBundles = "Client", AllowPrivateAccess = "true"))
	TSoftObjectPtr<UInputAction> LookInputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Input|Native Actions", meta = (AssetBundles = "Client", AllowPrivateAccess = "true"))
	TSoftObjectPtr<UInputAction> JumpInputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Input|Native Actions", meta = (AssetBundles = "Client", AllowPrivateAccess = "true"))
	TSoftObjectPtr<UInputAction> CrouchInputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Input|Native Actions", meta = (AssetBundles = "Client", AllowPrivateAccess = "true"))
	TSoftObjectPtr<UInputAction> InteractInputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Input|Native Actions", meta = (AssetBundles = "Client", AllowPrivateAccess = "true"))
	TSoftObjectPtr<UInputAction> AttackInputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Input|Native Actions", meta = (AssetBundles = "Client", AllowPrivateAccess = "true"))
	TSoftObjectPtr<UInputAction> AimInputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Input|Native Actions", meta = (AssetBundles = "Client", AllowPrivateAccess = "true"))
	TSoftObjectPtr<UInputAction> OpenInfoUiInputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Input|Native Actions", meta = (AssetBundles = "Client", AllowPrivateAccess = "true"))
	TSoftObjectPtr<UInputAction> SelectPandoraInputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Input|Actions", meta = (TitleProperty = "InputTag", AllowPrivateAccess = "true"))
	TArray<FPdAbilityInputAction> AbilityInputActions;

	//------------------------------------------------------------------------------------------------------------------
	//--- Block State
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Input|Block State", meta = (Categories = "State", AllowPrivateAccess = "true"))
	FGameplayTag MovementBlockStateTag;
};
