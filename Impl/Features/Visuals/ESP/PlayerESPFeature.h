#pragma once

class PlayerESPFeature : public scaffold::Feature 
{
public:

    PlayerESPFeature();

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

    // !! Moved to player provider !!
    // Actor focus has no use right now, but I think we were wanting to allow other features to mark
    // specific actors (e.g put a circle around their player) to mark targets. The idea behind this was
    // to allow features like aimbot to mark the actor it's targeting. It'd be up this feature to decide
    // how the ESP will change to show this.
    /*void OnActorFocusStateChange(const ActorFocusStateChangeEvent& Event);*/

private:

   class PlayerProviderFeature* PlayerProvider = nullptr;
   
};