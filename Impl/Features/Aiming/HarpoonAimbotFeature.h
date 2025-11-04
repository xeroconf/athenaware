#pragma once

#define FEATURE_HARPOONAIMBOT_FOV 600.f

class HarpoonAimbotFeature : 
    public scaffold::Feature 
{
public:

    HarpoonAimbotFeature(std::shared_ptr<scaffold::Messenger> InMessenger);

private:

    bool OnStart() override;
    bool OnEnd() override;
    void OnReset(EFeatureResetReason InResetReason) override;
    void OnDiscard(float DeltaTime) override;
    bool CanExecute() override;
    void PostTick(float DeltaTime) override;
    void Tick(float DeltaTime) override;
    void OnExecute(float DeltaTime) override;
    void OnConditionChange(bool bConditionChange) override {};

protected:

    bool IsUsingHarpoon();

    void OnProcessRemoteFunction(ProcessRemoteFunctionEvent& Event);

private:

    UFunction* RequestFireFunction = nullptr;

    AFloatingItemProxy* Target = nullptr;

    std::shared_ptr<scaffold::Messenger> Dispatcher;
    
};