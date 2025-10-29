#pragma once

#define SHIP_ESP_DRAWER_INTERP_SPEED 7.f
#define SHIP_ESP_HOVER_THRESHOLD 750.0f
#define SHIP_ESP_DRAW_HEIGHT 2500.f

//---------------------------------------------------------------------------

class ShipESPFeature : public scaffold::Feature 
{
public:

    enum class EShipTeam
    {
        Crew,
        Enemy,
        AI,
        Alliance
    };

    struct ShipBindedContext
    {

        float DrawerPos = 0.0f; // 0.0 (closed) <-> 1.0 (open)

        bool bIsHovered = false; // Is hovered near crosshair?

        bool bIsOnScreen = false; // Is on screen (w2s result)

        FVector2D ScreenPosition; // Don't use without checking bIsOnScreen first

        EShipTeam Team = EShipTeam::Enemy;

        std::string SizeName;

        bool bIsSinking = false;

        FVector2D VisualsDrawScreenPosition;

        bool bIsVisualsPositionOnScreen = false;

        float DistanceMeters;

        float NormalizedWaterAmount = 0.0f;

        uint32_t Holes = 0;

        bool bHasFocus = true;
    };

    struct ShipProxyBindedContext
    {
        std::string SizeName;
        bool IsAI = false;
    };

    ShipESPFeature(std::shared_ptr<scaffold::Messenger> InMessenger);

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

    void OnActorAggregationUpdate(const ActorAggregationUpdateEvent& Event);

    //void OnShipFocusChange(const ActorFocusStateChangeEvent& Event);

    // Register a ship for context bindings
    void RegisterShip(AActor* InShip);

    // Deregister a ship for context bindings
    void DeregisterShip(AActor* InShip);

    // Get the string name of a ship (Sloop, Brig, etc..)
    std::string GetShipSizeName(AShip* Ship);

    std::string GetProxySizeName(AShipNetProxy* InShipProxy);

    // Get the team colour of the ship
    const ImColor& GetTeamColor(EShipTeam InTeam);

private:

    scaffold::shared_ptr<scaffold::Messenger> Dispatcher;

    // To store arbitrary data on each networked ship
    std::unordered_map<AShip*, ShipBindedContext*> Contexts;

    std::unordered_map<AShipNetProxy*, ShipProxyBindedContext*> Proxies;

    scaffold::SubscriptionHandle ActorUpdateHandle;
    //scaffold::SubscriptionHandle ShipFocusChangeHandle;

    uint64_t LastDelegatedUpdate = 0;

    const std::string TemplateEmissaryDistant = std::string(xorstr_("\xef\x80\xa4 {} ({}m)"));
    const std::string TemplateDistant = std::string(xorstr_("{} ({}m)"));
    const std::string TemplateSinking = std::string(xorstr_("\xEF\x8D\x94 {} \xEE\x90\x84 Sinking \xEE\x90\x84 {}m \xEF\x8D\x94"));
    const std::string TemplateBase = std::string(xorstr_("{} {} \xEE\x90\x84 {} \xEE\x90\x84 {}m"));

};

//---------------------------------------------------------------------------
