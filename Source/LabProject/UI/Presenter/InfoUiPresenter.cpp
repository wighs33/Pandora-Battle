#include "UI/Presenter/InfoUiPresenter.h"

#include "Component/Item/InventoryComponent.h"
#include "Data/ContentDataSubsystem.h"
#include "Definition/Common/ProjectTagConfig.h"
#include "Definition/Item/ItemDefinition.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "Item/ItemInstance.h"
#include "Mode/PdPlayerController.h"
#include "UI/InfoLoadoutStore.h"
#include "UI/Presenter/InfoItemTabPresenter.h"
#include "UI/Presenter/InfoPandoraTabPresenter.h"
#include "UI/Presenter/InfoSkinTabPresenter.h"
#include "UI/Presenter/InfoStatusTabPresenter.h"
#include "UI/Widget/InfoWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InfoUiPresenter)

UInfoUiPresenter::UInfoUiPresenter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UWorld* UInfoUiPresenter::GetWorld() const
{
	return OwningController ? OwningController->GetWorld() : Super::GetWorld();
}

void UInfoUiPresenter::Initialize(APdPlayerController* InController)
{
	if (OwningController == InController)
	{
		EnsureTabPresenters();
		BindLoadoutStateNotification();
		LoadoutStore->Initialize(InController);
		return;
	}

	Deinitialize();
	OwningController = InController;
	EnsureTabPresenters();
	StatusPresenter->Initialize(InController);
	ItemPresenter->Initialize(InController);
	SkinPresenter->Initialize(InController);
	PandoraPresenter->Initialize(InController);
	BindLoadoutStateNotification();
	LoadoutStore->Initialize(InController);
}

void UInfoUiPresenter::Deinitialize()
{
	ReleaseItemPresentationPreload();
	UnbindLoadoutStateNotification();

	if (StatusPresenter)
	{
		StatusPresenter->Deinitialize();
	}
	if (ItemPresenter)
	{
		ItemPresenter->Deinitialize();
	}
	if (SkinPresenter)
	{
		SkinPresenter->Deinitialize();
	}
	if (PandoraPresenter)
	{
		PandoraPresenter->Deinitialize();
	}
	if (LoadoutStore)
	{
		LoadoutStore->Deinitialize();
	}

	InfoWidget = nullptr;
	OwningController = nullptr;
}

void UInfoUiPresenter::BindInfoUi(UInfoWidget* InInfoWidget)
{
	EnsureTabPresenters();
	InfoWidget = InInfoWidget;
	StatusPresenter->BindInfoUi(InInfoWidget);
	ItemPresenter->BindInfoUi(InInfoWidget);
	SkinPresenter->BindInfoUi(InInfoWidget);
	PandoraPresenter->BindInfoUi(InInfoWidget);
	BindLoadoutStateNotification();
	LoadoutStore->RefreshBindings();
	BeginItemPresentationPreload();
}

void UInfoUiPresenter::UnbindInfoUi(const UInfoWidget* ExpectedInfoWidget)
{
	if (ExpectedInfoWidget && InfoWidget != ExpectedInfoWidget)
	{
		return;
	}
	ReleaseItemPresentationPreload();
	EnsureTabPresenters();
	StatusPresenter->BindInfoUi(nullptr);
	ItemPresenter->BindInfoUi(nullptr);
	SkinPresenter->BindInfoUi(nullptr);
	PandoraPresenter->BindInfoUi(nullptr);
	InfoWidget = nullptr;
}

UItemInstance* UInfoUiPresenter::GetSelectedWeapon(const EEnum_Direction Direction) const
{
	return LoadoutStore ? LoadoutStore->GetSelectedWeapon(Direction) : nullptr;
}

UPandoraInstance* UInfoUiPresenter::GetSelectedPandora(const EEnum_Direction Direction) const
{
	return LoadoutStore ? LoadoutStore->GetSelectedPandora(Direction) : nullptr;
}

bool UInfoUiPresenter::WouldSelectedPandoraDirectionChangeLoadout(
	const EEnum_Direction Direction) const
{
	return LoadoutStore
		&& LoadoutStore->WouldSelectedDirectionChangeLoadout(Direction);
}

void UInfoUiPresenter::HandleSelectedPandoraDirection(const EEnum_Direction Direction)
{
	if (LoadoutStore)
	{
		LoadoutStore->RequestSelectLoadoutDirection(Direction);
	}
}

void UInfoUiPresenter::HandleOpenedInfoUi()
{
	EnsureTabPresenters();
	BindLoadoutStateNotification();
	LoadoutStore->RefreshBindings();
	StatusPresenter->Activate();
	ItemPresenter->HandleInfoUiOpened();
	SkinPresenter->HandleInfoUiOpened();
	PandoraPresenter->HandleInfoUiOpened();
}

void UInfoUiPresenter::HandleClickedInfoCenterButton(
	const FGameplayTag LeftUiTag,
	const FGameplayTag RightUiTag)
{
	static_cast<void>(RightUiTag);
	if (LeftUiTag.IsValid())
	{
		SetActiveTab(LeftUiTag);
	}
}

void UInfoUiPresenter::EnsureTabPresenters()
{
	if (!LoadoutStore)
	{
		LoadoutStore = NewObject<UInfoLoadoutStore>(this);
	}
	if (!StatusPresenter)
	{
		StatusPresenter = NewObject<UInfoStatusTabPresenter>(this);
	}
	if (!ItemPresenter)
	{
		ItemPresenter = NewObject<UInfoItemTabPresenter>(this);
	}
	if (!SkinPresenter)
	{
		SkinPresenter = NewObject<UInfoSkinTabPresenter>(this);
	}
	if (!PandoraPresenter)
	{
		PandoraPresenter = NewObject<UInfoPandoraTabPresenter>(this);
	}

	ItemPresenter->SetLoadoutStore(LoadoutStore);
	PandoraPresenter->SetLoadoutStore(LoadoutStore);
}

void UInfoUiPresenter::SetActiveTab(const FGameplayTag LeftUiTag)
{
	EnsureTabPresenters();
	const UProjectTagConfig* Tags = UProjectTagConfig::Get(this);
	const bool bItemTab = Tags && LeftUiTag == Tags->GetUiEquipmentLeftTag();
	const bool bPandoraTab = Tags && LeftUiTag == Tags->GetUiPandoraEquipmentLeftTag();
	ItemPresenter->SetActive(bItemTab);
	PandoraPresenter->SetActive(bPandoraTab);

	if (!Tags)
	{
		return;
	}
	if (LeftUiTag == Tags->GetUiProfileLeftTag())
	{
		StatusPresenter->Activate();
	}
	else if (bItemTab)
	{
		ItemPresenter->Activate();
	}
	else if (LeftUiTag == Tags->GetUiSkinEquipmentLeftTag())
	{
		SkinPresenter->Activate();
	}
	else if (bPandoraTab)
	{
		PandoraPresenter->Activate();
	}
}

void UInfoUiPresenter::BindLoadoutStateNotification()
{
	if (!LoadoutStore || LoadoutStateChangedDelegateHandle.IsValid())
	{
		return;
	}
	LoadoutStateChangedDelegateHandle = LoadoutStore->OnStateChanged.AddUObject(
		this,
		&ThisClass::HandleLoadoutStateChanged);
}

void UInfoUiPresenter::UnbindLoadoutStateNotification()
{
	if (LoadoutStore && LoadoutStateChangedDelegateHandle.IsValid())
	{
		LoadoutStore->OnStateChanged.Remove(LoadoutStateChangedDelegateHandle);
	}
	LoadoutStateChangedDelegateHandle.Reset();
}

void UInfoUiPresenter::BeginItemPresentationPreload()
{
	ReleaseItemPresentationPreload();
	const int32 PreloadGeneration = ++ItemPresentationPreloadGeneration;
	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UContentDataSubsystem* ContentSubsystem = GameInstance
		? GameInstance->GetSubsystem<UContentDataSubsystem>()
		: nullptr;
	UInventoryComponent* Inventory = LoadoutStore
		? LoadoutStore->GetInventoryComponent()
		: nullptr;
	if (!ContentSubsystem || !Inventory)
	{
		return;
	}

	TArray<FSoftObjectPath> IconPaths;
	for (const TObjectPtr<UItemInstance>& ItemInstance : Inventory->GetAllItems().Items)
	{
		const UItemDefinition* ItemDefinition = IsValid(ItemInstance)
			? ItemInstance->ItemDefinition.Get()
			: nullptr;
		if (ItemDefinition)
		{
			IconPaths.Add(ItemDefinition->IconTexture.ToSoftObjectPath());
		}
	}

	ItemPresentationPreloadHandle = ContentSubsystem->PreloadSoftObjectPathsAsync(
		IconPaths,
		FSimpleDelegate::CreateWeakLambda(this, [this, PreloadGeneration]()
		{
			if (PreloadGeneration != ItemPresentationPreloadGeneration)
			{
				return;
			}
			if (LoadoutStore)
			{
				LoadoutStore->NotifyPresentationAssetsReady();
			}
		}));
}

void UInfoUiPresenter::ReleaseItemPresentationPreload()
{
	++ItemPresentationPreloadGeneration;
	if (ItemPresentationPreloadHandle.IsValid())
	{
		ItemPresentationPreloadHandle->CancelHandle();
		ItemPresentationPreloadHandle->ReleaseHandle();
		ItemPresentationPreloadHandle.Reset();
	}
}

void UInfoUiPresenter::HandleLoadoutStateChanged(const EInfoLoadoutStateChange Change)
{
	switch (Change)
	{
	case EInfoLoadoutStateChange::Bindings:
		if (ItemPresenter)
		{
			ItemPresenter->ResetInventoryDisplaySlots();
			ItemPresenter->HandleInventoryChanged();
		}
		if (PandoraPresenter)
		{
			PandoraPresenter->HandleInventoryChanged();
		}
		BeginItemPresentationPreload();
		break;
	case EInfoLoadoutStateChange::Inventory:
		BeginItemPresentationPreload();
		if (ItemPresenter)
		{
			ItemPresenter->HandleInventoryChanged();
		}
		if (PandoraPresenter)
		{
			PandoraPresenter->HandleInventoryChanged();
		}
		break;
	case EInfoLoadoutStateChange::WeaponLoadout:
		BeginItemPresentationPreload();
		if (ItemPresenter)
		{
			ItemPresenter->HandleWeaponLoadoutChanged();
		}
		if (PandoraPresenter)
		{
			PandoraPresenter->HandleWeaponLoadoutChanged();
		}
		break;
	case EInfoLoadoutStateChange::PandoraLoadout:
		if (ItemPresenter)
		{
			ItemPresenter->HandlePandoraLoadoutChanged();
		}
		if (PandoraPresenter)
		{
			PandoraPresenter->HandlePandoraLoadoutChanged();
		}
		break;
	case EInfoLoadoutStateChange::PresentationAssets:
		if (ItemPresenter)
		{
			ItemPresenter->HandlePresentationAssetsReady();
		}
		if (PandoraPresenter)
		{
			PandoraPresenter->HandlePresentationAssetsReady();
		}
		break;
	default:
		break;
	}
}
