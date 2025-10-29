#pragma once

//---------------------------------------------------------------------------

#define FEATURE_CHARACTER_AIMBOT_ENABLE_WALLBANG 1
#define FEATURE_CHARACTER_AIMBOT_ENABLE_DEBUG 0
#define FEATURE_CHARACTER_AIMBOT_ENABLE_AIR_FREEZE_SUPPORT 1

class CharacterAimbotV2Feature : public scaffold::Feature 
{
public:

    CharacterAimbotV2Feature(std::shared_ptr<scaffold::Messenger> InMessenger);

private:

    struct SuitableTargetResult
    {
        bool IsUsingLocationOverride = false;
        AActor* Target = nullptr;
        FVector OverrideLocation = false;
        float AngleDegrees = 0.0f;

        bool Exists() const { return this->Target != nullptr; }
    };

    enum class EWallbangFireType : uint8_t
    {
        Origin,
        Extended
    };

    typedef std::function<bool(AWieldableItem*, AActor*)> TGoodTargetFunction;
    typedef std::function<bool(AWieldableItem*, AActor*, FVector, FVector, FRotator*)> TGetAimAngleFunction;

    struct WeaponAimbotDelegates
    {
        WeaponAimbotDelegates(TGoodTargetFunction InValidationFunction, TGetAimAngleFunction InGetAimAngleFunction) :
            ValidationFunction(InValidationFunction),
            GetAimAngleFunction(InGetAimAngleFunction)
        {};

        TGoodTargetFunction ValidationFunction;
        TGetAimAngleFunction GetAimAngleFunction;
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

    
    bool TryWallbang(CharacterAimbotV2Feature::SuitableTargetResult& InSuitableTarget);
    bool TryAimbot(CharacterAimbotV2Feature::SuitableTargetResult& InSuitableTarget);

    bool GetSuitableTarget(CharacterAimbotV2Feature::SuitableTargetResult* OutVisibleTarget, CharacterAimbotV2Feature::SuitableTargetResult* OutInvisibleTarget);

    bool PredictAimAngle(AActor* InTarget, FVector InFireLocation, FVector InTargetLocation, FRotator* OutAimRotation);


    // Prediction Algorithms
    // Returns if we could predict an angle
    bool GetAimAngle_ThrowableKnife(AWieldableItem* InWeapon, AActor* InTarget, FVector InFireLocation, FVector InTargetLocation, FRotator* OutAimAngle);
    bool GetAimAngle_ProjectileWeapon(AWieldableItem* InWeapon, AActor* InTarget, FVector InFireLocation, FVector InTargetLocation, FRotator* OutAimAngle);
    bool GetAimAngle_Blowpipe(AWieldableItem* InWeapon, AActor* InTarget, FVector InFireLocation, FVector InTargetLocation, FRotator* OutAimAngle);

    bool IsGoodTarget_ThrowableKnife(AWieldableItem* InWeapon, AActor* InTarget);
    bool IsGoodTarget_ProjectileWeapon(AWieldableItem* InWeapon, AActor* InTarget);
    bool IsGoodTarget_Blowpipe(AWieldableItem* InWeapon, AActor* InTarget);

    AWieldableItem* GetWieldedWeapon();
    AAthenaPlayerCharacter* GetLocalPlayer();
    FVector GetLocalViewLocation();

    void ResetWallbangState();

    void OnSendSerialisedRpc(SendSerializedRpcEvent& Ev);
    void OnReceiveSecurityAction(ReceivedSecurityAction& Ev);

    bool FindBestLineOfSightFireLocation(AActor* InTarget, FVector InCameraLocation, FRotator InCameraRotation, FVector* OutFireLocation, float InMaximumDistance = 300);

public:

private:

    scaffold::shared_ptr<scaffold::Messenger> Dispatcher;

    std::unordered_map<UClass*, WeaponAimbotDelegates> WeaponCategoryDelegates;
    std::unordered_set<UClass*> WallbangableWeapons;

    EWallbangFireType WallbangFireType = EWallbangFireType::Origin;
    AActor* WallbangTarget = nullptr;
    FVector WallbangFireDirection;
    FVector WallbangFireLocation;

    scaffold::SubscriptionHandle SendSerialisedRpcHandle;

    class PlayerProviderFeature* PlayerProvider = nullptr;
    class AirFreezeFeature* AirFreezeFeatureInstance = nullptr;

    std::vector<std::pair<FVector, bool>> LastDirectionPoints;
};