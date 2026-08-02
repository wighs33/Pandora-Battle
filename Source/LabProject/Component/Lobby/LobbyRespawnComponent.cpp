#include "Component/Lobby/LobbyRespawnComponent.h"

#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "Character/CharacterBase.h"
#include "Common/LabGameplayTags.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Definition/Lobby/LobbyModeDefinition.h"
#include "GameplayEffect.h"
#include "Lobby/Contents/LobbyGameMode.h"
#include "Lobby/Services/LobbyPreviewGrantService.h"
#include "Mode/PdPlayerController.h"
#include "Mode/PdPlayerState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LobbyRespawnComponent)

ULobbyRespawnComponent::ULobbyRespawnComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void ULobbyRespawnComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	Shutdown();
	Super::EndPlay(EndPlayReason);
}

void ULobbyRespawnComponent::RequestLobbyPlayerRespawn(
	AController* PlayerController,
	APawn* DeadPawn)
{
	ALobbyGameMode* GameMode = GetLobbyGameMode();
	if (!GameMode
		|| !GameMode->HasAuthority()
		|| !PlayerController
		|| !PlayerController->IsPlayerController())
	{
		return;
	}

	const TObjectKey<AController> PlayerControllerKey(
		PlayerController);
	if (PendingLobbyRespawnTimers.Contains(
		PlayerControllerKey))
	{
		return;
	}

	TWeakObjectPtr<AController> WeakPlayerController(
		PlayerController);
	TWeakObjectPtr<APawn> WeakDeadPawn(DeadPawn);
	const float RespawnDelay = GetLobbyRespawnDelay();

	if (ACharacterBase* DeadCharacter =
		Cast<ACharacterBase>(DeadPawn))
	{
		DeadCharacter->ClearCharacterOverlayMaterial();
		if (RespawnDelay > 0.0f)
		{
			DeadCharacter->StartDeathDissolve(
				RespawnDelay);
		}
	}

	if (APdPlayerController* PlayerControllerForUi =
		Cast<APdPlayerController>(PlayerController))
	{
		PlayerControllerForUi
			->Client_StartRespawnDelayCountdown(
				RespawnDelay);
	}

	if (RespawnDelay <= 0.0f)
	{
		FinishLobbyPlayerRespawn(
			WeakPlayerController,
			WeakDeadPawn);
		return;
	}

	UWorld* World = GameMode->GetWorld();
	if (!World)
	{
		return;
	}

	FTimerHandle RespawnTimerHandle;
	World->GetTimerManager().SetTimer(
		RespawnTimerHandle,
		FTimerDelegate::CreateWeakLambda(
			this,
			[this,
				WeakPlayerController,
				WeakDeadPawn,
				PlayerControllerKey]()
			{
				PendingLobbyRespawnTimers.Remove(
					PlayerControllerKey);
				FinishLobbyPlayerRespawn(
					WeakPlayerController,
					WeakDeadPawn);
			}),
		RespawnDelay,
		false);
	PendingLobbyRespawnTimers.Add(
		PlayerControllerKey,
		RespawnTimerHandle);
}

void ULobbyRespawnComponent::HandlePlayerLogout(
	AController* ExitingController)
{
	if (!ExitingController)
	{
		return;
	}

	const TObjectKey<AController> ControllerKey(
		ExitingController);
	if (FTimerHandle* TimerHandle =
		PendingLobbyRespawnTimers.Find(ControllerKey))
	{
		if (ALobbyGameMode* GameMode =
			GetLobbyGameMode())
		{
			GameMode->GetWorldTimerManager().ClearTimer(
				*TimerHandle);
		}
		PendingLobbyRespawnTimers.Remove(ControllerKey);
	}
}

void ULobbyRespawnComponent::Shutdown()
{
	if (ALobbyGameMode* GameMode = GetLobbyGameMode())
	{
		for (TPair<TObjectKey<AController>, FTimerHandle>&
			RespawnTimer : PendingLobbyRespawnTimers)
		{
			GameMode->GetWorldTimerManager().ClearTimer(
				RespawnTimer.Value);
		}
	}

	PendingLobbyRespawnTimers.Reset();
}

ALobbyGameMode*
ULobbyRespawnComponent::GetLobbyGameMode() const
{
	return Cast<ALobbyGameMode>(GetOwner());
}

void ULobbyRespawnComponent::FinishLobbyPlayerRespawn(
	TWeakObjectPtr<AController> WeakPlayerController,
	TWeakObjectPtr<APawn> WeakDeadPawn)
{
	ALobbyGameMode* GameMode = GetLobbyGameMode();
	if (!GameMode || !GameMode->HasAuthority())
	{
		return;
	}

	AController* PlayerController =
		WeakPlayerController.Get();
	if (!PlayerController)
	{
		return;
	}

	PendingLobbyRespawnTimers.Remove(
		TObjectKey<AController>(PlayerController));
	ResetLobbyPlayerStateForRespawn(
		PlayerController);

	FTransform RespawnTransform;
	const bool bHasRespawnTransform =
		TryGetLobbyPlayerRespawnTransform(
			PlayerController,
			RespawnTransform);

	APawn* CurrentPawn = PlayerController->GetPawn();
	APawn* DeadPawn = WeakDeadPawn.Get();
	APawn* RespawnPawn = CurrentPawn
		? CurrentPawn
		: DeadPawn;

	if (!CurrentPawn && IsValid(RespawnPawn))
	{
		PlayerController->Possess(RespawnPawn);
	}

	if (IsValid(RespawnPawn))
	{
		const FTransform FinalRespawnTransform =
			bHasRespawnTransform
				? RespawnTransform
				: RespawnPawn->GetActorTransform();

		if (ACharacterBase* RespawnedCharacter =
			Cast<ACharacterBase>(RespawnPawn))
		{
			RespawnedCharacter
				->ResetDeathStateForRespawnAtTransform(
					FinalRespawnTransform);
		}
		else
		{
			RespawnPawn->SetActorTransform(
				FinalRespawnTransform,
				false,
				nullptr,
				ETeleportType::TeleportPhysics);
			RespawnPawn->ForceNetUpdate();
		}
		PlayerController->SetControlRotation(
			FinalRespawnTransform
				.GetRotation().Rotator());
		if (APdPlayerController* PdPlayerController =
			Cast<APdPlayerController>(
				PlayerController))
		{
			PdPlayerController
				->Client_ResetRespawnedPawnStateAtTransform(
					FinalRespawnTransform);
			PdPlayerController
				->Client_HideRespawnDelayCountdown();
		}
		return;
	}

	if (bHasRespawnTransform)
	{
		GameMode->RestartPlayerAtTransform(
			PlayerController,
			RespawnTransform);
	}
	else
	{
		GameMode->RestartPlayer(PlayerController);
	}

	APawn* SpawnedPawn = PlayerController->GetPawn();
	if (!IsValid(SpawnedPawn))
	{
		if (APdPlayerController* PdPlayerController =
			Cast<APdPlayerController>(
				PlayerController))
		{
			PdPlayerController
				->Client_HideRespawnDelayCountdown();
		}
		return;
	}

	const FTransform FinalRespawnTransform =
		bHasRespawnTransform
			? RespawnTransform
			: SpawnedPawn->GetActorTransform();
	PlayerController->SetControlRotation(
		FinalRespawnTransform.GetRotation().Rotator());
	if (ACharacterBase* RespawnedCharacter =
		Cast<ACharacterBase>(SpawnedPawn))
	{
		RespawnedCharacter
			->ResetDeathStateForRespawnAtTransform(
				FinalRespawnTransform);
	}
	else
	{
		SpawnedPawn->ForceNetUpdate();
	}
	if (APdPlayerController* PdPlayerController =
		Cast<APdPlayerController>(PlayerController))
	{
		PdPlayerController
			->Client_ResetRespawnedPawnStateAtTransform(
				FinalRespawnTransform);
		PdPlayerController
			->Client_HideRespawnDelayCountdown();
	}

	if (APlayerController* PlayerControllerForGrant =
		Cast<APlayerController>(PlayerController))
	{
		if (ULobbyPreviewGrantService* PreviewGrantService =
			GameMode->GetPreviewGrantService())
		{
			PreviewGrantService->ScheduleGrant(
				PlayerControllerForGrant);
		}
	}
}

bool ULobbyRespawnComponent::
TryGetLobbyPlayerRespawnTransform(
	AController* PlayerController,
	FTransform& OutRespawnTransform)
{
	ALobbyGameMode* GameMode = GetLobbyGameMode();
	if (!GameMode || !PlayerController)
	{
		return false;
	}

	if (AActor* PlayerStart =
		GameMode->ChoosePlayerStart(PlayerController))
	{
		OutRespawnTransform =
			PlayerStart->GetActorTransform();
		return true;
	}

	if (const APawn* CurrentPawn =
		PlayerController->GetPawn())
	{
		OutRespawnTransform =
			CurrentPawn->GetActorTransform();
		return true;
	}

	return false;
}

void ULobbyRespawnComponent::
ResetLobbyPlayerStateForRespawn(
	AController* PlayerController) const
{
	APdPlayerState* PlayerState = PlayerController
		? PlayerController->GetPlayerState<APdPlayerState>()
		: nullptr;
	UPdAbilitySystemComponent* AbilitySystemComponent =
		PlayerState
			? PlayerState->GetPdAbilitySystemComponent()
			: nullptr;
	if (!AbilitySystemComponent
		|| !AbilitySystemComponent->IsRegistered()
		|| !AbilitySystemComponent->GetAttributeSet(
			UBasicAttributeSet::StaticClass()))
	{
		return;
	}

	const float MaxHealth =
		AbilitySystemComponent->GetNumericAttribute(
			UBasicAttributeSet::GetMaxHealthAttribute());
	const float MaxStamina =
		AbilitySystemComponent->GetNumericAttribute(
			UBasicAttributeSet::GetMaxStaminaAttribute());
	const float MaxMana =
		AbilitySystemComponent->GetNumericAttribute(
			UBasicAttributeSet::GetMaxManaAttribute());

	AbilitySystemComponent
		->ClearStatusEffectsForRespawn();

	FGameplayTagContainer DeadTags;
	DeadTags.AddTag(LabGameplayTags::State_Dead);
	AbilitySystemComponent
		->RemoveActiveEffectsWithGrantedTags(DeadTags);
	AbilitySystemComponent->RemoveActiveEffects(
		FGameplayEffectQuery
			::MakeQuery_MatchAnyOwningTags(DeadTags));

	AbilitySystemComponent->SetNumericAttributeBase(
		UBasicAttributeSet::GetHealthAttribute(),
		FMath::Max(MaxHealth, 1.0f));
	AbilitySystemComponent->SetNumericAttributeBase(
		UBasicAttributeSet::GetShieldAttribute(),
		0.0f);
	AbilitySystemComponent->SetNumericAttributeBase(
		UBasicAttributeSet::GetStaminaAttribute(),
		FMath::Max(MaxStamina, 0.0f));
	AbilitySystemComponent->SetNumericAttributeBase(
		UBasicAttributeSet::GetManaAttribute(),
		FMath::Max(MaxMana, 0.0f));
	AbilitySystemComponent->ForceReplication();
}

float ULobbyRespawnComponent::GetLobbyRespawnDelay() const
{
	const ALobbyGameMode* GameMode =
		GetLobbyGameMode();
	const ULobbyModeDefinition* Definition = GameMode
		? GameMode->GetLobbyModeDefinition()
		: nullptr;
	return Definition
		? FMath::Max(
			Definition->GetFlowSettings()
				.LobbyRespawnDelay,
			0.0f)
		: 0.0f;
}
