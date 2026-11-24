#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "StoryHUD.generated.h"

class UStoryPauseMenu;
class UStoryScreen;

/** 스토리 맵의 화면. 로딩이 끝나면 감독에게 장을 시작하게 하고, ESC로 잠깐 멈춤 메뉴를 연다. */
UCLASS()
class LABPROJECT_API AStoryHUD : public AHUD
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Public API ------------------------------------------------------------------------------------------------------
	AStoryHUD(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UStoryScreen* GetStoryScreen() const { return StoryScreen; }

private:
	// Event Handlers --------------------------------------------------------------------------------------------------
	void OpenPauseMenu();
	void HandlePauseMenuClosed();

private:
	UPROPERTY(Transient)
	TObjectPtr<UStoryScreen> StoryScreen;

	UPROPERTY(Transient)
	TObjectPtr<UStoryPauseMenu> PauseMenu;

	bool bChapterStarted = false;
};
