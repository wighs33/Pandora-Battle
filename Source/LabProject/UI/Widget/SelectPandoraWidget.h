#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Common/Enum_Direction.h"
#include "SelectPandoraWidget.generated.h"

class UImage;
class UTexture2D;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPdOnSelectedPandoraDirection, EEnum_Direction, Direction);

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API USelectPandoraWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora")
	void SetPandoraImage(int32 Nth, UTexture2D* PandoraImage);

	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora")
	void SetPandoraEnabled(int32 Nth, bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora")
	void SetWeaponImage(int32 Nth, UTexture2D* WeaponImage);

	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora")
	void SetDirection(int32 Index);

	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "!UI|Pandora")
	FPdOnSelectedPandoraDirection OnSelected;

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Pandora|Bind")
	TObjectPtr<UImage> FirstPandoraImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Pandora|Bind")
	TObjectPtr<UImage> SecondPandoraImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Pandora|Bind")
	TObjectPtr<UImage> ThirdPandoraImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Pandora|Bind")
	TObjectPtr<UImage> FirstWeaponImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Pandora|Bind")
	TObjectPtr<UImage> SecondWeaponImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Pandora|Bind")
	TObjectPtr<UImage> ThirdWeaponImage;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!UI|Pandora")
	EEnum_Direction Direction = EEnum_Direction::Center;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Pandora|Style", meta = (AllowPrivateAccess = "true"))
	FLinearColor EnabledPandoraTint = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Pandora|Style", meta = (AllowPrivateAccess = "true"))
	FLinearColor DisabledPandoraTint = FLinearColor(0.15f, 0.15f, 0.15f, 0.55f);

private:
	void CacheDefaultImageBrushes();
	void SetImageByIndex(const TArray<UImage*>& Images, const TArray<FSlateBrush>& DefaultBrushes, int32 Nth, UTexture2D* Texture);
	void SetImageTintByIndex(const TArray<UImage*>& Images, int32 Nth, const FLinearColor& TintColor) const;
	void SelectDirection(EEnum_Direction InDirection, bool bBroadcast);

	UPROPERTY(Transient)
	TArray<FSlateBrush> DefaultPandoraImageBrushes;

	UPROPERTY(Transient)
	TArray<FSlateBrush> DefaultWeaponImageBrushes;

	UPROPERTY(Transient)
	bool bDefaultImageBrushesCached = false;
};
