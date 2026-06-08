#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "WidgetClassDefinition.generated.h"

class UInfoWidget;
class UInfoUiPresenter;
class USelectPandoraWidget;
class UPandoraTreeWidget;
class URightNotificationsWidget;

USTRUCT(BlueprintType)
struct LABPROJECT_API FPlayerHudWidgetSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|PlayerHUD", meta = (Categories = "UI.Widget"))
	FGameplayTag WidgetTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|PlayerHUD")
	TSubclassOf<UUserWidget> WidgetClass;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FInfoWidgetSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|InfoWidget", meta = (Categories = "UI.Widget"))
	FGameplayTag WidgetTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|InfoWidget")
	TSubclassOf<UInfoWidget> WidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|InfoWidget")
	TSubclassOf<UInfoUiPresenter> PresenterClass;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FSelectPandoraWidgetSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|SelectPandoraWidget", meta = (Categories = "UI.Widget"))
	FGameplayTag WidgetTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|SelectPandoraWidget")
	TSubclassOf<USelectPandoraWidget> WidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|SelectPandoraWidget", meta = (ClampMin = "0.0"))
	double DeadZoneRadius = 0.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|SelectPandoraWidget", meta = (ClampMin = "0.001"))
	double SegmentAngle = 90.0;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FAimCrosshairWidgetSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|AimCrosshairWidget", meta = (Categories = "UI.Widget"))
	FGameplayTag WidgetTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|AimCrosshairWidget")
	TSubclassOf<UUserWidget> WidgetClass;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FPandoraTreeWidgetSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|PandoraTreeWidget", meta = (Categories = "UI.Widget"))
	FGameplayTag WidgetTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|PandoraTreeWidget")
	TSubclassOf<UPandoraTreeWidget> WidgetClass;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FRightNotificationsWidgetSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|RightNotificationsWidget")
	TSubclassOf<URightNotificationsWidget> WidgetClass;
};

UCLASS(BlueprintType, Blueprintable, meta = (DisplayName = "Widget Class Definition"))
class LABPROJECT_API UWidgetClassDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UWidgetClassDefinition();

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	UFUNCTION(BlueprintPure, Category = "!UI|Widget")
	TSubclassOf<UUserWidget> FindWidgetClassByTag(FGameplayTag WidgetTag) const;

	TSubclassOf<UUserWidget> GetPlayerHudWidgetClass() const;
	TSubclassOf<UInfoWidget> GetInfoWidgetClass() const;
	TSubclassOf<USelectPandoraWidget> GetSelectPandoraWidgetClass() const;
	TSubclassOf<UUserWidget> GetAimCrosshairWidgetClass() const;
	TSubclassOf<UPandoraTreeWidget> GetPandoraTreeWidgetClass() const;
	TSubclassOf<URightNotificationsWidget> GetRightNotificationsWidgetClass() const;
	const FPlayerHudWidgetSettings& GetPlayerHudWidgetSettings() const { return PlayerHudWidgetSettings; }
	const FInfoWidgetSettings& GetInfoWidgetSettings() const { return InfoWidgetSettings; }
	const FSelectPandoraWidgetSettings& GetSelectPandoraWidgetSettings() const { return SelectPandoraWidgetSettings; }
	const FAimCrosshairWidgetSettings& GetAimCrosshairWidgetSettings() const { return AimCrosshairWidgetSettings; }
	const FPandoraTreeWidgetSettings& GetPandoraTreeWidgetSettings() const { return PandoraTreeWidgetSettings; }
	const FRightNotificationsWidgetSettings& GetRightNotificationsWidgetSettings() const { return RightNotificationsWidgetSettings; }

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|PlayerHUD", meta = (AllowPrivateAccess = "true"))
	FPlayerHudWidgetSettings PlayerHudWidgetSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|InfoWidget", meta = (AllowPrivateAccess = "true"))
	FInfoWidgetSettings InfoWidgetSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|SelectPandoraWidget", meta = (AllowPrivateAccess = "true"))
	FSelectPandoraWidgetSettings SelectPandoraWidgetSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|AimCrosshairWidget", meta = (AllowPrivateAccess = "true"))
	FAimCrosshairWidgetSettings AimCrosshairWidgetSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|PandoraTreeWidget", meta = (AllowPrivateAccess = "true"))
	FPandoraTreeWidgetSettings PandoraTreeWidgetSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|RightNotificationsWidget", meta = (AllowPrivateAccess = "true"))
	FRightNotificationsWidgetSettings RightNotificationsWidgetSettings;
};
