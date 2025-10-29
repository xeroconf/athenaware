#include "NoCannonFlashFeature.h"
//---------------------------------------------------------------------------

NoCannonFlashFeature::NoCannonFlashFeature() : 
  scaffold::Feature("Cannon Flash Disabler")
{
};

//---------------------------------------------------------------------------

bool NoCannonFlashFeature::OnStart()
{
	return true;
}

//---------------------------------------------------------------------------

bool NoCannonFlashFeature::OnEnd()
{
	return true;
}

//---------------------------------------------------------------------------

void NoCannonFlashFeature::OnReset(EFeatureResetReason InResetReason) 
{
    ActiveCannon = nullptr;
    CachedVfx = nullptr;
}

//---------------------------------------------------------------------------

void NoCannonFlashFeature::PostTick(float DeltaTime) {}

//---------------------------------------------------------------------------

void NoCannonFlashFeature::Tick(float DeltaTime) {}

//---------------------------------------------------------------------------

bool NoCannonFlashFeature::CanExecute()
{
	return GetCfg().Aim.Cannon.DisableFireFX->Get() && GetPlayer() && IsMountedToCannon();
}

//---------------------------------------------------------------------------

void NoCannonFlashFeature::OnConditionChange(bool bConditionChange) 
{
    if (bConditionChange)
    {
        if (!ActiveCannon)
        {
            ACannon* Cannon = (ACannon*)GetPlayer()->GetAttachParentActor();

            CachedVfx = Cannon->MuzzleFlashVfxFirstPerson;
            ActiveCannon = (ACannon*)GetPlayer()->GetAttachParentActor();

            Cannon->MuzzleFlashVfxFirstPerson = nullptr;
        }
    }
    else
    {
        if (ActiveCannon)
        {
            ActiveCannon->MuzzleFlashVfxFirstPerson = CachedVfx;

            ActiveCannon = nullptr;
            CachedVfx = nullptr;
        }
    }
}

//---------------------------------------------------------------------------

void NoCannonFlashFeature::OnExecute(float DeltaTime) 
{

}

//---------------------------------------------------------------------------

void NoCannonFlashFeature::OnDiscard(float DeltaTime) {}

//---------------------------------------------------------------------------

bool NoCannonFlashFeature::IsMountedToCannon()
{
    const ACannon* Attachment = (ACannon*)GetPlayer()->GetAttachParentActor();
    if (Attachment && Attachment->IsA(ACannon::StaticClass()))
    {
        // We're inside the cannon tube, disregard
        if (Attachment->LoadableComponent && Attachment->LoadableComponent->LoadableComponentState.LoadedItem == GetPlayer())
            return false;

        return true;
    }
    return false;
}

//---------------------------------------------------------------------------