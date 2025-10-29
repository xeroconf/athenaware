#include "AntiDrunkFeature.h"

//---------------------------------------------------------------------------

AntiDrunkFeature::AntiDrunkFeature() : 
  scaffold::Feature("Anti Drunk")
{
};

//---------------------------------------------------------------------------

bool AntiDrunkFeature::OnStart() { return true; }

//---------------------------------------------------------------------------

bool AntiDrunkFeature::OnEnd() { return true; }

//---------------------------------------------------------------------------

void AntiDrunkFeature::OnReset(EFeatureResetReason InResetReason) {}

//---------------------------------------------------------------------------

void AntiDrunkFeature::OnDiscard(float DeltaTime) {}

//---------------------------------------------------------------------------

bool AntiDrunkFeature::CanExecute()
{
    return GetCfg().Other.Misc.EnableNoDrunkness->Get()
        && GetPlayer()
        && GetPlayer()->DrunkennessComponent;
}

//---------------------------------------------------------------------------

void AntiDrunkFeature::PostTick(float DeltaTime) {}

//---------------------------------------------------------------------------

void AntiDrunkFeature::Tick(float DeltaTime) {}

//---------------------------------------------------------------------------

void AntiDrunkFeature::OnExecute(float DeltaTime)
{
    UDrunkennessComponent* DrunkComponent = GetPlayer()->DrunkennessComponent;

    DrunkComponent->TargetDrunkenness[0] = 0.0f;
    DrunkComponent->TargetDrunkenness[1] = 0.0f;
}

//---------------------------------------------------------------------------