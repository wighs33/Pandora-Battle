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

	inline FName StartSkillTrail()
	{
		static const FName Name(TEXT("StartSkillTrail"));
		return Name;
	}

	inline FName StopSkillTrail()
	{
		static const FName Name(TEXT("StopSkillTrail"));
		return Name;
	}

	inline FName SpawnSkillSlash()
	{
		static const FName Name(TEXT("SpawnSkillSlash"));
		return Name;
	}
}
