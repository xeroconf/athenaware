#pragma once

//---------------------------------------------------------------------------

class AutoSwapFeature : public scaffold::Feature {
public:

    AutoSwapFeature(std::shared_ptr<scaffold::Messenger> InMessenger);

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

    void OnSendSerializedRpc(const SendSerializedRpcEvent& Ev);

public:

private:

    scaffold::shared_ptr<scaffold::Messenger> Dispatcher;

    scaffold::SubscriptionHandle SendSerializedRpcHandle;

    bool SwapWeaponNextFrame = false;
};