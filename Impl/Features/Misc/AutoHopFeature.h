#pragma once

//---------------------------------------------------------------------------

class AutoHopFeature : public scaffold::Feature 
{
public:

    AutoHopFeature(std::shared_ptr<scaffold::Messenger> InMessenger);

private:

    enum class EState : uint32_t
    {
        TryingToScuttle,
        AwaitingSuitabilityCheck
    };

    bool OnStart() override;
    bool OnEnd() override;
    void OnReset(EFeatureResetReason InResetReason) override;
    bool CanExecute() override;
    void OnConditionChange(bool bConditionChange) override;
    void OnDiscard(float DeltaTime) override;
    void OnExecute(float DeltaTime) override;
    void PostTick(float DeltaTime) override;
    void Tick(float DeltaTime) override;

    void OnActiveInWorld(const ActiveInGameWorldEvent& Ev);
    void OnMarkedUsersEncountered(const EncounteredMarkedUserEvent& Ev);

    bool NotifyWebhook(std::vector<std::string> InReasons);
    bool NotifyUser(std::vector<std::string> InReasons);

    bool CheckIfSuitableServer(bool bNotify = false);

private:

    scaffold::shared_ptr<scaffold::Messenger> Dispatcher;

    scaffold::SubscriptionHandle ActiveInWorldHandle;
    scaffold::SubscriptionHandle MarkedUserEncounterHandle;

    bool IsGoodServer = false;

    bool IsActive = false;

    EState CurrentState = EState::AwaitingSuitabilityCheck;

    uint64_t LastScuttleAttempt = 0;

    uint64_t HopsCount = 0;

    bool CurrentSessionFoundAutomatically = false;

    UClass* FortOfDamnedSignal = nullptr;
    UClass* FortOfFortuneSignal = nullptr;

    class PlayerListFeature* PlayerListInstance = nullptr;
    class DependencyLoaderFeature* DependencyLoaderInstance = nullptr;
};

//---------------------------------------------------------------------------
