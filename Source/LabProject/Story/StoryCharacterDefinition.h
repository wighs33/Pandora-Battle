#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "StoryCharacterDefinition.generated.h"

class UAnimSequenceBase;
class UMaterialInterface;
class USkeletalMesh;

/** 이야기에 나오는 인물 하나: 스틱맨 몸에 입히는 메시, 얼굴판, 동작, 이름과 목소리. */
UCLASS(BlueprintType)
class LABPROJECT_API UStoryCharacterDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** 이름(StoryText 키)과 대사 창의 이름 색 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Story|Name")
	FName NameKey;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Story|Name")
	FLinearColor NameColor = FLinearColor::White;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Story|Look")
	TObjectPtr<USkeletalMesh> BodyMesh;

	/** 몸의 자세를 따라가는 옷, 머리카락, 장신구 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Story|Look")
	TArray<TObjectPtr<USkeletalMesh>> OutfitMeshes;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Story|Look")
	TObjectPtr<USkeletalMesh> FaceMesh;

	/** 표정 16칸을 담은 재질. 칸은 FaceCell 값으로 고른다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Story|Look")
	TObjectPtr<UMaterialInterface> FaceMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Story|Animation")
	TMap<FName, TObjectPtr<UAnimSequenceBase>> Clips;

	/** 아무 동작도 하지 않을 때, 앞으로·뒤로 걸을 때의 동작(Clips의 이름) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Story|Animation")
	FName IdleClip;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Story|Animation")
	FName WalkClip;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Story|Animation")
	FName WalkBackClip;

	/** 걷기 동작이 제 속도일 때의 이동 속도. 실제 속도와의 비율로 재생 속도를 맞춘다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Story|Animation", meta = (ClampMin = "1.0", Units = "CentimetersPerSecond"))
	float WalkClipSpeed = 150.f;

	/** 떠 있는 인물은 위아래로 흔들리고, 걷는 동작 없이 미끄러지듯 움직이며, 몸이 이야기 상대를 향한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Story|Floating")
	bool bFloats = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Story|Floating", meta = (EditCondition = "bFloats", Units = "cm"))
	float BobHeight = 6.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Story|Floating", meta = (EditCondition = "bFloats", ClampMin = "0.1", Units = "s"))
	float BobSeconds = 3.2f;

	/** 글자가 나올 때 나는 목소리 소리의 높이 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Story|Voice", meta = (ClampMin = "0.1"))
	float VoicePitch = 1.f;
};
