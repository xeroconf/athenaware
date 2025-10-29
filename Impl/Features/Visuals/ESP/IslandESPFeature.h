#pragma once

class IslandESPFeature : public scaffold::Feature 
{
public:

    IslandESPFeature();

public:

    enum class EType : int8_t
    {
        Resource,
        Outpost,
        Seapost,
        Fortress,
        Reef
    };

    enum class EIslandEventType : int8_t
    {
        SkeletonFort,
        FortOfDamned,
        FortOfFortune,
        AshenWinds
    };

    struct IslandEntry
    {
        std::string Name;
        FVector Pos;
        float RevealFade = 0.0f;
        float Rotation = 0.0f;
        float BoundRadius = 0.0f;
        EType Type = EType::Resource;
    };


    bool OnStart() override;
    bool OnEnd() override;
    void OnReset(EFeatureResetReason InResetReason) override;
    void OnDiscard(float DeltaTime) override;
    bool CanExecute() override;
    void PostTick(float DeltaTime) override;
    void Tick(float DeltaTime) override;
    void OnExecute(float DeltaTime) override;
    void OnConditionChange(bool bConditionChange) override {};

    bool IsValidIsland(UIslandDataAssetEntry* InIsland);

    bool CacheIslands();

    bool GetFIsland(UIslandDataAssetEntry* InIsland, FIsland& OutIsland);

    IslandESPFeature::EType GetIslandTypeFromGameType(EIslandType InGameType);

    void OnActorAggregationUpdate(const ActorAggregationUpdateEvent& Event);

public:

    std::set<IslandEntry*>& GetCachedIslands();

private:
    // Cached islands
    std::set<IslandEntry*> Islands;

    // True if we've cached all islands into the Islands set
    bool bIslandsCached = false;
};