#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "UObject/PrimaryAssetId.h"
#include "ControllerInputDefinition.generated.h"

class UInputAction;
class UInputMappingContext;

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
	const TSoftObjectPtr<UInputAction>& GetSkill1InputAction() const { return Skill1InputAction; }
	const TSoftObjectPtr<UInputAction>& GetSkill2InputAction() const { return Skill2InputAction; }
	const TSoftObjectPtr<UInputAction>& GetSkill3InputAction() const { return Skill3InputAction; }
	const TSoftObjectPtr<UInputAction>& GetSkill4InputAction() const { return Skill4InputAction; }
	const TSoftObjectPtr<UInputAction>& GetTargetConfirmInputAction() const { return TargetConfirmInputAction; }
	const TSoftObjectPtr<UInputAction>& GetOpenInfoUiInputAction() const { return OpenInfoUiInputAction; }
	const TSoftObjectPtr<UInputAction>& GetSelectPandoraInputAction() const { return SelectPandoraInputAction; }
	const TSoftObjectPtr<UInputAction>& GetPandoraTreeInputAction() const { return PandoraTreeInputAction; }
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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Input|Ability Actions", meta = (AssetBundles = "Client", AllowPrivateAccess = "true"))
	TSoftObjectPtr<UInputAction> Skill1InputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Input|Ability Actions", meta = (AssetBundles = "Client", AllowPrivateAccess = "true"))
	TSoftObjectPtr<UInputAction> Skill2InputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Input|Ability Actions", meta = (AssetBundles = "Client", AllowPrivateAccess = "true"))
	TSoftObjectPtr<UInputAction> Skill3InputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Input|Ability Actions", meta = (AssetBundles = "Client", AllowPrivateAccess = "true"))
	TSoftObjectPtr<UInputAction> Skill4InputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Input|Ability Actions", meta = (AssetBundles = "Client", AllowPrivateAccess = "true"))
	TSoftObjectPtr<UInputAction> TargetConfirmInputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Input|Native Actions", meta = (AssetBundles = "Client", AllowPrivateAccess = "true"))
	TSoftObjectPtr<UInputAction> OpenInfoUiInputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Input|Native Actions", meta = (AssetBundles = "Client", AllowPrivateAccess = "true"))
	TSoftObjectPtr<UInputAction> SelectPandoraInputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Input|Native Actions", meta = (AssetBundles = "Client", AllowPrivateAccess = "true"))
	TSoftObjectPtr<UInputAction> PandoraTreeInputAction;

	//------------------------------------------------------------------------------------------------------------------
	//--- Block State
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Input|Block State", meta = (Categories = "State", AllowPrivateAccess = "true"))
	FGameplayTag MovementBlockStateTag;
};
