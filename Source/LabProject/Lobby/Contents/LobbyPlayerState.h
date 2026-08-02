#pragma once

#include "CoreMinimal.h"
#include "Mode/PdPlayerState.h"
#include "LobbyPlayerState.generated.h"

class UBasicAttributeSet;
class UInventoryComponent;
class ULobbyPlayerStateComponent;
class ULobbyPreviewDefinition;
class UPandoraComponent;
class UPandoraTreeComponent;
class USkinComponent;

DECLARE_MULTICAST_DELEGATE(FOnLobbyPlayerStateChanged);

UCLASS()
class LABPROJECT_API ALobbyPlayerState : public APdPlayerState
{
	GENERATED_BODY()

public:
	ALobbyPlayerState(
		const FObjectInitializer& ObjectInitializer =
			FObjectInitializer::Get());

	virtual void BeginPlay() override;
	virtual void EndPlay(
		const EEndPlayReason::Type EndPlayReason) override;
	virtual USkinComponent* GetSkinComponent() const override;

	FOnLobbyPlayerStateChanged OnLobbyPlayerStateChanged;

	ULobbyPlayerStateComponent* GetLobbyPlayerStateComponent() const
	{
		return LobbyPlayerStateComponent.Get();
	}

	UFUNCTION(BlueprintCallable, Category = "!Lobby")
	void SetReady(bool bInReady);

	UFUNCTION(BlueprintPure, Category = "!Lobby")
	bool IsReady() const;

	UFUNCTION(BlueprintCallable, Category = "!Lobby")
	void SetLeavingLobby(bool bInLeavingLobby);

	UFUNCTION(BlueprintPure, Category = "!Lobby")
	bool IsLeavingLobby() const;

	UFUNCTION(BlueprintCallable, Category = "!Lobby")
	void SetNickname(const FText& InNickname);

	UFUNCTION(BlueprintCallable, Category = "!Lobby")
	void SetDefaultNickname(const FText& InNickname);

	UFUNCTION(BlueprintCallable, Category = "!Lobby")
	void ClearCustomNickname();

	UFUNCTION()
	FText GetNickname() const;

	UFUNCTION()
	FText GetNicknameHint() const;

	UFUNCTION(BlueprintPure, Category = "!Lobby")
	bool IsUsingNicknameHint() const;

	UFUNCTION(BlueprintCallable, Category = "!Lobby|Team")
	void SetTeamColorIndex(int32 InTeamColorIndex);

	UFUNCTION()
	int32 GetTeamColorIndex() const;

	void ImportPlayerMatchIdentity(
		const FPlayerMatchIdentity& InMatchIdentity);
	void InitializeLobbyPreviewAbilitySystem(
		const ULobbyPreviewDefinition* PreviewDefinition = nullptr);

	UFUNCTION(BlueprintCallable, Category = "!Lobby|UI")
	void RefreshLobbyUI() const;

protected:
	virtual FPlayerMatchIdentity
		GetMatchIdentityForCopyProperties() const override;

private:
	void HandleLobbyRuntimeStateChanged();
	void RequestDeferredLobbyUiRefresh() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Lobby|State", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ULobbyPlayerStateComponent> LobbyPlayerStateComponent;

	UPROPERTY(VisibleAnywhere, Category = "!Lobby|Preview", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInventoryComponent> LobbyInventoryComponent;

	UPROPERTY(VisibleAnywhere, Category = "!Lobby|Preview", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkinComponent> LobbySkinComponent;

	UPROPERTY(VisibleAnywhere, Category = "!Lobby|Preview", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPandoraComponent> LobbyPandoraComponent;

	UPROPERTY(VisibleAnywhere, Category = "!Lobby|Preview", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPandoraTreeComponent> LobbyPandoraTreeComponent;

	UPROPERTY(VisibleAnywhere, Category = "!Lobby|Preview", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBasicAttributeSet> LobbyBasicAttributeSet;
};
