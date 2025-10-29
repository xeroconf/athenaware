#pragma once

//---------------------------------------------------------------------------

class NoCannonFlashFeature : public scaffold::Feature {
public:
    NoCannonFlashFeature();
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

    bool IsMountedToCannon();

public:

private:

    ACannon* ActiveCannon = nullptr;
    UObject* CachedVfx = nullptr;

};