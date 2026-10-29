#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

namespace PdComponentLookup
{
	// 설정에 적은 이름이 컴포넌트 이름이나 컴포넌트 태그와 정확히 같은지 본다.
	inline bool MatchesNameOrTag(const UActorComponent& Component, const FName Name)
	{
		return Component.GetFName() == Name || Component.ComponentHasTag(Name);
	}

	// Actor에서 이름이나 태그가 Name인 첫 ComponentType을 찾는다. 이름이 비어 있으면 찾지 않는다.
	template <typename ComponentType>
	ComponentType* FindByNameOrTag(const AActor* Actor, const FName Name)
	{
		if (!Actor || Name.IsNone())
		{
			return nullptr;
		}

		TInlineComponentArray<ComponentType*> Components(Actor);
		for (ComponentType* Component : Components)
		{
			if (Component && MatchesNameOrTag(*Component, Name))
			{
				return Component;
			}
		}

		return nullptr;
	}
}
