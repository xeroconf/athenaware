#pragma once

//---------------------------------------------------------------------------

class CannonAutoLoadFeature : public scaffold::Feature 
{
public:

    CannonAutoLoadFeature(std::shared_ptr<scaffold::Messenger> InMessenger);

private:

    bool OnStart() override;
    bool OnEnd() override;
    void OnReset(EFeatureResetReason InResetReason) override;
    bool CanExecute() override;
    void OnConditionChange(bool bConditionChange) override;
    void OnDiscard(float DeltaTime) override;
    void OnExecute(float DeltaTime) override;
    void PostTick(float DeltaTime) override;
    void Tick(float DeltaTime) override;

    athena::ACannon* GetMountedOnCannon();

    void FireFeatureListEvent(bool InNewState);


private:

    scaffold::shared_ptr<scaffold::Messenger> Dispatcher;

    bool bIsActive = false;

    uint64_t LastLoadAttempt = 0;
};