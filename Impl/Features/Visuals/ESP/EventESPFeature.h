#pragma once

//---------------------------------------------------------------------------

class EventESPFeature : public scaffold::Feature 
{
public:

    enum class EGameplayEventType : uint8_t
    {
        Unknown,
        FortOfFortune,
        FortOfDamned,
        FortOfSkeleton,
        Fleet,
        AshenWinds,
        Flameheart
    };
    
    struct GameplayEventData
    {
        GameplayEventData() {};
        GameplayEventData(std::string InName) :
            Name(InName)
        {};

        std::string Name;
    };

    EventESPFeature(std::shared_ptr<scaffold::Messenger> InMessenger);

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

    void OnActorAggregationUpdate(const ActorAggregationUpdateEvent& Ev);

public:

    EGameplayEventType GetEventTypeFromSignal(AGameplayEventSignal* InSignal);

private:

    scaffold::shared_ptr<scaffold::Messenger> Dispatcher;

    UClass* FortOfFortuneSignal = nullptr;
    UClass* FortOfDamnedSignal = nullptr;
    UClass* FortSkeletonSignal = nullptr;
    UClass* FleetSignal = nullptr;
    UClass* AshenWindsSignal = nullptr;
    UClass* FlameheartSignal = nullptr;

    std::unordered_map< EGameplayEventType, GameplayEventData > EventTypes;

    scaffold::SubscriptionHandle AggregationUpdateHandle;

    bool bSetInternalAssets = false;

    std::map<AGameplayEventSignal*, EGameplayEventType> ActiveEvents;
};