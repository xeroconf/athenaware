#include "ShipESPFeature.h"

//---------------------------------------------------------------------------

ShipESPFeature::ShipESPFeature(std::shared_ptr<scaffold::Messenger> InMessenger) : 
  scaffold::Feature(xorstr_("Ship ESP")), 
  Dispatcher(std::move(InMessenger)) 
{
};

//---------------------------------------------------------------------------

bool ShipESPFeature::OnStart()
{
    // Bind event listeners
    Dispatcher->Subscribe<ActorAggregationUpdateEvent>(ActorUpdateHandle, [this](const ActorAggregationUpdateEvent& Event) { OnActorAggregationUpdate(Event); });
    if (!ActorUpdateHandle.IsRegistered())
    {
        PLOG(error, "[ShipESPFeature] Failed to register ActorUpdateHandle");
    }

    //Dispatcher->Subscribe<ActorFocusStateChangeEvent>(ShipFocusChangeHandle, [](){});
    
    // Get all ships and load them into the context list as we may have missed spawn events.
    auto& CurrentShips = ActorService::Get().GetActorsOfType<AShip>();
    for (AShip* Ship : CurrentShips)
    {
        RegisterShip(Ship);
    }

    auto& CurrentProxies = ActorService::Get().GetActorsOfType<AShipNetProxy>();
    for (AShipNetProxy* Ship : CurrentProxies)
    {
        RegisterShip(Ship);
    }

    return true;
}

//---------------------------------------------------------------------------

bool ShipESPFeature::OnEnd()
{
    if (!Dispatcher->Unsubscribe(ActorUpdateHandle))
    {
        PLOG(critical, "[ShipESPFeature] Failed to unsubscribe from ActorUpdateHandle");
        return false;
    }

    /*if (!Dispatcher->Unsubscribe(ShipFocusChangeHandle))
    {
        PLOG(critical, "[ShipESPFeature] Failed to unsubscribe from ShipFocusChangeHandle");
        return false;
    }*/

    return true;
}

//---------------------------------------------------------------------------

void ShipESPFeature::OnReset(EFeatureResetReason InResetReason)
{
    // Free memory and clear the context list
    for (const auto& Entry : Contexts)
    {
        const auto Context = Entry.second;
        delete Context;
    }

    for (const auto& Entry : Proxies)
    {
        const auto Context = Entry.second;
        delete Context;
    }

    Proxies.clear();
    Contexts.clear();
}

//---------------------------------------------------------------------------

void ShipESPFeature::OnDiscard(float DeltaTime)
{

}

//---------------------------------------------------------------------------

bool ShipESPFeature::CanExecute()
{
    const bool IsEnabledInConfig = GetCfg().Visuals.Ships.DrawEnemyPlayerShips->Get() || GetCfg().Visuals.Ships.DrawFriendlyPlayerShips->Get() || GetCfg().Visuals.Ships.DrawAIShips->Get() || GetCfg().Visuals.Ships.DrawGhostShips->Get();

    return (GetController()
        && GetPlayer() && IsEnabledInConfig);
}

//---------------------------------------------------------------------------

void ShipESPFeature::PostTick(float DeltaTime)
{
}

//---------------------------------------------------------------------------

void ShipESPFeature::Tick(float DeltaTime)
{

}

//---------------------------------------------------------------------------

void ShipESPFeature::OnExecute(float DeltaTime)
{
    auto Controller = GetController();


    DrawService& Draw = DrawService::Get();
    Draw.PushLayer(DRAWLIST_VISUALS_FORE);
    const auto& Config = GetCfg();
    

    // Draw the far proxy ships
    for (const auto& Entry : Proxies)
    {
        AShipNetProxy* Ship = Entry.first;
        ShipProxyBindedContext* Context = Entry.second;

        FVector Location = Ship->K2_GetActorLocation();
        uint32_t Distance = GetPlayer()->K2_GetActorLocation().Distance(Location);
        

        if (Ship->OwningActor && Ship->OwningActor->IsA(AShip::StaticClass()))
            continue;
        

        FVector2D Screen;
        if (!GetController()->ProjectWorldLocationToScreen(Ship->K2_GetActorLocation(), Screen))
            continue;
        

        ImColor RenderColor = Config.Visuals.Ships.Colors.EnemyShips->Get();
        

        if (Context->IsAI)
            RenderColor = Config.Visuals.Ships.Colors.AIShips->Get();
        

        uint32_t DistanceMeters = Distance * 0.01f;
        

        if (Ship->EmissaryFlagActive)
            Draw.AddBoxedText(std::vformat(TemplateEmissaryDistant, std::make_format_args(Context->SizeName, DistanceMeters)).c_str(), Screen, RenderColor, DrawService::Get().ScaleFont(14.f), EAlign::Center, EFontFace::PoppinsBold, { 2, 2 }, 0.5f * RenderColor.A(), 4.0f);
        else
            Draw.AddBoxedText(std::vformat(TemplateDistant, std::make_format_args(Context->SizeName, DistanceMeters)).c_str(), Screen, RenderColor, DrawService::Get().ScaleFont(14.f), EAlign::Center, EFontFace::PoppinsBold, { 2, 2 }, 0.5f * RenderColor.A(), 4.0f);
    }

    bool DoDelegatedUpdate = Timestamp::GetTimeSince(LastDelegatedUpdate) > 4000;

    for (const auto& Context : Contexts)
    {
        

        AShip* Ship = Context.first;
        auto Data = Context.second;

        FVector Location = Ship->K2_GetActorLocation();
        FVector ESPLocation = Location + FVector(0.f, 0.f, SHIP_ESP_DRAW_HEIGHT);
        

        Data->bIsOnScreen = Controller->ProjectWorldLocationToScreen(Location, Data->ScreenPosition);
        
        Data->bIsVisualsPositionOnScreen = Controller->ProjectWorldLocationToScreen(ESPLocation, Data->VisualsDrawScreenPosition);
        Data->bIsSinking = Ship->SinkingComponent ? Ship->SinkingComponent->IsSinking() : false;
        
        Data->DistanceMeters = Location.Distance(GetPlayer()->K2_GetActorLocation()) * 0.01f;
        
        Data->Holes = Ship->GetHullDamage() ? Ship->GetHullDamage()->ActiveHullDamageZones.Num() : 0;
        
        Data->NormalizedWaterAmount = Ship->GetInternalWater() ? Ship->GetInternalWater()->GetNormalizedWaterAmount() : 1.0f;
        

        if (Data->bIsOnScreen)
        {
            auto ScreenCenter = ScreenSize / 2;
            Data->bIsHovered = Data->VisualsDrawScreenPosition.Distance(ScreenCenter) < SHIP_ESP_HOVER_THRESHOLD;

            const float DrawerTarget = Data->bIsHovered ? 1.0f : 0.0f;
            Data->DrawerPos = UKismetMathLibrary::FInterpTo(Data->DrawerPos, DrawerTarget, DeltaTime, SHIP_ESP_DRAWER_INTERP_SPEED);
        }
    }

    // Draw the cached/full ships
    for (const auto& Context : Contexts)
    {
        AShip* Ship = Context.first;
        auto Data = Context.second;
        

        if (!Data->bIsVisualsPositionOnScreen)
            continue;

        ImColor Color = GetTeamColor(Data->Team);

        // Disregard drawing if we've disabled certain types of ship teams
        if ((Data->Team == EShipTeam::Crew || Data->Team == EShipTeam::Alliance) && !Config.Visuals.Ships.DrawFriendlyPlayerShips->Get())
            continue;
        

        if (Data->Team == EShipTeam::Enemy && !Config.Visuals.Ships.DrawEnemyPlayerShips->Get())
            continue;
        

        if (Data->Team == EShipTeam::AI && !Config.Visuals.Ships.DrawAIShips->Get())
            continue;
        
        //
        // Draw an effect on the ship to bring it into focus. For example, point out that this ship is being targeted in cannon aimbot
        //if (Data->bHasFocus && GetCfg().Visuals.Ships.bShowFocusEffect)
        {
            //ImColor FocusColor = ImColor(Color.Value.x, Color.Value.y, Color.Value.z, 0.5f * Data->DrawerPos);
            //DrawList->AddRadialGradient(Data->bIsVisualsPositionOnScreen, 64.0f, FocusColor, IM_COL32(255, 255, 255, 0));
        }
        

        bool bHasFlag = Ship->EmissaryFlagActive;
        
        auto DistanceMetersRounded = (uint32_t)(Data->DistanceMeters);

        std::string MainLabel;

        if (Data->bIsSinking)
        {
            auto Args = std::make_format_args(Data->SizeName, DistanceMetersRounded);

            MainLabel = std::vformat(TemplateSinking, Args);
        }
        else
        {
            auto FlagPlace = bHasFlag ? "\xef\x80\xa4" : "";

            auto Args = std::make_format_args(FlagPlace, Data->SizeName, Data->Holes, DistanceMetersRounded);

            MainLabel = std::vformat(TemplateBase, Args);
        }
        
        FVector2D NewPos = Data->VisualsDrawScreenPosition;
        FVector2D BoxPadding = { 2, 2 };
        float BarHeight = 6.0f;
        float FontSize = DrawService::Get().ScaleFont(14.f);

        ImFont* FontFace = Draw.GetFont(EFontFace::PoppinsBold);
        ImVec2 Size = FontFace->CalcTextSizeA(FontSize, FLT_MAX, 0.f, MainLabel.data());
        NewPos.X -= Size.x * 0.5f;
        NewPos.Y -= Size.y;

        FVector2D From = { NewPos.X - BoxPadding.X, NewPos.Y - BoxPadding.Y };
        FVector2D To = { (NewPos.X + Size.x) + BoxPadding.X, (NewPos.Y + Size.y) + BoxPadding.Y };

        Draw.AddFilledRectangle(From, To, ImColor(18, 22, 25, (0.5f * Color.A())), 4.0f);
        Draw.AddText(MainLabel, NewPos, Color, EAlign::Left, EFontFx::None, FontSize, EFontFace::PoppinsBold);
        From.Y += Size.y + BoxPadding.Y + 5;

        if (!Data->bIsSinking)
        {
            float Width = To.X - From.X;
            Draw.AddFilledRectangle(From, { From.X + (Width), From.Y + BarHeight }, ImColor(18, 22, 25, (0.5f * Color.A())* Data->DrawerPos), 4.0f);
            Draw.AddFilledRectangle(From, { From.X + (Width * (1.0f - Data->NormalizedWaterAmount)), From.Y + BarHeight }, Color.WithAlpha(Data->DrawerPos), 4.0f);
        }
    }

    Draw.PopLayer();
};

//---------------------------------------------------------------------------

void ShipESPFeature::RegisterShip(AActor* InShip)
{
    if (!InShip)
        return;
    
    if (InShip->IsA(AShip::StaticClass()))
    {
        AShip* Ship = (AShip*)InShip;

        // The same ship shouldn't be re-created if it's already added but we should still account for this edge case
        const auto Context = Contexts.find(Ship);
        
        if (Context != Contexts.end())
        {
            PLOG(warn, "[ShipESPFeature] Attempted to add a context binding for a ship that already exists");
            return;
        }
        
        ShipBindedContext* ShipContext = new ShipBindedContext();
        
        // Assign team to this ship context. If no conditions apply, default as an enemy..
        if (Ship->CrewOwnershipComponent)
        {
            FGuid MyCrewId = UCrewFunctions::GetCrewIdFromActor(GetWorld(), GetPlayer());

            if (MyCrewId == Ship->CrewOwnershipComponent->CachedCrewId)
                ShipContext->Team = EShipTeam::Crew;
            else if (UAllianceBlueprintLibrary::AreCrewsInSameAlliance(GetWorld(), MyCrewId, Ship->CrewOwnershipComponent->CachedCrewId))
                ShipContext->Team = EShipTeam::Alliance;
        }
        
        ShipContext->SizeName = GetShipSizeName(Ship);

        PLOGF(debug, "[ShipESPFeature] Added context binding for ship '{0}'", ShipContext->SizeName);

        Contexts.insert(std::pair(Ship, ShipContext));
    }
    else if (InShip->IsA(AShipNetProxy::StaticClass()))
    {
        AShipNetProxy* Ship = (AShipNetProxy*)InShip;

        const auto Context = Proxies.find(Ship);

        if (Context != Proxies.end())
        {
            PLOG(warn, "[ShipESPFeature] Attempted to add a context binding for a proxy ship that already exists");
            return;
        }
        
        ShipProxyBindedContext* ShipContext = new ShipProxyBindedContext();

        ShipContext->SizeName = GetProxySizeName(Ship);
        
        ShipContext->IsAI = Ship->IsA(AILargeProxy) || Ship->IsA(AISmallProxy);
        
        PLOGF(debug, "[ShipESPFeature] Added context binding for proxy ship '{0}'", ShipContext->SizeName);
        
        Proxies.insert(std::pair(Ship, ShipContext));
    }
}

//---------------------------------------------------------------------------

void ShipESPFeature::DeregisterShip(AActor* InShip)
{
    if (InShip->IsA(AShip::StaticClass()))
    {
        const auto Context = Contexts.find((AShip*)InShip);

        if (Context != Contexts.end())
        {
            PLOGF(debug, "[ShipESPFeature] Removed context binding for ship '{0}'", Context->second->SizeName);
            delete Context->second;
            Contexts.erase(Context);
        }
        else
        {
            PLOG(warn, "[ShipESPFeature] Attempted to remove binding for a ship that doesn't exist");
        }
    }
    else if (InShip->IsA(AShipNetProxy::StaticClass()))
    {
        const auto Context = Proxies.find((AShipNetProxy*)InShip);

        if (Context != Proxies.end())
        {
            PLOGF(debug, "[ShipESPFeature] Removed context binding for proxy ship '{0}'", Context->second->SizeName);
            delete Context->second;
            Proxies.erase(Context);
        }
        else
        {
            PLOG(warn, "[ShipESPFeature] Attempted to remove binding for a proxy ship that doesn't exist");
        }
    }
}

//---------------------------------------------------------------------------

void ShipESPFeature::OnActorAggregationUpdate(const ActorAggregationUpdateEvent& Event)
{
    
    if (Event.AssignedClass == AShip::StaticClass() || Event.AssignedClass == AShipNetProxy::StaticClass())
    {
        switch (Event.UpdateType)
        {
        case ActorAggregationUpdateEvent::EType::Added:
            RegisterShip(Event.Actor);
            break;

        case ActorAggregationUpdateEvent::EType::Removed:
            DeregisterShip(Event.Actor);
            break;
        }
    }
    
}

//---------------------------------------------------------------------------

std::string ShipESPFeature::GetShipSizeName(AShip* Ship)
{
    if (Ship->ShipSizeObject == USmallShip::StaticClass())
        return T_SLOOP;
    else if (Ship->ShipSizeObject == UMediumShip::StaticClass())
        return T_BRIGANTINE;
    else if (Ship->ShipSizeObject == ULargeShip::StaticClass())
        return T_GALLEON;
    else if (Ship->ShipSizeObject == UAILargeShip::StaticClass())
        return T_AI_GALLEON;
    else if (Ship->ShipSizeObject == UAISmallShip::StaticClass())
        return T_AI_SLOOP;
    else if (Ship->ShipSizeObject == UReapersTributeShip::StaticClass())
        return T_BURNING_BLADE;

    return T_SHIP;
}

//---------------------------------------------------------------------------

const ImColor& ShipESPFeature::GetTeamColor(EShipTeam InTeam)
{
    const auto& Config = GetCfg();

    if (InTeam == EShipTeam::Crew)
        return Config.Visuals.Ships.Colors.FriendlyShips->Get();
    else if (InTeam == EShipTeam::Alliance)
        return Config.Visuals.Ships.Colors.FriendlyShips->Get();
    else if (InTeam == EShipTeam::Enemy)
        return Config.Visuals.Ships.Colors.EnemyShips->Get();
    else if (InTeam == EShipTeam::AI)
        return Config.Visuals.Ships.Colors.AIShips->Get();

    return Config.Visuals.Ships.Colors.EnemyShips->Get();
}

//---------------------------------------------------------------------------

std::string ShipESPFeature::GetProxySizeName(AShipNetProxy* InShipProxy)
{
    if (InShipProxy)
    {
        if (InShipProxy->IsA(AShipNetProxy::SmallProxy()))
            return T_SLOOP;
        else if (InShipProxy->IsA(AShipNetProxy::MediumProxy()))
            return T_BRIGANTINE;
        else if (InShipProxy->IsA(AShipNetProxy::LargeProxy()))
            return T_GALLEON;
        else if (InShipProxy->IsA(AShipNetProxy::AILargeProxy()))
            return T_AI_GALLEON;
        else if (InShipProxy->IsA(AShipNetProxy::AISmallProxy()))
            return T_AI_SLOOP;
        else if (InShipProxy->IsA(AShipNetProxy::ReapersTribute()))
            return T_BURNING_BLADE;
    }

    return T_SHIP;
}

//---------------------------------------------------------------------------