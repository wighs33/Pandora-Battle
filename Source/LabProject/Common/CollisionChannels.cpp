#include "Common/CollisionChannels.h"

#include "Engine/CollisionProfile.h"

namespace
{
	DEFINE_LOG_CATEGORY_STATIC(LogCollisionChannels, Log, All);

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

		UE_LOG(LogCollisionChannels, Fatal,
			TEXT("Required collision channel '%s' is missing. Configure it in Project Settings > Engine > Collision."),
			ChannelName);
		return ECC_MAX;
	}
}

ECollisionChannel CollisionChannels::HitableBody()
{
	static const ECollisionChannel Channel = ResolveRequiredChannel(TEXT("HitableBody"));
	return Channel;
}

ECollisionChannel CollisionChannels::Projectile()
{
	static const ECollisionChannel Channel = ResolveRequiredChannel(TEXT("ArrowProjectile"));
	return Channel;
}

ECollisionChannel CollisionChannels::OverlapBox()
{
	static const ECollisionChannel Channel = ResolveRequiredChannel(TEXT("OverlapBox"));
	return Channel;
}

ETraceTypeQuery CollisionChannels::VisibilityTrace()
{
	return UEngineTypes::ConvertToTraceType(ECC_Visibility);
}
