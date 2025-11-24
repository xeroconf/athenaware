#pragma once

//---------------------------------------------------------------------------

#define PLAYER_LIST_FEATURE_ON_DUPE_RESOLVE_IGNORE

// Mark types, reflective of the server.
enum class EMarkType : uint32_t
{
    None = 0,
    Whitelist = 1,
    Blacklist = 2,
    Streamer = 3,
    Cheater = 4,
    Female = 5,
    RareAssociate = 6,
    YouTuber = 7,

    // 0 - 99 reserved for future types

    // 100+ is reserved for types we determine at runtime
    AthenawareUser = 100,
    NewAccount = 101,

    Custom = 200
};

//---------------------------------------------------------------------------

struct PlayerListEntry
{
    PlayerListEntry() {};
    PlayerListEntry(std::string InGamertag, EMarkType InMarkType, std::string InMarkInfo) :
        Gamertag(InGamertag),
        MarkType(InMarkType),
        MarkInfo(InMarkInfo)
    {};

    std::string Gamertag;
    EMarkType MarkType = EMarkType::None;
    std::string MarkInfo;
    std::wstring GamertagWide;
};

//---------------------------------------------------------------------------

enum class EPlayerListMode
{
    Crews,
    Players
};

//---------------------------------------------------------------------------

class PlayerListFeature : 
    public scaffold::Feature 
{
public:

    PlayerListFeature(scaffold::Messenger* InMessenger, HttpClient* InPrismarineServiceHttpClient);

private:

    bool OnStart() override;
    bool OnEnd() override;
    void OnReset(EFeatureResetReason InResetReason) override;
    void OnDiscard(float DeltaTime) override;
    bool CanExecute() override;
    void PostTick(float DeltaTime) override;
    void Tick(float DeltaTime) override;
    void OnExecute(float DeltaTime) override;
    void OnConditionChange(bool bConditionChange) override;

    void OnPartyUserUpdate(const PartyUserUpdateEvent& Event);

    void OnReceiveResolvedGamertags(const RemoteReceiveResolvedXuidsEvent& Event);

    void OnActiveInGameWorld(const ActiveInGameWorldEvent& Event);

    void OnRecieveXboxToken(const XboxAuthTokenEvent& Event);

    void OnConfigLoaded(const ConfigUpdateEvent& Event);

    void OnSessionUpdate(const SessionStateChangeEvent& Ev);

    void OnActorAggregationUpdate(const ActorAggregationUpdateEvent& Ev);

    void SendResolveRequest(std::unordered_set<uint64_t>& InXuidList);

    void DrawPlayerList();

    void DrawCrewList();

    void DrawNoSessionList();

    bool ReadMarkConfiguration();

    AAthenaPlayerState* GetPlayerStateFromXuid(uint64_t InXuid);

    void UpdateServerWithSelfMarkPreference();

    std::string GetXboxToken() { return XboxAuthToken; }

    std::map<uint64_t, PlayerListEntry>& GetPlayerMappings() { return PlayerMappings; }

    std::pair<bool, uint32_t> GetMarkTypeConfigOptions(EMarkType InType);
    
    uint32_t RelevantMarksCount();

private:

    scaffold::SubscriptionHandle PartyUserUpdateHandle;

    scaffold::SubscriptionHandle ReceivedResolvedGamertagsHandle;

    scaffold::SubscriptionHandle ActiveInGameWorldHandle;

    scaffold::SubscriptionHandle ReceiveXboxTokenHandle;

    scaffold::SubscriptionHandle ConfigLoadedHandle;

    scaffold::SubscriptionHandle SessionUpdateHandle;

    scaffold::SubscriptionHandle ActorAggregationHandle;

    std::map<uint64_t, PlayerListEntry> PlayerMappings;

    //Queue users we get via the aggregation update so we can wait for their unique ID/XUID to replicate
    std::set<AAthenaPlayerState*> PlayerStateAwaitReplicateQueue;

    // List of names of users who are in the custom mark list
    std::unordered_set<std::string> CustomMarkList;

    EPlayerListMode ListMode = EPlayerListMode::Crews;

    // Queue up XUIDS before we join a session in order to prevent rate limit
    std::unordered_set<uint64_t> PreSessionQueue;

    std::string XboxAuthToken;

    bool bWindowPositionRequiresUpdate = true;

    bool bIconsLoaded = false;

    std::map<uint8_t, UTexture2D*> ActivityIcons;

    uint64_t LastQueueResolve = 0;

    scaffold::Feature* MenuFeatureInstance = nullptr;

    scaffold::Messenger* Dispatcher = nullptr;

    HttpClient* ServiceHttpClient = nullptr;
};

//---------------------------------------------------------------------------
