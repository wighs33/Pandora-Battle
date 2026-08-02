#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "UObject/PrimaryAssetId.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "ControllerInputDefinition.generated.h"

class UInputAction;
class UInputMappingContext;
class UCharacterActionDefinition;

USTRUCT(BlueprintType)
struct LABPROJECT_API FPdInputActionIconMapping
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Input|Icon", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<UInputAction> InputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Input|Icon",
		meta = (AssetBundles = "Client", AllowedClasses = "/Script/Engine.Texture2D,/Script/Engine.MaterialInterface"))
	TSoftObjectPtr<UObject> Icon;
};

UCLASS(BlueprintType, Const)
class LABPROJECT_API UControllerInputDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UControllerInputDefinition();

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	static FSoftObjectPath GetDefaultInputDefinitionPath();

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
	const TSoftObjectPtr<UInputAction>& GetPaintInputAction() const { return PaintInputAction; }
	const TSoftObjectPtr<UInputAction>& GetGrappleInputAction() const { return GrappleInputAction; }
	const TSoftObjectPtr<UInputAction>& GetSkill1InputAction() const { return Skill1InputAction; }
	const TSoftObjectPtr<UInputAction>& GetSkill2InputAction() const { return Skill2InputAction; }
	const TSoftObjectPtr<UInputAction>& GetSkill3InputAction() const { return Skill3InputAction; }
	const TSoftObjectPtr<UInputAction>& GetSkill4InputAction() const { return Skill4InputAction; }
	const TSoftObjectPtr<UInputAction>& GetQuickSlot1InputAction() const { return QuickSlot1InputAction; }
	const TSoftObjectPtr<UInputAction>& GetQuickSlot2InputAction() const { return QuickSlot2InputAction; }
	const TSoftObjectPtr<UInputAction>& GetQuickSlot3InputAction() const { return QuickSlot3InputAction; }
	const TSoftObjectPtr<UInputAction>& GetQuickSlot4InputAction() const { return QuickSlot4InputAction; }
	const TSoftObjectPtr<UInputAction>& GetGesture1InputAction() const { return Gesture1InputAction; }
	const TSoftObjectPtr<UInputAction>& GetGesture2InputAction() const { return Gesture2InputAction; }
	const TSoftObjectPtr<UInputAction>& GetGesture3InputAction() const { return Gesture3InputAction; }
	const TSoftObjectPtr<UInputAction>& GetGesture4InputAction() const { return Gesture4InputAction; }
	const TSoftObjectPtr<UInputAction>& GetTargetConfirmInputAction() const { return TargetConfirmInputAction; }
	const TSoftObjectPtr<UInputAction>& GetOpenInfoProfileInputAction() const { return OpenInfoProfileInputAction; }
	const TSoftObjectPtr<UInputAction>& GetOpenInfoItemInputAction() const { return OpenInfoItemInputAction; }
	const TSoftObjectPtr<UInputAction>& GetOpenInfoSkinInputAction() const { return OpenInfoSkinInputAction; }
	const TSoftObjectPtr<UInputAction>& GetOpenInfoPandoraInputAction() const { return OpenInfoPandoraInputAction; }
	const TSoftObjectPtr<UInputAction>& GetOpenInfoMapInputAction() const { return OpenInfoMapInputAction; }
	const TSoftObjectPtr<UInputAction>& GetOpenSettingUiInputAction() const { return OpenSettingUiInputAction; }
	const TSoftObjectPtr<UInputAction>& GetEscapeInputAction() const { return EscapeInputAction; }
	const TSoftObjectPtr<UInputAction>& GetOpenLobbyInputAction() const { return OpenLobbyInputAction; }
	const TSoftObjectPtr<UInputAction>& GetSelectPandoraInputAction() const { return SelectPandoraInputAction; }
	const TSoftObjectPtr<UInputAction>& GetPandoraTreeInputAction() const { return PandoraTreeInputAction; }
	const TSoftObjectPtr<UCharacterActionDefinition>& GetCharacterActionDefinition() const { return CharacterActionDefinition; }
	const TArray<FPdInputActionIconMapping>& GetInputActionIconMappings() const { return InputActionIconMappings; }
	const TArray<FName>& GetOpenLobbyAllowedMapNames() const { return OpenLobbyAllowedMapNames; }
	bool IsOpenLobbyInputAllowedForMap(const FString& LevelName) const;
	UObject* ResolveInputActionIconObject(const UInputAction* InputAction) const;
	void GetRuntimePreloadAssetPaths(TArray<FSoftObjectPath>& OutAssetPaths) const;
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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Input|Paint", meta = (AssetBundles = "Client", AllowPrivateAccess = "true", DisplayName = "IA Paint"))
	TSoftObjectPtr<UInputAction> PaintInputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Input|Native Actions", meta = (AssetBundles = "Client", AllowPrivateAccess = "true"))
	TSoftObjectPtr<UInputAction> GrappleInputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Input|Ability Actions", meta = (AssetBundles = "Client", AllowPrivateAccess = "true"))
	TSoftObjectPtr<UInputAction> Skill1InputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Input|Ability Actions", meta = (AssetBundles = "Client", AllowPrivateAccess = "true"))
	TSoftObjectPtr<UInputAction> Skill2InputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Input|Ability Actions", meta = (AssetBundles = "Client", AllowPrivateAccess = "true"))
	TSoftObjectPtr<UInputAction> Skill3InputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Input|Ability Actions", meta = (AssetBundles = "Client", AllowPrivateAccess = "true"))
	TSoftObjectPtr<UInputAction> Skill4InputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Input|Quick Slots", meta = (AssetBundles = "Client", AllowPrivateAccess = "true"))
	TSoftObjectPtr<UInputAction> QuickSlot1InputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Input|Quick Slots", meta = (AssetBundles = "Client", AllowPrivateAccess = "true"))
	TSoftObjectPtr<UInputAction> QuickSlot2InputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Input|Quick Slots", meta = (AssetBundles = "Client", AllowPrivateAccess = "true"))
	TSoftObjectPtr<UInputAction> QuickSlot3InputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Input|Quick Slots", meta = (AssetBundles = "Client", AllowPrivateAccess = "true"))
	TSoftObjectPtr<UInputAction> QuickSlot4InputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Input|Gesture Actions", meta = (AssetBundles = "Client", AllowPrivateAccess = "true"))
	TSoftObjectPtr<UInputAction> Gesture1InputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Input|Gesture Actions", meta = (AssetBundles = "Client", AllowPrivateAccess = "true"))
	TSoftObjectPtr<UInputAction> Gesture2InputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Input|Gesture Actions", meta = (AssetBundles = "Client", AllowPrivateAccess = "true"))
	TSoftObjectPtr<UInputAction> Gesture3InputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Input|Gesture Actions", meta = (AssetBundles = "Client", AllowPrivateAccess = "true"))
	TSoftObjectPtr<UInputAction> Gesture4InputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Input|Ability Actions", meta = (AssetBundles = "Client", AllowPrivateAccess = "true"))
	TSoftObjectPtr<UInputAction> TargetConfirmInputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Input|Info Hub", meta = (AssetBundles = "Client", AllowPrivateAccess = "true", DisplayName = "Open Profile Input Action"))
	TSoftObjectPtr<UInputAction> OpenInfoProfileInputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Input|Info Hub", meta = (AssetBundles = "Client", AllowPrivateAccess = "true", DisplayName = "Open Item Input Action"))
	TSoftObjectPtr<UInputAction> OpenInfoItemInputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Input|Info Hub", meta = (AssetBundles = "Client", AllowPrivateAccess = "true", DisplayName = "Open Skin Input Action"))
	TSoftObjectPtr<UInputAction> OpenInfoSkinInputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Input|Info Hub", meta = (AssetBundles = "Client", AllowPrivateAccess = "true", DisplayName = "Open Pandora Input Action"))
	TSoftObjectPtr<UInputAction> OpenInfoPandoraInputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Input|Info Hub", meta = (AssetBundles = "Client", AllowPrivateAccess = "true", DisplayName = "Open Map Input Action"))
	TSoftObjectPtr<UInputAction> OpenInfoMapInputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Input|Native Actions", meta = (AssetBundles = "Client", AllowPrivateAccess = "true"))
	TSoftObjectPtr<UInputAction> OpenSettingUiInputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Input|Native Actions",
		meta = (AssetBundles = "Client", AllowPrivateAccess = "true", DisplayName = "Escape Input Action"))
	TSoftObjectPtr<UInputAction> EscapeInputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Input|Native Actions", meta = (AssetBundles = "Client", AllowPrivateAccess = "true"))
	TSoftObjectPtr<UInputAction> OpenLobbyInputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Input|Native Actions", meta = (AllowPrivateAccess = "true"))
	TArray<FName> OpenLobbyAllowedMapNames = { TEXT("LV_Lobby") };

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Input|Native Actions", meta = (AssetBundles = "Client", AllowPrivateAccess = "true"))
	TSoftObjectPtr<UInputAction> SelectPandoraInputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Input|Native Actions", meta = (AssetBundles = "Client", AllowPrivateAccess = "true"))
	TSoftObjectPtr<UInputAction> PandoraTreeInputAction;

	//------------------------------------------------------------------------------------------------------------------
	//--- Character Actions
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Input|Character Actions", meta = (AssetBundles = "Client", AllowPrivateAccess = "true"))
	TSoftObjectPtr<UCharacterActionDefinition> CharacterActionDefinition;

	//------------------------------------------------------------------------------------------------------------------
	//--- Input Icons
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Input|Icons", meta = (TitleProperty = "InputAction", AllowPrivateAccess = "true"))
	TArray<FPdInputActionIconMapping> InputActionIconMappings;

	//------------------------------------------------------------------------------------------------------------------
	//--- Block State
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Input|Block State", meta = (Categories = "State", AllowPrivateAccess = "true"))
	FGameplayTag MovementBlockStateTag;
};
