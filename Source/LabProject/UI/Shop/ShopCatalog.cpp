#include "UI/Shop/ShopCatalog.h"

#include "Data/ContentDataSubsystem.h"
#include "Definition/Item/ItemDefinition.h"
#include "Definition/Pandora/PandoraDefinition.h"
#include "Definition/Skin/SkinDefinition.h"

namespace
{
	FString MakeProductKey(const FShopCatalogProductReference& ProductReference)
	{
		FSoftObjectPath ObjectPath;
		switch (ProductReference.ProductType)
		{
		case EShopProductType::Pandora:
			ObjectPath = ProductReference.PandoraDefinition.ToSoftObjectPath();
			break;
		case EShopProductType::Skin:
			ObjectPath = ProductReference.SkinDefinition.ToSoftObjectPath();
			break;
		case EShopProductType::Item:
			ObjectPath = ProductReference.ItemDefinition.ToSoftObjectPath();
			break;
		default:
			break;
		}

		if (!ObjectPath.IsValid())
		{
			return FString();
		}

		return FString::Printf(TEXT("%d:%s"), static_cast<int32>(ProductReference.ProductType), *ObjectPath.ToString());
	}

	FShopProductDefinitionData ResolveShopData(const UObject* ProductObject, const EShopProductType ProductType)
	{
		switch (ProductType)
		{
		case EShopProductType::Pandora:
			if (const UPandoraDefinition* PandoraDefinition = Cast<UPandoraDefinition>(ProductObject))
			{
				return PandoraDefinition->GetShopData();
			}
			break;
		case EShopProductType::Skin:
			if (const USkinDefinition* SkinDefinition = Cast<USkinDefinition>(ProductObject))
			{
				return SkinDefinition->ShopData;
			}
			break;
		case EShopProductType::Item:
			if (const UItemDefinition* ItemDefinition = Cast<UItemDefinition>(ProductObject))
			{
				return ItemDefinition->ShopData;
			}
			break;
		default:
			break;
		}

		return FShopProductDefinitionData();
	}

	FString GetProductSortName(const UObject* ProductObject)
	{
		if (const UPandoraDefinition* PandoraDefinition = Cast<UPandoraDefinition>(ProductObject))
		{
			return PandoraDefinition->GetDisplayName().ToString();
		}

		if (const USkinDefinition* SkinDefinition = Cast<USkinDefinition>(ProductObject))
		{
			return SkinDefinition->DisplayName.ToString();
		}

		if (const UItemDefinition* ItemDefinition = Cast<UItemDefinition>(ProductObject))
		{
			return ItemDefinition->DisplayName.ToString();
		}

		return GetNameSafe(ProductObject);
	}

	FShopCatalogProductReference MakeProductReference(UPandoraDefinition* PandoraDefinition)
	{
		FShopCatalogProductReference ProductReference;
		ProductReference.ProductType = EShopProductType::Pandora;
		ProductReference.PandoraDefinition = PandoraDefinition;
		return ProductReference;
	}

	FShopCatalogProductReference MakeProductReference(USkinDefinition* SkinDefinition)
	{
		FShopCatalogProductReference ProductReference;
		ProductReference.ProductType = EShopProductType::Skin;
		ProductReference.SkinDefinition = SkinDefinition;
		return ProductReference;
	}

	class FShopCatalogBuilder
	{
	public:
		void Add(const FShopCatalogProductReference& ProductReference)
		{
			if (!ProductReference.HasValidProduct())
			{
				return;
			}

			const FString ProductKey = MakeProductKey(ProductReference);
			if (ProductKey.IsEmpty() || SeenProductKeys.Contains(ProductKey))
			{
				return;
			}

			const UObject* ProductObject = PdShopCatalog::ResolveProductObject(ProductReference);
			if (!ProductObject)
			{
				return;
			}

			SeenProductKeys.Add(ProductKey);
			FShopCatalogEntry& CatalogEntry = Entries.AddDefaulted_GetRef();
			CatalogEntry.Product = ProductReference;
			CatalogEntry.ShopData = ResolveShopData(ProductObject, ProductReference.ProductType);
		}

		// 불러온 정의를 표시 이름 순으로 넣는다.
		template <typename DefinitionType>
		void AddAllLoaded(const TMap<FName, TObjectPtr<DefinitionType>>& LoadedDefinitionsByName)
		{
			TArray<DefinitionType*> Definitions;
			for (const TPair<FName, TObjectPtr<DefinitionType>>& DefinitionPair : LoadedDefinitionsByName)
			{
				if (IsValid(DefinitionPair.Value))
				{
					Definitions.Add(DefinitionPair.Value.Get());
				}
			}

			Definitions.Sort([](const DefinitionType& Left, const DefinitionType& Right)
			{
				return GetProductSortName(&Left) < GetProductSortName(&Right);
			});

			for (DefinitionType* Definition : Definitions)
			{
				Add(MakeProductReference(Definition));
			}
		}

		TArray<FShopCatalogEntry> Finish()
		{
			Entries.Sort([](const FShopCatalogEntry& Left, const FShopCatalogEntry& Right)
			{
				if (Left.ShopData.SortOrder != Right.ShopData.SortOrder)
				{
					return Left.ShopData.SortOrder < Right.ShopData.SortOrder;
				}

				if (Left.Product.ProductType != Right.Product.ProductType)
				{
					return static_cast<uint8>(Left.Product.ProductType) < static_cast<uint8>(Right.Product.ProductType);
				}

				return GetProductSortName(PdShopCatalog::ResolveProductObject(Left.Product))
					< GetProductSortName(PdShopCatalog::ResolveProductObject(Right.Product));
			});
			return MoveTemp(Entries);
		}

	private:
		TArray<FShopCatalogEntry> Entries;
		TSet<FString> SeenProductKeys;
	};
}

TArray<FShopCatalogEntry> PdShopCatalog::Build(const FShopCatalogSource& Source, const UContentDataSubsystem* ContentData)
{
	FShopCatalogBuilder Builder;
	for (const FShopCatalogProductReference& ProductReference : Source.ManualProducts)
	{
		Builder.Add(ProductReference);
	}

	if (Source.bIncludeAllPandoras && ContentData)
	{
		TMap<FName, TObjectPtr<UPandoraDefinition>> LoadedPandorasByName;
		ContentData->GetLoadedPandoraDefinitionsByName(LoadedPandorasByName);
		Builder.AddAllLoaded(LoadedPandorasByName);
	}

	if (Source.bIncludeAllSkins && ContentData)
	{
		TMap<FName, TObjectPtr<USkinDefinition>> LoadedSkinsByName;
		ContentData->GetLoadedSkinDefinitionsByName(LoadedSkinsByName);
		Builder.AddAllLoaded(LoadedSkinsByName);
	}

	return Builder.Finish();
}

UObject* PdShopCatalog::ResolveProductObject(const FShopCatalogProductReference& ProductReference)
{
	switch (ProductReference.ProductType)
	{
	case EShopProductType::Pandora:
		return ProductReference.PandoraDefinition.Get();
	case EShopProductType::Skin:
		return ProductReference.SkinDefinition.Get();
	case EShopProductType::Item:
		return ProductReference.ItemDefinition.Get();
	default:
		return nullptr;
	}
}
