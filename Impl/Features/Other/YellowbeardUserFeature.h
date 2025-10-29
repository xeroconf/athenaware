#pragma once

//---------------------------------------------------------------------------

// lol
// PATCHED

class YellowbeardUserFeature : 
    public scaffold::Feature
    //public scaffold::diagnostics::TelemetrySource
{
public:

    YellowbeardUserFeature(std::shared_ptr<scaffold::Messenger> InMessenger);

private:

    enum class EProcessBanEventResult
    {
        None,
        OpenFailed,
        ConnectFailed,
        JsonParseFailed,
        OpenRequestFailed,
        SendRequestFailed
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


    bool ProcessBanEvent(uint64_t InTargetXuid, uint32_t InSequence);
    void OnFeatureHashUpdate(const FeatureHashUpdateEvent& Ev);
    void OnXboxAuthToken(const XboxAuthTokenEvent& Ev);

    std::string GetCurrentISOTime();

public:

    bool YellowbeardUser(uint64_t InTargetXuid);

    bool IsReady() { return !XboxToken.empty() && !GameVersion.empty() && !FeatureHash.empty(); };

private:

    std::map<uint64_t, uint32_t> TargetQueue;

    std::string FakeSessionId;

    std::string FakeGameGuid;

    std::string FeatureHash;

    std::string GameVersion;

    std::string XboxToken;

    uint64_t TimeSinceLastProcess = 0;

    scaffold::SubscriptionHandle FeatureHashUpdateHandle;
    scaffold::SubscriptionHandle XboxAuthTokenHandle;

    scaffold::shared_ptr<scaffold::Messenger> Dispatcher;
    
};

//---------------------------------------------------------------------------