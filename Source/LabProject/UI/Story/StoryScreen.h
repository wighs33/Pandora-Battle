#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "StoryScreen.generated.h"

class UBorder;
class UCanvasPanel;
class UGameplayScreen;
class UImage;
class UProgressBar;
class USizeBox;
class USoundBase;
class UStoryCharacterDefinition;
class UTextBlock;
class UVerticalBox;

/**
 * 스토리 모드의 화면: 대사, 장 제목, 암전, 눈꺼풀, 연타 막대, 직접 걷는 동안의 목표를 그린다.
 * 무엇을 언제 보일지는 이야기 단계가 정하고, 이 화면은 넘김 입력(스페이스·엔터·클릭)을 받아 알려 준다.
 */
UCLASS()
class LABPROJECT_API UStoryScreen : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual bool NativeOnHandleBackAction() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	// Public API ------------------------------------------------------------------------------------------------------
	UStoryScreen(const FObjectInitializer& ObjectInitializer);

	// 층에 올리는 일은 UUiSubsystem::PushScreen이 한다.
	static UStoryScreen* Create(APlayerController* OwningPlayer, FSimpleDelegate InOnBack);

	// 대사
	void ShowLine(const UStoryCharacterDefinition* Speaker, FName TextKey, bool bShout, USoundBase* Voice);
	void HideLine();
	bool IsTyping() const;
	/** 플레이어가 대사를 넘겼으면 한 번만 true. 대사를 걷는다. */
	bool ConsumeLineAdvance();

	// 화면 덮기
	void FadeTo(const FLinearColor& Color, float Alpha, float Seconds);
	void SetEyelids(float Openness, float Seconds);
	void ShowTitleCard(FName ChapterKey, FName TitleKey, float Seconds);

	// 연타
	void BeginMash(FName PromptKey);
	void SetMashProgress(float Progress);
	int32 ConsumeMashPresses();
	void EndMash();

	/** 플레이어가 직접 걷는 동안에는 게임 입력을 열고 목표를 보여 준다. */
	void SetPlayerControl(bool bControl, FName ObjectiveKey);

	/** 넘김 입력 하나. 글자가 나오는 중이면 다 보이고, 다 보였으면 넘긴다. */
	void PressAdvance();

private:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	UTextBlock* MakeText(int32 Size, const FLinearColor& Color);
	UImage* AddFullScreenImage();
	void BuildLine();
	void BuildTitleCard();
	void BuildMash();
	void TickTyping(float DeltaSeconds);
	void TickLetterbox(float DeltaSeconds);
	void TickTitleCard(float DeltaSeconds);
	void ApplyEyelids();
	float MeasureLineWidth(const FString& Text) const;

private:
	UPROPERTY(Transient)
	TObjectPtr<UBorder> Root;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> Canvas;

	UPROPERTY(Transient)
	TObjectPtr<UImage> EyelidTop;

	UPROPERTY(Transient)
	TObjectPtr<UImage> EyelidBottom;

	UPROPERTY(Transient)
	TObjectPtr<UImage> LetterboxTop;

	UPROPERTY(Transient)
	TObjectPtr<UImage> LetterboxBottom;

	UPROPERTY(Transient)
	TObjectPtr<UImage> Shade;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> LineBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> LineSize;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LineText;

	UPROPERTY(Transient)
	TObjectPtr<UImage> NextMark;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HintText;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> TitleBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ChapterText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> MashBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MashText;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> MashBar;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ObjectiveText;

	UPROPERTY(Transient)
	TObjectPtr<UImage> FadeImage;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> VoiceSound;

	/** 직접 걷는 동안 위에 올려 두는 게임 중 기본 화면 */
	UPROPERTY(Transient)
	TObjectPtr<UGameplayScreen> GameplayScreen;

	FSimpleDelegate OnBack;

	FString FullLine;
	float TypedCharacters = 0.f;
	float TypingPause = 0.f;
	float VoicePitch = 1.f;
	int32 LastVoiceCharacter = 0;
	float LineElapsed = 0.f;
	bool bLineShown = false;
	bool bLineAdvanced = false;

	FLinearColor FadeColor = FLinearColor::Black;
	float FadeFrom = 1.f;
	float FadeTarget = 1.f;
	float FadeSeconds = 0.f;
	float FadeElapsed = 0.f;

	float EyelidOpenness = 1.f;
	float EyelidFrom = 1.f;
	float EyelidTarget = 1.f;
	float EyelidSeconds = 0.f;
	float EyelidElapsed = 0.f;

	float TitleSeconds = 0.f;
	float TitleElapsed = 0.f;

	int32 MashPresses = 0;
	bool bMashing = false;

	float LetterboxAmount = 0.f;
	bool bPlayerControl = false;
};
