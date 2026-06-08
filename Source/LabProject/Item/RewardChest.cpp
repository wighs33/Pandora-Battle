#include "Item/RewardChest.h"

#include "Animation/AnimationAsset.h"
#include "Animation/AnimMontage.h"
#include "Character/PdPlayer.h"
#include "Components/MaterialBillboardComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/World.h"
#include "Item/ItemDefinition.h"
#include "Kismet/GameplayStatics.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Pandora/PandoraDefinition.h"
#include "Skin/SkinDefinition.h"
#include "Sound/SoundBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RewardChest)

DEFINE_LOG_CATEGORY_STATIC(LogRewardChest, Log, All);

ARewardChest::ARewardChest(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bReplicates = true;

	ChestMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("ChestMesh"));
	SetRootComponent(ChestMesh);
	if (ChestMesh)
	{
		ChestMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		ChestMesh->SetCollisionObjectType(ECC_WorldDynamic);
		ChestMesh->SetCollisionResponseToAllChannels(ECR_Block);
		ChestMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		ChestMesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
		ChestMesh->SetGenerateOverlapEvents(true);
		ChestMesh->SetCanEverAffectNavigation(false);
		ChestMesh->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::HandleChestBeginOverlap);
		ChestMesh->OnComponentEndOverlap.AddDynamic(this, &ThisClass::HandleChestEndOverlap);
	}

	InteractionBillboard = CreateDefaultSubobject<UMaterialBillboardComponent>(TEXT("InteractionBillboard"));
	if (InteractionBillboard)
	{
		InteractionBillboard->SetupAttachment(ChestMesh);
		InteractionBillboard->SetHiddenInGame(false);
		InteractionBillboard->SetVisibility(true);
	}

	InteractionTipWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("InteractionTipWidget"));
	if (InteractionTipWidget)
	{
		InteractionTipWidget->SetupAttachment(InteractionBillboard ? static_cast<USceneComponent*>(InteractionBillboard) : ChestMesh);
		InteractionTipWidget->SetHiddenInGame(true);
		InteractionTipWidget->SetVisibility(false);
		InteractionTipWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		InteractionTipWidget->SetDrawAtDesiredSize(true);
		InteractionTipWidget->SetWidgetSpace(EWidgetSpace::Screen);
	}

	OpenEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("OpenEffect"));
	if (OpenEffect)
	{
		OpenEffect->SetupAttachment(ChestMesh);
		OpenEffect->SetAutoActivate(false);
	}
}

void ARewardChest::BeginPlay()
{
	Super::BeginPlay();

	ConfigureChestCollision(ChestState == ERewardChestState::Closed);

	if (InteractionTipWidget && !InteractionTipWidget->GetWidgetClass())
	{
		UE_LOG(LogRewardChest, Warning,
			TEXT("[RewardChest] InteractionTipWidget has no WidgetClass. Assign WBP_InteractTip on the Blueprint. chest=%s widget=%s"),
			*GetNameSafe(this),
			*GetNameSafe(InteractionTipWidget));
	}

	UE_LOG(LogRewardChest, Verbose,
		TEXT("[RewardChest] BeginPlay. chest=%s state=%s hasAuthority=%s items=%d skins=%d pandoras=%d mesh=%s collisionEnabled=%s objectType=%d worldLocation=%s billboard=%s widget=%s effect=%s"),
		*GetNameSafe(this),
		*UEnum::GetValueAsString(ChestState),
		HasAuthority() ? TEXT("true") : TEXT("false"),
		RewardItems.Num(),
		RewardSkins.Num(),
		RewardPandoras.Num(),
		*GetNameSafe(ChestMesh),
		ChestMesh ? *UEnum::GetValueAsString(ChestMesh->GetCollisionEnabled()) : TEXT("None"),
		ChestMesh ? static_cast<int32>(ChestMesh->GetCollisionObjectType()) : INDEX_NONE,
		ChestMesh ? *ChestMesh->GetComponentLocation().ToString() : TEXT("None"),
		*GetNameSafe(InteractionBillboard),
		*GetNameSafe(InteractionTipWidget),
		*GetNameSafe(OpenEffect));

	ApplyChestState(nullptr);
}

void ARewardChest::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FinishOpeningTimerHandle);
		World->GetTimerManager().ClearTimer(HideOpenedChestTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void ARewardChest::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(ARewardChest, ChestState, Params);
}

bool ARewardChest::CanInteract_Implementation(AActor* InteractingActor)
{
	return ChestState == ERewardChestState::Closed && IsValid(InteractingActor);
}

bool ARewardChest::Interact_Implementation(AActor* InteractingActor)
{
	UE_LOG(LogRewardChest, Log,
		TEXT("[RewardChest] Interact. chest=%s interactor=%s state=%s"),
		*GetNameSafe(this),
		*GetNameSafe(InteractingActor),
		*UEnum::GetValueAsString(ChestState));

	if (!CanInteract_Implementation(InteractingActor))
	{
		UE_LOG(LogRewardChest, Warning,
			TEXT("[RewardChest] Interact skipped: cannot interact. chest=%s interactor=%s state=%s"),
			*GetNameSafe(this),
			*GetNameSafe(InteractingActor),
			*UEnum::GetValueAsString(ChestState));
	}

	// Reward application is owned by PlayerRewardComponent.
	return false;
}

FText ARewardChest::GetInteractText_Implementation(AActor* InteractingActor)
{
	return CanInteract_Implementation(InteractingActor)
		? NSLOCTEXT("RewardChest", "OpenChestInteractText", "Open")
		: FText::GetEmpty();
}

void ARewardChest::GetRewardItems_Implementation(TArray<FPrimaryAssetId>& OutItemDefinitionList)
{
	OutItemDefinitionList.Reset();
	if (ChestState != ERewardChestState::Closed)
	{
		UE_LOG(LogRewardChest, Warning,
			TEXT("[RewardChest] GetRewardItems skipped: not closed. chest=%s state=%s"),
			*GetNameSafe(this),
			*UEnum::GetValueAsString(ChestState));
		return;
	}

	AppendPrimaryAssetIds(RewardItems, OutItemDefinitionList);
	UE_LOG(LogRewardChest, Log,
		TEXT("[RewardChest] GetRewardItems. chest=%s configured=%d resolved=%d"),
		*GetNameSafe(this),
		RewardItems.Num(),
		OutItemDefinitionList.Num());
}

void ARewardChest::GetRewardSkins_Implementation(TArray<FPrimaryAssetId>& OutSkinDefinitionList)
{
	OutSkinDefinitionList.Reset();
	if (ChestState != ERewardChestState::Closed)
	{
		UE_LOG(LogRewardChest, Warning,
			TEXT("[RewardChest] GetRewardSkins skipped: not closed. chest=%s state=%s"),
			*GetNameSafe(this),
			*UEnum::GetValueAsString(ChestState));
		return;
	}

	AppendPrimaryAssetIds(RewardSkins, OutSkinDefinitionList);
	UE_LOG(LogRewardChest, Log,
		TEXT("[RewardChest] GetRewardSkins. chest=%s configured=%d resolved=%d"),
		*GetNameSafe(this),
		RewardSkins.Num(),
		OutSkinDefinitionList.Num());
}

void ARewardChest::GetRewardPandoras_Implementation(TArray<FPrimaryAssetId>& OutPandoraDefinitionList)
{
	OutPandoraDefinitionList.Reset();
	if (ChestState != ERewardChestState::Closed)
	{
		UE_LOG(LogRewardChest, Warning,
			TEXT("[RewardChest] GetRewardPandoras skipped: not closed. chest=%s state=%s"),
			*GetNameSafe(this),
			*UEnum::GetValueAsString(ChestState));
		return;
	}

	AppendPrimaryAssetIds(RewardPandoras, OutPandoraDefinitionList);
	UE_LOG(LogRewardChest, Log,
		TEXT("[RewardChest] GetRewardPandoras. chest=%s configured=%d resolved=%d"),
		*GetNameSafe(this),
		RewardPandoras.Num(),
		OutPandoraDefinitionList.Num());
}

void ARewardChest::OnRewardsClaimed_Implementation(AActor* RewardReceiver)
{
	UE_LOG(LogRewardChest, Log,
		TEXT("[RewardChest] OnRewardsClaimed. chest=%s receiver=%s state=%s hasAuthority=%s"),
		*GetNameSafe(this),
		*GetNameSafe(RewardReceiver),
		*UEnum::GetValueAsString(ChestState),
		HasAuthority() ? TEXT("true") : TEXT("false"));

	MarkOpened(RewardReceiver);
}

void ARewardChest::MarkOpened(AActor* RewardReceiver)
{
	if (!HasAuthority() || ChestState != ERewardChestState::Closed)
	{
		UE_LOG(LogRewardChest, Warning,
			TEXT("[RewardChest] MarkOpened skipped. chest=%s receiver=%s state=%s hasAuthority=%s"),
			*GetNameSafe(this),
			*GetNameSafe(RewardReceiver),
			*UEnum::GetValueAsString(ChestState),
			HasAuthority() ? TEXT("true") : TEXT("false"));
		return;
	}

	UE_LOG(LogRewardChest, Verbose,
		TEXT("[RewardChest] MarkOpened applying. chest=%s receiver=%s"),
		*GetNameSafe(this),
		*GetNameSafe(RewardReceiver));

	PlayCharacterInteractionAnimation(RewardReceiver);
	SetChestState(ERewardChestState::Opening, RewardReceiver);
}

void ARewardChest::OnRep_ChestState()
{
	UE_LOG(LogRewardChest, Log,
		TEXT("[RewardChest] OnRep_ChestState. chest=%s state=%s"),
		*GetNameSafe(this),
		*UEnum::GetValueAsString(ChestState));

	ApplyChestState(nullptr);
}

void ARewardChest::HandleChestBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	static_cast<void>(OtherBodyIndex);
	static_cast<void>(bFromSweep);
	static_cast<void>(SweepResult);

	APdPlayer* Player = Cast<APdPlayer>(OtherActor);
	const bool bShouldShowTip = ChestState == ERewardChestState::Closed && Player && Player->IsLocallyControlled();
	UE_LOG(LogRewardChest, Verbose,
		TEXT("[RewardChest] BeginOverlap. chest=%s state=%s overlapped=%s otherActor=%s otherComp=%s player=%s local=%s showTip=%s"),
		*GetNameSafe(this),
		*UEnum::GetValueAsString(ChestState),
		*GetNameSafe(OverlappedComponent),
		*GetNameSafe(OtherActor),
		*GetNameSafe(OtherComp),
		*GetNameSafe(Player),
		Player && Player->IsLocallyControlled() ? TEXT("true") : TEXT("false"),
		bShouldShowTip ? TEXT("true") : TEXT("false"));

	if (bShouldShowTip)
	{
		SetInteractionTipVisible(true);
	}
}

void ARewardChest::HandleChestEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex)
{
	static_cast<void>(OtherBodyIndex);

	APdPlayer* Player = Cast<APdPlayer>(OtherActor);
	const bool bShouldHideTip = ChestState == ERewardChestState::Closed && Player && Player->IsLocallyControlled();
	UE_LOG(LogRewardChest, Verbose,
		TEXT("[RewardChest] EndOverlap. chest=%s state=%s overlapped=%s otherActor=%s otherComp=%s player=%s local=%s hideTip=%s"),
		*GetNameSafe(this),
		*UEnum::GetValueAsString(ChestState),
		*GetNameSafe(OverlappedComponent),
		*GetNameSafe(OtherActor),
		*GetNameSafe(OtherComp),
		*GetNameSafe(Player),
		Player && Player->IsLocallyControlled() ? TEXT("true") : TEXT("false"),
		bShouldHideTip ? TEXT("true") : TEXT("false"));

	if (bShouldHideTip)
	{
		SetInteractionTipVisible(false);
	}
}

template <typename DefinitionType>
void ARewardChest::AppendPrimaryAssetIds(
	const TArray<TSoftObjectPtr<DefinitionType>>& SourceDefinitions,
	TArray<FPrimaryAssetId>& OutPrimaryAssetIds) const
{
	for (const TSoftObjectPtr<DefinitionType>& SourceDefinition : SourceDefinitions)
	{
		if (SourceDefinition.IsNull())
		{
			continue;
		}

		const DefinitionType* LoadedDefinition = SourceDefinition.LoadSynchronous();
		if (!LoadedDefinition)
		{
			UE_LOG(LogRewardChest, Warning,
				TEXT("[RewardChest] reward definition load failed. chest=%s path=%s"),
				*GetNameSafe(this),
				*SourceDefinition.ToSoftObjectPath().ToString());
			continue;
		}

		const FPrimaryAssetId PrimaryAssetId = LoadedDefinition->GetPrimaryAssetId();
		if (PrimaryAssetId.IsValid())
		{
			OutPrimaryAssetIds.Add(PrimaryAssetId);
		}
		else
		{
			UE_LOG(LogRewardChest, Warning,
				TEXT("[RewardChest] reward definition has invalid primary asset id. chest=%s definition=%s"),
				*GetNameSafe(this),
				*GetNameSafe(LoadedDefinition));
		}
	}
}

void ARewardChest::ConfigureChestCollision(const bool bEnableInteraction) const
{
	if (!ChestMesh)
	{
		UE_LOG(LogRewardChest, Warning,
			TEXT("[RewardChest] ConfigureChestCollision skipped: ChestMesh missing. chest=%s interaction=%s"),
			*GetNameSafe(this),
			bEnableInteraction ? TEXT("true") : TEXT("false"));
		return;
	}

	ChestMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	ChestMesh->SetCollisionObjectType(ECC_WorldDynamic);
	ChestMesh->SetCollisionResponseToAllChannels(ECR_Block);
	ChestMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	ChestMesh->SetCollisionResponseToChannel(ECC_WorldDynamic, bEnableInteraction ? ECR_Overlap : ECR_Ignore);
	ChestMesh->SetGenerateOverlapEvents(bEnableInteraction);
	ChestMesh->SetCanEverAffectNavigation(false);
	ChestMesh->UpdateOverlaps();

	UE_LOG(LogRewardChest, Verbose,
		TEXT("[RewardChest] ConfigureChestCollision. chest=%s mesh=%s interaction=%s enabled=%s objectType=%d pawnResponse=%d worldDynamicResponse=%d location=%s generateOverlap=%s"),
		*GetNameSafe(this),
		*GetNameSafe(ChestMesh),
		bEnableInteraction ? TEXT("true") : TEXT("false"),
		*UEnum::GetValueAsString(ChestMesh->GetCollisionEnabled()),
		static_cast<int32>(ChestMesh->GetCollisionObjectType()),
		static_cast<int32>(ChestMesh->GetCollisionResponseToChannel(ECC_Pawn)),
		static_cast<int32>(ChestMesh->GetCollisionResponseToChannel(ECC_WorldDynamic)),
		*ChestMesh->GetComponentLocation().ToString(),
		ChestMesh->GetGenerateOverlapEvents() ? TEXT("true") : TEXT("false"));
}

void ARewardChest::PlayCharacterInteractionAnimation(AActor* RewardReceiver) const
{
	if (!CharacterInteractionMontage)
	{
		UE_LOG(LogRewardChest, Verbose,
			TEXT("[RewardChest] character interaction animation skipped: montage missing. chest=%s receiver=%s"),
			*GetNameSafe(this),
			*GetNameSafe(RewardReceiver));
		return;
	}

	APdPlayer* Player = Cast<APdPlayer>(RewardReceiver);
	if (!Player)
	{
		UE_LOG(LogRewardChest, Warning,
			TEXT("[RewardChest] character interaction animation skipped: receiver is not APdPlayer. chest=%s receiver=%s montage=%s"),
			*GetNameSafe(this),
			*GetNameSafe(RewardReceiver),
			*GetNameSafe(CharacterInteractionMontage));
		return;
	}

	UE_LOG(LogRewardChest, Verbose,
		TEXT("[RewardChest] playing character interaction animation. chest=%s receiver=%s montage=%s playRate=%.2f"),
		*GetNameSafe(this),
		*GetNameSafe(Player),
		*GetNameSafe(CharacterInteractionMontage),
		CharacterInteractionMontagePlayRate);
	Player->PlayInteractionMontage(CharacterInteractionMontage, CharacterInteractionMontagePlayRate);
}

void ARewardChest::SetChestState(const ERewardChestState NewState, AActor* RewardReceiver)
{
	if (ChestState == NewState)
	{
		UE_LOG(LogRewardChest, Verbose,
			TEXT("[RewardChest] SetChestState skipped: same state. chest=%s state=%s receiver=%s"),
			*GetNameSafe(this),
			*UEnum::GetValueAsString(ChestState),
			*GetNameSafe(RewardReceiver));
		return;
	}

	UE_LOG(LogRewardChest, Log,
		TEXT("[RewardChest] state changed. chest=%s old=%s new=%s receiver=%s hasAuthority=%s"),
		*GetNameSafe(this),
		*UEnum::GetValueAsString(ChestState),
		*UEnum::GetValueAsString(NewState),
		*GetNameSafe(RewardReceiver),
		HasAuthority() ? TEXT("true") : TEXT("false"));

	ChestState = NewState;
	MARK_PROPERTY_DIRTY_FROM_NAME(ARewardChest, ChestState, this);
	ApplyChestState(RewardReceiver);
}

void ARewardChest::ApplyChestState(AActor* RewardReceiver)
{
	UE_LOG(LogRewardChest, Verbose,
		TEXT("[RewardChest] ApplyChestState. chest=%s state=%s receiver=%s"),
		*GetNameSafe(this),
		*UEnum::GetValueAsString(ChestState),
		*GetNameSafe(RewardReceiver));

	switch (ChestState)
	{
	case ERewardChestState::Closed:
		ApplyClosedState();
		break;
	case ERewardChestState::Opening:
		ApplyOpeningState(RewardReceiver);
		break;
	case ERewardChestState::Opened:
		ApplyOpenedState();
		break;
	case ERewardChestState::Hidden:
		ApplyHiddenState();
		break;
	default:
		ApplyClosedState();
		break;
	}
}

void ARewardChest::ApplyClosedState()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FinishOpeningTimerHandle);
		World->GetTimerManager().ClearTimer(HideOpenedChestTimerHandle);
	}

	if (ChestMesh)
	{
		ChestMesh->SetHiddenInGame(false, false);
		ChestMesh->SetVisibility(true, false);
	}

	if (InteractionBillboard)
	{
		SetInteractionAnchorVisible(true);
	}
	SetInteractionTipVisible(false);

	ConfigureChestCollision(true);
	UE_LOG(LogRewardChest, Verbose,
		TEXT("[RewardChest] closed state applied. chest=%s mesh=%s billboard=%s tipWidget=%s"),
		*GetNameSafe(this),
		*GetNameSafe(ChestMesh),
		*GetNameSafe(InteractionBillboard),
		*GetNameSafe(InteractionTipWidget));
}

void ARewardChest::ApplyOpeningState(AActor* RewardReceiver)
{
	SetInteractionTipVisible(false);
	SetInteractionAnchorVisible(false);
	ConfigureChestCollision(false);

	if (ChestMesh)
	{
		ChestMesh->SetHiddenInGame(false, false);
		ChestMesh->SetVisibility(true, false);
	}

	if (ChestMesh && OpenAnimation)
	{
		UE_LOG(LogRewardChest, Verbose,
			TEXT("[RewardChest] playing open animation. chest=%s mesh=%s animation=%s"),
			*GetNameSafe(this),
			*GetNameSafe(ChestMesh),
			*GetNameSafe(OpenAnimation));
		ChestMesh->PlayAnimation(OpenAnimation, false);
	}
	else
	{
		UE_LOG(LogRewardChest, Warning,
			TEXT("[RewardChest] open animation skipped. chest=%s mesh=%s animation=%s"),
			*GetNameSafe(this),
			*GetNameSafe(ChestMesh),
			*GetNameSafe(OpenAnimation));
	}

	if (OpenEffect)
	{
		UE_LOG(LogRewardChest, Verbose,
			TEXT("[RewardChest] activating effect. chest=%s effect=%s asset=%s activeBefore=%s"),
			*GetNameSafe(this),
			*GetNameSafe(OpenEffect),
			*GetNameSafe(OpenEffect->GetAsset()),
			OpenEffect->IsActive() ? TEXT("true") : TEXT("false"));
		OpenEffect->Activate(true);
	}
	else
	{
		UE_LOG(LogRewardChest, Warning,
			TEXT("[RewardChest] open effect skipped: OpenEffect component missing. chest=%s"),
			*GetNameSafe(this));
	}

	if (OpenSound)
	{
		UE_LOG(LogRewardChest, Verbose,
			TEXT("[RewardChest] playing sound. chest=%s sound=%s volume=%.2f"),
			*GetNameSafe(this),
			*GetNameSafe(OpenSound),
			OpenSoundVolume);
		UGameplayStatics::PlaySoundAtLocation(this, OpenSound, GetActorLocation(), OpenSoundVolume);
	}

	BP_OnChestOpened(RewardReceiver);
	ScheduleFinishOpening();
}

void ARewardChest::ApplyOpenedState()
{
	SetInteractionTipVisible(false);
	SetInteractionAnchorVisible(false);
	ConfigureChestCollision(false);
	ScheduleHideOpenedChest();
}

void ARewardChest::ApplyHiddenState()
{
	SetInteractionTipVisible(false);
	SetInteractionAnchorVisible(false);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FinishOpeningTimerHandle);
		World->GetTimerManager().ClearTimer(HideOpenedChestTimerHandle);
	}

	if (ChestMesh)
	{
		ChestMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ChestMesh->SetGenerateOverlapEvents(false);
		ChestMesh->SetHiddenInGame(true, false);
		ChestMesh->SetVisibility(false, false);
	}

	UE_LOG(LogRewardChest, Verbose,
		TEXT("[RewardChest] hidden state applied. chest=%s mesh=%s effect=%s effectAsset=%s"),
		*GetNameSafe(this),
		*GetNameSafe(ChestMesh),
		*GetNameSafe(OpenEffect),
		*GetNameSafe(OpenEffect ? OpenEffect->GetAsset() : nullptr));
}

void ARewardChest::ScheduleFinishOpening()
{
	if (!HasAuthority())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		FinishOpening();
		return;
	}

	World->GetTimerManager().ClearTimer(FinishOpeningTimerHandle);

	const float AnimationLength = OpenAnimation ? OpenAnimation->GetPlayLength() : 0.0f;
	const float FinishDelay = AnimationLength > 0.0f ? AnimationLength : HideAfterOpenFallbackDelay;
	UE_LOG(LogRewardChest, Verbose,
		TEXT("[RewardChest] schedule finish opening. chest=%s animation=%s animationLength=%.3f delay=%.3f"),
		*GetNameSafe(this),
		*GetNameSafe(OpenAnimation),
		AnimationLength,
		FinishDelay);

	if (FinishDelay <= 0.0f)
	{
		FinishOpening();
		return;
	}

	World->GetTimerManager().SetTimer(
		FinishOpeningTimerHandle,
		this,
		&ThisClass::FinishOpening,
		FinishDelay,
		false);
}

void ARewardChest::FinishOpening()
{
	if (!HasAuthority() || ChestState != ERewardChestState::Opening)
	{
		UE_LOG(LogRewardChest, Verbose,
			TEXT("[RewardChest] FinishOpening skipped. chest=%s state=%s hasAuthority=%s"),
			*GetNameSafe(this),
			*UEnum::GetValueAsString(ChestState),
			HasAuthority() ? TEXT("true") : TEXT("false"));
		return;
	}

	SetChestState(ERewardChestState::Opened, nullptr);
}

void ARewardChest::ScheduleHideOpenedChest()
{
	if (!HasAuthority())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		HideOpenedChest();
		return;
	}

	World->GetTimerManager().ClearTimer(HideOpenedChestTimerHandle);
	UE_LOG(LogRewardChest, Verbose,
		TEXT("[RewardChest] schedule hide opened chest. chest=%s delay=%.3f"),
		*GetNameSafe(this),
		HideAfterOpenFallbackDelay);

	if (HideAfterOpenFallbackDelay <= 0.0f)
	{
		HideOpenedChest();
		return;
	}

	World->GetTimerManager().SetTimer(
		HideOpenedChestTimerHandle,
		this,
		&ThisClass::HideOpenedChest,
		HideAfterOpenFallbackDelay,
		false);
}

void ARewardChest::HideOpenedChest()
{
	if (!HasAuthority() || ChestState != ERewardChestState::Opened)
	{
		UE_LOG(LogRewardChest, Verbose,
			TEXT("[RewardChest] HideOpenedChest skipped. chest=%s state=%s hasAuthority=%s"),
			*GetNameSafe(this),
			*UEnum::GetValueAsString(ChestState),
			HasAuthority() ? TEXT("true") : TEXT("false"));
		return;
	}

	SetChestState(ERewardChestState::Hidden, nullptr);
}

void ARewardChest::SetInteractionAnchorVisible(const bool bVisible) const
{
	UE_LOG(LogRewardChest, Verbose,
		TEXT("[RewardChest] SetInteractionAnchorVisible. chest=%s visible=%s billboard=%s"),
		*GetNameSafe(this),
		bVisible ? TEXT("true") : TEXT("false"),
		*GetNameSafe(InteractionBillboard));

	if (InteractionBillboard)
	{
		InteractionBillboard->SetHiddenInGame(!bVisible);
		InteractionBillboard->SetVisibility(bVisible);
	}
}

void ARewardChest::SetInteractionTipVisible(const bool bVisible) const
{
	if (bVisible)
	{
		SetInteractionAnchorVisible(false);
	}
	else if (ChestState == ERewardChestState::Closed)
	{
		SetInteractionAnchorVisible(true);
	}

	UE_LOG(LogRewardChest, Verbose,
		TEXT("[RewardChest] SetInteractionTipVisible. chest=%s visible=%s tipWidget=%s parent=%s"),
		*GetNameSafe(this),
		bVisible ? TEXT("true") : TEXT("false"),
		*GetNameSafe(InteractionTipWidget),
		InteractionTipWidget ? *GetNameSafe(InteractionTipWidget->GetAttachParent()) : TEXT("None"));

	if (InteractionTipWidget)
	{
		InteractionTipWidget->SetHiddenInGame(!bVisible, true);
		InteractionTipWidget->SetVisibility(bVisible, true);
	}
}
