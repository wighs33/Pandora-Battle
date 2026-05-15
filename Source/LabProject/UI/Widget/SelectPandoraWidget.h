#pragma once

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
	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora")
	void SetPandoraImage(int32 Nth, UTexture2D* PandoraImage);

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

private:
	void SetImageByIndex(const TArray<UImage*>& Images, int32 Nth, UTexture2D* Texture) const;
	void SelectDirection(EEnum_Direction InDirection, bool bBroadcast);
};
