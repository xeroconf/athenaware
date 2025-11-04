#pragma once

class ShipOnboardESPFeature : 
    public scaffold::Feature 
{
public:

    ShipOnboardESPFeature();

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

private:

    AMapTable* GetMapTable(AShip* InShip);

    std::string_view GetTrackedShipName(EWorldMapShipType InType);

private:
};