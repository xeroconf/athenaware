#pragma once

//---------------------------------------------------------------------------

#define FEATURE_SKIN_CHANGER_CUSTOM_SKINS_FILE xorstr_("UserSkins.json")
#define FEATURE_SKIN_CHANGER_DEFAULT_SKINS_FILE xorstr_("DefaultSkinsV2.json")
#define FEATURE_SKIN_CHANGER_USE_TICK_QUEUE_LOADING 1
#define FEATURE_SKIN_CHANGER_IMMEDIATE_ICON_LOAD 1
#define FEATURE_SKIN_CHANGER_NONCE 8811002299
#define FEATURE_SKIN_CHANGER_EXPORT_SKINS 0

//---------------------------------------------------------------------------

enum class ESkinChangerWieldableType : uint32_t
{
    Invalid = 0,
    Pistol = 1,
    Blunderbuss = 2,
    EyeOfReach = 3,
    DoubleBarrel = 4,
    ThrowingKnives = 5,
    Sword = 6,
};

struct SkinChangerMaterialEntry
{
    int32_t Index;
    UMaterialInstance* Material;
    std::wstring Path;
};


struct SkinChangerWeaponSkinEntry
{
    // Meta
    uint64_t Id = 0;
    std::string Name;
    std::vector<std::wstring> MeshPaths;
    std::wstring IconPath;

    /// The materials associated with this skin mesh
    std::vector<SkinChangerMaterialEntry> Materials;

    /// The meshes associated with this skin. If there's only 1 mesh then only
    /// the first index will be occupied (size of 1) index 0.
    std::vector<UObject*> Meshes;

    UTexture2D* Icon = nullptr;

    // Does this skin have materials?
    bool bHasMaterials = false;

    // Does this skin have multiple meshes? Only to support throwing knives
    bool bIsMultiMesh = false;

    // Is this skin loaded?
    bool bIsLoaded = false;

    // Is this skin a custom user defined one?
    bool bIsCustom = false;

    // Whether this skin is invalid and was unable to be loaded.
    bool bInvalidSkin = false;
};

class SkinChangerV2Feature : public scaffold::Feature 
{
public:

    SkinChangerV2Feature(std::shared_ptr<scaffold::Messenger> InMessenger, std::shared_ptr<HttpClient> InPrismarineServiceHttpClient);

private:

    typedef std::function<bool(AWieldableItem*, SkinChangerWeaponSkinEntry*)> TApplySkinFn;
    typedef std::function<bool(AWieldableItem*)> TRemoveSkinFn;

    struct SkinLifecycleEntry
    {
        SkinLifecycleEntry() {};
        SkinLifecycleEntry(TApplySkinFn InApplyFn, TRemoveSkinFn InRemoveFn) :
            ApplyFunction(InApplyFn),
            RemoveFunction(InRemoveFn)
        {};

        TApplySkinFn ApplyFunction = nullptr;
        TRemoveSkinFn RemoveFunction = nullptr;
    };

    bool OnStart() override;
    bool OnEnd() override;
    void OnReset(EFeatureResetReason InResetReason) override;
    bool CanExecute() override;
    void OnConditionChange(bool bConditionChange) override;
    void OnDiscard(float DeltaTime) override;
    void OnExecute(float DeltaTime) override;
    void PostTick(float DeltaTime) override;
    void Tick(float DeltaTime) override;

    void OnProcessRemoteFunction(const ProcessRemoteFunctionEvent& Event);

    void OnConfigUpdateEvent(const ConfigUpdateEvent& Event);

    void OnReceivedRemoteAssetContent(const ReceivedRemoteAssetContentEvent& Event);

    bool LoadDefaultSkinDefinitions();

    bool SetFromConfig(bool bForceUpdate = false);

    bool ApplyWieldableSkin(SkinChangerWeaponSkinEntry* InSkin, AWieldableItem* InItem);
    
    bool RemoveWieldableSkin(AWieldableItem* InItem);

    ESkinChangerWieldableType GetTypeFromWieldable(AWieldableItem* InItem);

    bool LoadIcons(ESkinChangerWieldableType InType);

    bool FullyLoadSkin(SkinChangerWeaponSkinEntry* InSkin);

    bool FindAndExportDefaultSkinDefinitions();

    bool GetSkinsDataFromEndpoint();

public:

    FEATURE_EXPORT bool LoadCustomSkinDefinitions();

    FEATURE_EXPORT std::map<ESkinChangerWieldableType, std::map<uint64_t, SkinChangerWeaponSkinEntry*>>& GetSkinCache() { return this->SkinsCache; }

    FEATURE_EXPORT std::map<ESkinChangerWieldableType, SkinChangerWeaponSkinEntry*>& GetActiveSkins() { return this->AppliedSkins; }

    FEATURE_EXPORT void ForceSkinApplyUpdate() { bForceUpdateNextFrame = true; }

    FEATURE_EXPORT uint64_t* GetSkinIdConfigPtr(ESkinChangerWieldableType InType);

    FEATURE_EXPORT bool SetSkin(ESkinChangerWieldableType InType, SkinChangerWeaponSkinEntry* InSkin);

public:

private:

    std::map<ESkinChangerWieldableType, std::map<uint64_t, SkinChangerWeaponSkinEntry*>> SkinsCache;

    std::map<ESkinChangerWieldableType, SkinChangerWeaponSkinEntry*> AppliedSkins;

    std::map<ESkinChangerWieldableType, SkinLifecycleEntry> SkinLifecycle;

    std::queue<SkinChangerWeaponSkinEntry*> IconLoadQueue;

    uint64_t LastSkinLoad = 0;

    uint64_t LastLoadoutSkinUpdate = 0;

    std::vector<uint8_t> DefaultSkinAssetDataBuffer;

    std::shared_ptr<scaffold::Messenger> Dispatcher;

    std::shared_ptr<HttpClient> ServiceHttpClient;

    scaffold::SubscriptionHandle ProcessRemoteFunctionHandle;
    scaffold::SubscriptionHandle ConfigUpdateEventHandle;
    scaffold::SubscriptionHandle ReceivedAssetContentHandle;

    UFunction* ServerWieldFunction = nullptr;

    bool bForceUpdateNextFrame = false;

    bool bCustomSkinsLoaded = false;
};

//---------------------------------------------------------------------------
