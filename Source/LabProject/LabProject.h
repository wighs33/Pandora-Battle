#pragma once

#include "CoreMinimal.h"

// < IRIS 네트워크 규칙 >
//
// 기능 추가
// builc.cs에 SetupIrisSupport(Target);
//
// 헤더
// #include "Net/UnrealNetwork.h"
// #include "Net/Core/PushModel/PushModel.h"
//
// 선언 (GetLifetimeReplicatedProps 내부)
// FDoRepLifetimeParams Params; 
// Params.bIsPushBased = true;
// DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, 변수, Params);
// 
// 값 바꿀 때 
// MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, 변수, this);