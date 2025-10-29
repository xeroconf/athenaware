#include "CannonAutoLoadFeature.h"
//---------------------------------------------------------------------------

CannonAutoLoadFeature::CannonAutoLoadFeature(std::shared_ptr<scaffold::Messenger> InMessenger) : 
  scaffold::Feature(xorstr_("Cannon Auto Load")), 
  Dispatcher(std::move(InMessenger)) 
{
};

//---------------------------------------------------------------------------

bool CannonAutoLoadFeature::OnStart()
{
	return true;
}

//---------------------------------------------------------------------------

bool CannonAutoLoadFeature::OnEnd()
{
	return true;
}

//---------------------------------------------------------------------------

void CannonAutoLoadFeature::OnReset(EFeatureResetReason InResetReason) {}

//---------------------------------------------------------------------------

void CannonAutoLoadFeature::PostTick(float DeltaTime) {}

//---------------------------------------------------------------------------

void CannonAutoLoadFeature::Tick(float DeltaTime) {}

//---------------------------------------------------------------------------

bool CannonAutoLoadFeature::CanExecute()
{
	return athena::GetPlayer() && GetMountedOnCannon();
}

//---------------------------------------------------------------------------

void CannonAutoLoadFeature::OnConditionChange(bool bConditionChange) 
{
    if (bConditionChange) // Mounting to a cannon or in state to use
    {
        // Check if we have already enabled this prior to exiting a prior execution
        // as we want to append it to the enabled feature list.
        if (bIsActive)
        {
            FireFeatureListEvent(true);
        }
    }
    else // Exiting cannon or not in state to use cannon
    {
        // Always remove from list when we are not executing.
        FireFeatureListEvent(false);
    }
}

//---------------------------------------------------------------------------

void CannonAutoLoadFeature::OnExecute(float DeltaTime) 
{
    const auto Cannon = GetMountedOnCannon();

    if (!Cannon)
    {
        return;
    }

    if (InputService::Get().WasKeyPressed(GetCfg().Aim.Cannon.AutoLoadToggleKey->Get()))
    {
        bIsActive = !bIsActive;

        // Handle feature top-right
        if (bIsActive)
        {
            FireFeatureListEvent(true);
        }
        else
        {
            FireFeatureListEvent(false);

            // Stop unloading cannon if we are currently already loading, but stop midway through.
            athena::GetPlayer()->InteractWith(Cannon, Cannon->StopLoadingCannonInputId);
        }
    }

    if (!bIsActive)
        return;

    // Prevent spamming interaction, delay it by an amount.
    if (Timestamp::GetTimeSince(LastLoadAttempt) < 550)
        return;

    // Do not auto load if we've got a cannon loaded.
    if (Cannon->LoadedItemInfo)
        return;

    
    const auto Player = athena::GetPlayer();

    athena::GetPlayer()->InteractWith(Cannon, Cannon->StartLoadingCannonInputId);

    LastLoadAttempt = Timestamp::Now();
}

//---------------------------------------------------------------------------

void CannonAutoLoadFeature::OnDiscard(float DeltaTime) {}

//---------------------------------------------------------------------------

athena::ACannon* CannonAutoLoadFeature::GetMountedOnCannon()
{
    auto Player = athena::GetPlayer();

    if (!Player)
        return nullptr;

    athena::ACannon* Attachment = (athena::ACannon*)Player->GetAttachParentActor();

    if (Attachment && Attachment->IsA(athena::ACannon::StaticClass()))
    {
        // We're inside the cannon tube, disregard
        if (Attachment->LoadableComponent && Attachment->LoadableComponent->LoadableComponentState.LoadedItem == athena::GetPlayer())
            return nullptr;

        return Attachment;
    }

    return nullptr;
}

//---------------------------------------------------------------------------

void CannonAutoLoadFeature::FireFeatureListEvent(bool bInNewState)
{
    FeatureToggledEvent ToggleEvent(
        GetName(), 
        bInNewState
    );

    Dispatcher->Emit(
        ToggleEvent
    );
}

//---------------------------------------------------------------------------