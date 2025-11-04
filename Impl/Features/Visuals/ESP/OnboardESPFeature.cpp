#include "OnboardESPFeature.h"

//---------------------------------------------------------------------------

ShipOnboardESPFeature::ShipOnboardESPFeature() : 
  scaffold::Feature(xorstr_("Ship Onboard ESP"))
{
};

//---------------------------------------------------------------------------

bool ShipOnboardESPFeature::OnStart()
{
    return true;
}

//---------------------------------------------------------------------------

bool ShipOnboardESPFeature::OnEnd()
{
    return true;
}

//---------------------------------------------------------------------------

void ShipOnboardESPFeature::OnReset(EFeatureResetReason InResetReason)
{
}

//---------------------------------------------------------------------------

void ShipOnboardESPFeature::PostTick(float DeltaTime) {}

//---------------------------------------------------------------------------

void ShipOnboardESPFeature::Tick(float DeltaTime) {}

//---------------------------------------------------------------------------

bool ShipOnboardESPFeature::CanExecute()
{
    return GetController()
        && GetPlayer()
        && GetCamera()
        && GetPlayer()->GetCurrentShip(EShipTrackedType::OnShipDeckOrLadder);
}

//---------------------------------------------------------------------------

void ShipOnboardESPFeature::OnDiscard(float DeltaTime) {}

//---------------------------------------------------------------------------

void ShipOnboardESPFeature::OnExecute(float DeltaTime)
{
    auto MyShip = GetPlayer()->GetCurrentShip(EShipTrackedType::OnShipDeckOrLadder);

    AMapTable* Table = GetMapTable(MyShip);

    auto& Config = GetCfg();
    auto& Dm = DrawService::Get();
    auto Layer = Dm.PushLayer(DRAWLIST_VISUALS_BACK);

    auto Ship = GetPlayer()->GetCurrentShip(EShipTrackedType::OnShipDeckOrLadder);
    const auto MyLocation = GetPlayer()->K2_GetActorLocation();

    // Draw Map Pins
    if (Config.Visuals.Ships.Onboard.DrawMapPins->Get() && Table)
    {
        for (auto& Pin : Table->MapPins)
        {
            const FVector Location = FVector(Pin.X * 100, Pin.Y * 100, 7000.0f);

            FVector2D ScreenPos;
            if (!GetController()->ProjectWorldLocationToScreen(Location, ScreenPos))
                continue;

            FVector2D ScreenCenter = ScreenSize / 2;

            const int DistanceMeters = (int)Location.Distance(MyLocation) * 0.01f;

            bool IsHovered = ScreenPos.Distance(ScreenCenter) < 120.f;

            Dm.AddLabeledText(IsHovered, ICON_FA_MAP_PIN, std::vformat(xorstr_("{}m"), std::make_format_args(DistanceMeters)).c_str(), Config.Visuals.Ships.Colors.MapPins->Get(), Config.Visuals.Ships.Colors.MapPins->Get(), ScreenPos, EFontFace::PoppinsBold, EFontFace::PoppinsBold, DrawService::Get().ScaleFont(14.f), EAlign::Center, { 2.f, 2.f }, 0.5f, 4.0f);
        }
    }


    // Draw Holes
    if (Config.Visuals.Ships.Onboard.DrawDamageZones->Get() && MyShip->GetHullDamage())
    {
        for (auto& Hole : MyShip->GetHullDamage()->ActiveHullDamageZones)
        {
            const FVector Location = Hole->K2_GetActorLocation();

            FVector2D ScreenPos;
            if (!GetController()->ProjectWorldLocationToScreen(Location, ScreenPos))
                continue;

            Dm.AddBoxedText(ICON_FA_HAMMER, ScreenPos, Config.Visuals.Ships.Colors.DamageZones->Get(), DrawService::Get().ScaleFont(14.f), EAlign::Center, EFontFace::PoppinsBold, { 2.f, 2.f }, 0.5f, 4.0f);
        }
    }


    if (Config.Visuals.Ships.Onboard.DrawTrackedShips->Get() && Table)
    {
        for (auto& TrackedShip : Table->TrackedShips)
        {
            const FVector Location = FVector(TrackedShip.Location.X, TrackedShip.Location.Y, 500.0f);
            const int DistanceMeters = (int)(Location.Distance(GetCamera()->K2_GetActorLocation()) * 0.01f);

            // 3600m
            if (DistanceMeters < 3600)
                continue;

            FVector2D ScreenPos;
            if (!GetController()->ProjectWorldLocationToScreen(Location, ScreenPos))
                continue;

            Dm.AddBoxedText(std::vformat(xorstr_("{} ({})"), std::make_format_args(ICON_FA_SAILBOAT, DistanceMeters)).data(), ScreenPos, Config.Visuals.Ships.Colors.TrackedShips->Get(), DrawService::Get().ScaleFont(14.f), EAlign::Center, EFontFace::PoppinsBold, { 2.f, 2.f }, 0.5f, 4.0f);
        }
    }



    Dm.PopLayer();
}

//---------------------------------------------------------------------------

AMapTable* ShipOnboardESPFeature::GetMapTable(AShip* InShip)
{
    if (!InShip)
        return nullptr;

    if (InShip->ShipSizeObject == USmallShip::StaticClass())
    {
        if (((ABP_SmallShipTemplate_C*)InShip)->BP_SmallMapTable && ((ABP_SmallShipTemplate_C*)InShip)->BP_SmallMapTable->ChildActor)
            return (AMapTable*)((ABP_SmallShipTemplate_C*)InShip)->BP_SmallMapTable->ChildActor;
    }
    else if (InShip->ShipSizeObject == UMediumShip::StaticClass())
    {
        if (((ABP_MediumShipTemplate_C*)InShip)->BP_MediumMapTable && ((ABP_MediumShipTemplate_C*)InShip)->BP_MediumMapTable->ChildActor)
            return (AMapTable*)((ABP_MediumShipTemplate_C*)InShip)->BP_MediumMapTable->ChildActor;
    }
    else if (InShip->ShipSizeObject == ULargeShip::StaticClass())
    {
        if (((ABP_LargeShipTemplate_C*)InShip)->MapTable && ((ABP_LargeShipTemplate_C*)InShip)->MapTable->ChildActor)
            return (AMapTable*)((ABP_LargeShipTemplate_C*)InShip)->MapTable->ChildActor;
    }
    else if (InShip->ShipSizeObject == UReapersTributeShip::StaticClass())
    {
        if (((ABP_ReapersTributeShipTemplate_C*)InShip)->MapTable && ((ABP_ReapersTributeShipTemplate_C*)InShip)->MapTable->ChildActor)
            return (AMapTable*)((ABP_ReapersTributeShipTemplate_C*)InShip)->MapTable->ChildActor;
    }

    return nullptr;
}

//---------------------------------------------------------------------------

std::string_view ShipOnboardESPFeature::GetTrackedShipName(EWorldMapShipType InType)
{
    return "";
}

//---------------------------------------------------------------------------
