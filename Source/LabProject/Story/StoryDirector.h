#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Story/StoryTypes.h"
#include "StoryDirector.generated.h"

class AStoryCharacter;
class AStoryFloatingProp;
class AStoryPortal;
class UCameraComponent;
class USoundBase;
class UStaticMesh;
class UStoryChapterDefinition;
class UStoryScreen;
struct FStoryTask;

/**
 * 이야기 한 장을 재생한다. 인물을 세우고, 장의 단계를 차례로 진행하며, 카메라를 잡는다.
 * 화면(대사·암전·눈꺼풀)은 UStoryScreen이 그린다. 장이 끝나면 타이틀로 돌아간다.
 */
UCLASS()
class LABPROJECT_API AStoryDirector : public AActor
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void Tick(float DeltaSeconds) override;

	// Public API ------------------------------------------------------------------------------------------------------
	AStoryDirector(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	virtual ~AStoryDirector() override;

	static AStoryDirector* Find(const UObject* WorldContextObject);

	/** 인물을 세우고 첫 단계를 시작한다. 화면 준비가 끝난 뒤 HUD가 한 번 부른다. */
	void BeginChapter(UStoryScreen& InScreen);
	/** 남은 단계를 건너뛰고 장을 끝낸다. */
	void SkipChapter();
	void ReturnToTitle();
	bool IsPlaying() const { return bPlaying; }
	int32 GetStepIndex() const { return StepIndex; }

	// 단계가 쓰는 무대
	UStoryScreen& GetScreen() const { return *Screen; }
	AStoryPortal* GetPortal() const { return Portal; }
	AStoryCharacter* FindCharacter(FName Id) const;
	AStoryCharacter* GetPlayerCharacter() const;
	/** 자리 표시의 위치와 방향. 없으면 무대 원점이다. */
	FTransform GetMark(FName MarkId) const;
	/** 말하는 인물을 알린다. 다른 인물은 그 인물을 보고, 그 인물은 앞서 말한 인물을 본다. */
	void SetSpeaker(FName Id);
	void PlayShot(const FStoryShot& Shot);
	void Shake(float Strength, float Seconds);
	void PlaySound(USoundBase* Sound, float Pitch = 1.f) const;
	USoundBase* GetVoiceSound() const { return VoiceSound; }

	/** 플레이어에게 조종을 넘기고 되찾는다. 도착할 자리에는 별 표식을 띄운다. */
	void BeginPlayerControl(const FVector& Goal, FName ObjectiveKey);
	void EndPlayerControl();

private:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	void SpawnCast();
	void TickSteps(float DeltaSeconds);
	void TickCamera(float DeltaSeconds);
	void FinishChapter();
	FTransform ComputeShotTransform(const FStoryShot& Shot, float ShotSeconds) const;
	FVector GetShotPivot(const FStoryShot& Shot) const;

private:
	UPROPERTY(VisibleAnywhere, Category = "!Story")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(EditInstanceOnly, Category = "!Story")
	TObjectPtr<UStoryChapterDefinition> Chapter;

	UPROPERTY(EditInstanceOnly, Category = "!Story")
	TObjectPtr<AStoryPortal> Portal;

	/** 글자가 나올 때의 목소리 소리. 높이는 인물마다 다르다. */
	UPROPERTY(EditInstanceOnly, Category = "!Story")
	TObjectPtr<USoundBase> VoiceSound;

	/** 플레이어가 걸어갈 자리에 띄우는 표식 */
	UPROPERTY(EditInstanceOnly, Category = "!Story")
	TObjectPtr<UStaticMesh> GoalMarkerMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStoryScreen> Screen;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<AStoryCharacter>> Characters;

	UPROPERTY(Transient)
	TObjectPtr<AStoryFloatingProp> GoalMarker;

	FName CurrentSpeaker;
	FName PreviousSpeaker;

	TUniquePtr<FStoryTask> CurrentTask;
	TArray<TUniquePtr<FStoryTask>> BackgroundTasks;
	int32 StepIndex = INDEX_NONE;
	bool bPlaying = false;

	FStoryShot ActiveShot;
	/** bFollow를 끈 컷의 기준점과 방향. 컷이 시작할 때 정한다. */
	FVector ShotPivot = FVector::ZeroVector;
	float ShotYaw = 0.f;
	float ShotElapsed = 0.f;
	FTransform BlendFrom;
	float BlendFromFov = 40.f;
	float ShakeStrength = 0.f;
	float ShakeSeconds = 0.f;
	float ShakeElapsed = 0.f;
	float CameraTime = 0.f;
};
