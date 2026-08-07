#include "Common/CollisionChannels.h"

#include "Engine/CollisionProfile.h"

namespace
{
	DEFINE_LOG_CATEGORY_STATIC(LogLabCollisionChannels, Log, All);

	ECollisionChannel ResolveRequiredChannel(const TCHAR* ChannelName)
	{
		UCollisionProfile* CollisionProfile = UCollisionProfile::Get();
		const FName RequiredName(ChannelName);
		for (int32 ChannelIndex = 0; ChannelIndex < static_cast<int32>(ECC_MAX); ++ChannelIndex)
		{
			if (CollisionProfile->ReturnChannelNameFromContainerIndex(ChannelIndex) == RequiredName)
			{
				return static_cast<ECollisionChannel>(ChannelIndex);
			}
		}

		UE_LOG(
			LogLabCollisionChannels,
			Fatal,
			TEXT("Required collision channel '%s' is missing. Configure it in Project Settings > Engine > Collision."),
			ChannelName);
		return ECC_MAX;
	}
}

ECollisionChannel LabCollisionChannels::HitableBody()
{
	static const ECollisionChannel Channel = ResolveRequiredChannel(TEXT("HitableBody"));
	return Channel;
}

ECollisionChannel LabCollisionChannels::Projectile()
{
	static const ECollisionChannel Channel = ResolveRequiredChannel(TEXT("ArrowProjectile"));
	return Channel;
}

ECollisionChannel LabCollisionChannels::OverlapBox()
{
	static const ECollisionChannel Channel = ResolveRequiredChannel(TEXT("OverlapBox"));
	return Channel;
}

ETraceTypeQuery LabCollisionChannels::VisibilityTrace()
{
	return UEngineTypes::ConvertToTraceType(ECC_Visibility);
}
