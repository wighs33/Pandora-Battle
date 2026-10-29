#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MenuLocalizationSubsystem.generated.h"

class UFont;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMenuLanguageChanged);

/**
 * 메뉴 문구 조회와 언어 전환을 맡는다.
 * 문구 원문(영어)은 문자열 테이블 MenuText에, 번역은 Game 지역화 타깃(.po → .locres)에 있고, 언어는 UE Culture로 바꾼다.
 */
UCLASS()
class LABPROJECT_API UMenuLocalizationSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// Public API ------------------------------------------------------------------------------------------------------
	/** 문자열 테이블 MenuText를 등록한다. 게임 모듈이 시작할 때 부른다. */
	static void RegisterMenuTextTable();
	static void UnregisterMenuTextTable();

	/** 현재 언어의 Culture 이름. 지원하지 않는 Culture면 원문 언어인 영어로 본다. */
	FString GetLanguage() const;

	/** 언어를 바꾸고 GameUserSettings에 저장해, 다음 실행부터는 엔진이 시작할 때 적용한다. */
	bool SetLanguage(const FString& Culture);

	static FText GetText(FName Key);

	/** 표에 Key가 없으면 Fallback을 돌려준다. 세션의 맵 이름처럼 실행 중에 정해지는 키에 쓴다. */
	static FText GetTextOrFallback(FName Key, const FText& Fallback);

	UFont* GetFontForLanguage(const FString& Culture) const;
	UFont* GetCurrentFont() const { return GetFontForLanguage(GetLanguage()); }

	/** 플레이어가 어느 언어로든 입력할 수 있는 칸에 쓰는 CJK 글꼴. 라틴 문자도 담고 있다. */
	UFont* GetCjkFont() const { return CjkFont; }

	/** 글상자를 현재 언어의 글꼴로 바꾼다. 크기·외곽선 같은 나머지 글꼴 설정은 그대로 둔다. */
	void ApplyLanguageFont(UTextBlock& Text) const;

public:
	/** Culture가 바뀐 뒤에 알린다. 문자열 테이블 문구는 엔진이 다시 그리고, 받는 쪽은 글꼴과 조합한 문구를 다시 만든다. */
	UPROPERTY(BlueprintAssignable, Category="UI|Localization")
	FOnMenuLanguageChanged OnLanguageChanged;

private:
	void MoveLegacyLanguageSelection();
	void HandleCultureChanged();

	UPROPERTY(Transient) TObjectPtr<UFont> CjkFont;
	UPROPERTY(Transient) TObjectPtr<UFont> LatinFont;
	FDelegateHandle CultureChangedHandle;
};
