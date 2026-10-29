#include "Definition/AbilitySystem/SkillAreaSettings.h"

#include "Common/CollisionChannels.h"
#include "Common/PdGameplayTags.h"

FSkillAreaSettings::FSkillAreaSettings()
{
	TargetGroundTraceChannel = CollisionChannels::VisibilityTrace();
	IndicatorCueTag = PdGameplayTags::GameplayCue_AOEIndicator;
	ImpactCueTag = PdGameplayTags::GameplayCue_LightningBolt;
	TargetingMaxRange = 3000.0;
	Radius = 256.0;
	CameraSettings.TargetFOV = 70.0f;
	CameraSettings.TargetBoomSocketOffset = FVector(80.0f, 320.0f, 100.0f);
	CameraSettings.TargetCameraRotation = FRotator::ZeroRotator;
	CameraSettings.InterpSpeed = 10.0f;
}
