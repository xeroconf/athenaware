#include "HarpoonAimbotFeature.h"

//---------------------------------------------------------------------------

HarpoonAimbotFeature::HarpoonAimbotFeature(std::shared_ptr<scaffold::Messenger> InMessenger) : 
  scaffold::Feature(xorstr_("Harpoon Aimbot")), 
  Dispatcher(std::move(InMessenger))
{
};

//---------------------------------------------------------------------------

bool HarpoonAimbotFeature::OnStart()
{
    if (!RequestFireFunction)
    {
        RequestFireFunction = UObject::FindObject<UFunction>(xorstr_("Function Athena.HarpoonLauncher.Server_RequestFire"));
    }

    return true;
}

//---------------------------------------------------------------------------

bool HarpoonAimbotFeature::OnEnd()
{
    return true;
}

//---------------------------------------------------------------------------

void HarpoonAimbotFeature::OnDiscard(float DeltaTime)
{

}

//---------------------------------------------------------------------------

void HarpoonAimbotFeature::OnReset(EFeatureResetReason InResetReason)
{
    Target = nullptr;
}

//---------------------------------------------------------------------------

bool HarpoonAimbotFeature::CanExecute()
{
    return IsUsingHarpoon()
        && GetCfg().Aim.Harpoon.Enabled->Get()
        && InputService::Get().IsKeyPressed(ImGuiKey_MouseRight);
}

//---------------------------------------------------------------------------

void HarpoonAimbotFeature::PostTick(float DeltaTime)
{
    OnReset(EFeatureResetReason::Internal);
}

//---------------------------------------------------------------------------

void HarpoonAimbotFeature::Tick(float DeltaTime)
{

}

//---------------------------------------------------------------------------

void HarpoonAimbotFeature::OnExecute(float DeltaTime)
{
    const auto Harpoon = (AHarpoonLauncher*)GetPlayer()->GetAttachParentActor();

    AFloatingItemProxy* BestItem = nullptr;
    float BestDistance = FEATURE_HARPOONAIMBOT_FOV;
    
    for (AItemProxy* Proxy : ActorService::Get().GetActorsOfType<AItemProxy>())
    {
        // Only target floating items
        if (!Proxy->IsA(AFloatingItemProxy::StaticClass()))
            continue;

        // Useless/not valid item. Lets ignore it!
        if (!Proxy->ItemInfo || Proxy->ItemInfo->CanBeStoredInInventory)
            continue;

        FVector2D Screen;
        if (!GetController()->ProjectWorldLocationToScreen(Proxy->K2_GetActorLocation(), Screen))
            continue;

        FVector2D ScreenCenter = ScreenSize / 2;
        const float DistanceFromCenter = Screen.Distance(ScreenCenter);

        if (DistanceFromCenter > BestDistance)
            continue;

        BestDistance = DistanceFromCenter;
        BestItem = (AFloatingItemProxy*)Proxy;
    }

    // Begin targeting if we have a target
    if (BestItem)
    {
        Target = BestItem;

        FRotator LookAt = UKismetMathLibrary::FindLookAtRotation(
            GetCamera()->K2_GetActorLocation(), Target->K2_GetActorLocation()
        );

        FRotator Delta = UKismetMathLibrary::NormalizedDeltaRotator(
            LookAt,
            GetCamera()->K2_GetActorRotation()
        );

        // Ensure to stay within the harpoons angle bound
        if (Delta.Pitch >= -35.f && Delta.Pitch <= 67.f && Delta.Yaw <= 50.f && Delta.Yaw >= -50)
        {
            // Apply input to get near the item so when we fire,
            // we can intercept the RPC and apply our aim-assist item parameter.
            Harpoon->PitchInput = Delta.Pitch;
            Harpoon->YawInput = Delta.Yaw;
        }

    }
}

//---------------------------------------------------------------------------

void HarpoonAimbotFeature::OnProcessRemoteFunction(ProcessRemoteFunctionEvent& Event)
{
    if (Event.Function == RequestFireFunction)
    {
        const auto MyHarpoon = (AHarpoonLauncher*)GetPlayer()->GetAttachParentActor();

        // Check if this harpoon is the one we're using
        if (Event.Actor != MyHarpoon)
            return;

        // No target, disregard
        if (!Target || !Target->BaseComponent)
            return;

        struct Server_RequestFireParams {
            float InPitch;
            float InYaw;
            class UPrimitiveComponent* ClientDesiredTargetComponent;
        };

        Server_RequestFireParams* Params = (Server_RequestFireParams*)Event.Parameters;

        // Override the fire request with our item target
        Params->ClientDesiredTargetComponent = (UPrimitiveComponent*)Target->BaseComponent;
    }

}

//---------------------------------------------------------------------------

bool HarpoonAimbotFeature::IsUsingHarpoon()
{
    if (!GetController() || !IsInGameplay() || !GetPlayer())
        return false;

    const auto Attachment = GetPlayer()->GetAttachParentActor();

    if (!Attachment)
        return false;

    return Attachment->IsA(AHarpoonLauncher::StaticClass());
}

//---------------------------------------------------------------------------
