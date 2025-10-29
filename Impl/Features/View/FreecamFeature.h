#pragma once

#define FREECAM_FEATURE_USING_CINEMATIC_CAMERA 1
#define FREECAM_FEATURE_CINEMATIC_CAMERA_UPDATE_LOCAL_POS 1

class FreecamFeature : public scaffold::Feature {
public:

    FreecamFeature(std::shared_ptr<scaffold::Messenger> InMessenger);

private:

    enum class EFreecamUpdateState
    {
        Start,
        Current
    };
    enum class EFreecamMode
    {
        Client,
        Server
    };

    bool OnStart() override;
    bool OnEnd() override;
    void OnReset(EFeatureResetReason InResetReason) override;
    void OnDiscard(float DeltaTime) override;
    bool CanExecute() override;
    void PostTick(float DeltaTime) override;
    void Tick(float DeltaTime) override;
    void OnExecute(float DeltaTime) override;
    void OnConditionChange(bool bConditionChange) override;

private:

    bool ToggleCamera();

    bool EnableCamera();

    bool DisableCamera();

    void HandleFreecam();

    bool IsUpdateLocalPos();

public:

    bool IsActive() { return this->bActive; }

private:

    bool bActive = false;

    APlayerCameraManager* DummyCamera = nullptr;
    APlayerCameraManager* OriginalCamera = nullptr;

    FVector StartLocation;
    FVector CameraLocation;

    uint64_t LastRemoteUpdate = 0;

    uint64_t EnabledFreecamAt = 0;
    uint64_t DisabledFreecamAt = 0;

    bool bUsingServerFreecam = false;

    EFreecamUpdateState UpdateState = EFreecamUpdateState::Current;

    scaffold::shared_ptr<scaffold::Messenger> Dispatcher;

};