#include "Definition/AbilitySystem/SkillAreaSettings.h"

#include "AbilitySystem/TargetingActors/GroundTargetActor.h"
#include "Common/CollisionChannels.h"
#include "Common/LabGameplayTags.h"

FSkillAreaSettings::FSkillAreaSettings()
{
	TargetGroundTraceChannel = LabCollisionChannels::VisibilityTrace();
	IndicatorCueTag = LabGameplayTags::GameplayCue_AOEIndicator;
	ImpactCueTag = LabGameplayTags::GameplayCue_LightningBolt;
	TargetActorClass = AGroundTargetActor::StaticClass();
	TargetingMaxRange = 3000.0;
	Radius = 256.0;
	CameraSettings.TargetFOV = 70.0f;
	CameraSettings.TargetBoomSocketOffset = FVector(80.0f, 320.0f, 100.0f);
	CameraSettings.TargetCameraRotation = FRotator::ZeroRotator;
	CameraSettings.InterpSpeed = 10.0f;
}
