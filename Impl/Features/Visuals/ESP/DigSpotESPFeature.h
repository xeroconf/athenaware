#pragma once

#define DIG_SPOT_FEATURE_UPDATE_INTERVAL 2000 // 2 seconds

class DigSpotESPFeature : public scaffold::Feature 
{
public:

    DigSpotESPFeature(std::shared_ptr<scaffold::Messenger> InMessenger);

    struct MapContext
    {
        std::string TextureName;
        FVector IslandLocation;
        float IslandCameraOrthoWidth;
    };

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
    bool IsWieldingMap();

    void UpdateDigspots();

    bool RegisterMap(AXMarksTheSpotMap* InMap);
    bool UnregisterMap(AXMarksTheSpotMap* InMap);

    void OnActorAggregationUpdate(const ActorAggregationUpdateEvent& Event);

private:

    scaffold::shared_ptr<scaffold::Messenger> Dispatcher;

    scaffold::SubscriptionHandle ActorAggUpdateHandle;

    std::unordered_map<AXMarksTheSpotMap*, MapContext> Maps;

    std::deque<FVector> DigSpots;

    std::unordered_set<AXMarksTheSpotMap*> RegistrationQueue;

    uint64_t LastUpdate = 0;

};