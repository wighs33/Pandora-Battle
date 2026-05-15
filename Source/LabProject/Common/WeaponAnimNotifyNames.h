#pragma once

#include "CoreMinimal.h"

namespace WeaponAnimNotifyNames
{
	inline FName RedrawBow()
	{
		static const FName Name(TEXT("RedrawBow"));
		return Name;
	}

	inline FName HoldBow()
	{
		static const FName Name(TEXT("HoldBow"));
		return Name;
	}
}
