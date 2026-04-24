// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MVVMViewModelBase.h"
#include "CommonViewModelBase.generated.h"

/**
 * ViewModel 베이스 클래스
 *
 * UE5 MVVM 시스템의 UMVVMViewModelBase를 상속하며, FieldNotify 기반의 프로퍼티 변경 알림을
 * 지원합니다. 모든 ViewModel 클래스는 이 클래스를 상속해야 합니다.
 *
 * 사용 예시:
 * @code
 * UCLASS()
 * class UHealthViewModel : public UCommonViewModelBase
 * {
 *     GENERATED_BODY()
 *
 * public:
 *     UPROPERTY(BlueprintReadOnly, FieldNotify)
 *     float CurrentHealth = 100.f;
 *
 *     UPROPERTY(BlueprintReadOnly, FieldNotify)
 *     float MaxHealth = 100.f;
 *
 *     void SetCurrentHealth(float Value)
 *     {
 *         UE_MVVM_SET_PROPERTY_VALUE(CurrentHealth, Value);
 *     }
 * };
 * @endcode
 */
UCLASS(Abstract)
class LABPROJECT_API UCommonViewModelBase : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	UCommonViewModelBase();

	//-----------------------------------------------------------------------------
	// 초기화
	//-----------------------------------------------------------------------------

	/** ViewModel을 초기화합니다. 서브클래스에서 오버라이드하여 데이터 소스 바인딩 등을 수행합니다 */
	virtual void InitializeViewModel(UObject* SourceObject = nullptr);

	/** ViewModel을 정리합니다. 서브클래스에서 오버라이드하여 바인딩 해제 등을 수행합니다 */
	virtual void UninitializeViewModel();

	/** ViewModel이 초기화되었는지 반환합니다 */
	bool IsViewModelInitialized() const { return bIsInitialized; }

protected:
	/** ViewModel 초기화 상태 */
	bool bIsInitialized = false;
};