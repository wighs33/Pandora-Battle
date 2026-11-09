#pragma once

#include "CoreMinimal.h"

/**
 * UI 움직임의 공통 식. 메뉴가 열리고 고르는 동작이 같은 박자와 탄성을 갖게 한다.
 * 스프링은 질량 1에 강성(Stiffness)과 감쇠(Damping)로 정한다. 감쇠비 = Damping / (2 * sqrt(Stiffness))가 1보다 작으면 목표를 넘었다 돌아온다.
 */
namespace UiMotion
{
	/** 빠르게 출발해 부드럽게 멈춘다. 열기에 쓴다. */
	inline float EaseOutCubic(const float Alpha)
	{
		return 1.f - FMath::Cube(1.f - FMath::Clamp(Alpha, 0.f, 1.f));
	}

	/** 처음이 가장 빠르고 끝에서 길게 가라앉는다. 화면이 내려앉을 때 쓴다. */
	inline float EaseOutExpo(const float Alpha)
	{
		const float Clamped = FMath::Clamp(Alpha, 0.f, 1.f);
		return Clamped >= 1.f ? 1.f : 1.f - FMath::Pow(2.f, -10.f * Clamped);
	}

	/** 스프링의 성질. 같은 성질을 여러 값이 함께 쓴다. */
	struct FSpringSettings
	{
		float Stiffness = 300.f;
		float Damping = 24.f;
	};

	/** 목표로 끌려가는 값 하나. 프레임 시간이 길어도 튀지 않도록 작은 걸음으로 나눠 적분한다. */
	struct FSpring
	{
		float Value = 0.f;
		float Velocity = 0.f;

		void Snap(const float InValue)
		{
			Value = InValue;
			Velocity = 0.f;
		}

		void Step(const float Target, const float DeltaSeconds, const FSpringSettings& Settings)
		{
			constexpr float MaxStepSeconds = 1.f / 240.f;
			float Remaining = FMath::Min(DeltaSeconds, 0.1f);
			while (Remaining > 0.f)
			{
				const float StepSeconds = FMath::Min(Remaining, MaxStepSeconds);
				const float Acceleration = Settings.Stiffness * (Target - Value) - Settings.Damping * Velocity;
				Velocity += Acceleration * StepSeconds;
				Value += Velocity * StepSeconds;
				Remaining -= StepSeconds;
			}
		}

		bool IsSettled(const float Target, const float Tolerance = 0.001f) const
		{
			return FMath::Abs(Value - Target) < Tolerance && FMath::Abs(Velocity) < Tolerance;
		}
	};

	/** 각도(라디안)를 목표 쪽으로 가장 짧게 도는 값으로 바꾼다. 원형 메뉴의 강조가 반대편으로 한 바퀴 돌지 않게 한다. */
	inline float NearestAngle(const float From, const float To)
	{
		return From + FMath::FindDeltaAngleRadians(From, To);
	}

	/** 한 번 재생하고 멈추는 짧은 효과의 진행. 맞았을 때의 번쩍임, 준비됐을 때의 고리처럼 시작 신호만 있는 움직임에 쓴다. */
	struct FOneShot
	{
		void Start()
		{
			Elapsed = 0.f;
		}

		bool IsPlaying() const
		{
			return Elapsed >= 0.f;
		}

		/** 진행을 넘기고 0~1을 돌려준다. 끝까지 가면 1을 돌려주고 멈춘다. 멈춘 동안에도 1이다. */
		float Advance(const float DeltaSeconds, const float DurationSeconds)
		{
			if (!IsPlaying())
			{
				return 1.f;
			}
			Elapsed += DeltaSeconds;
			const float Alpha = DurationSeconds > 0.f ? FMath::Min(Elapsed / DurationSeconds, 1.f) : 1.f;
			if (Alpha >= 1.f)
			{
				Elapsed = -1.f;
			}
			return Alpha;
		}

	private:
		float Elapsed = -1.f;
	};

	/**
	 * 줄어든 목표는 잠시 머물렀다 따라가고, 늘어난 목표는 바로 따라가는 값. 체력 막대 뒤에서 방금 깎인 양을 보여 준다.
	 * 머무는 동안 또 줄면 머무는 시간을 다시 센다. 연타를 맞으면 꼬리가 한꺼번에 내려온다.
	 */
	struct FTrailingValue
	{
		float Value = 0.f;

		void Snap(const float InValue)
		{
			Value = Target = InValue;
			HoldRemaining = 0.f;
		}

		/** 목표를 바꾼다. 줄었으면 true를 돌려준다. */
		bool SetTarget(const float NewTarget, const float HoldSeconds)
		{
			const bool bDropped = NewTarget < Target;
			if (bDropped)
			{
				HoldRemaining = HoldSeconds;
			}
			Target = NewTarget;
			if (Target >= Value)
			{
				Snap(Target);
			}
			return bDropped;
		}

		/** 머무는 시간이 끝났으면 목표 쪽으로 지수적으로 다가간다. CatchUpSpeed가 클수록 빨리 따라간다. */
		void Step(const float DeltaSeconds, const float CatchUpSpeed)
		{
			if (HoldRemaining > 0.f)
			{
				HoldRemaining -= DeltaSeconds;
				return;
			}
			Value = FMath::FInterpTo(Value, Target, DeltaSeconds, CatchUpSpeed);
			if (FMath::IsNearlyEqual(Value, Target, SettleTolerance))
			{
				Value = Target;
			}
		}

		bool IsSettled() const
		{
			return HoldRemaining <= 0.f && Value == Target;
		}

	private:
		static constexpr float SettleTolerance = 0.001f;
		float Target = 0.f;
		float HoldRemaining = 0.f;
	};
}
