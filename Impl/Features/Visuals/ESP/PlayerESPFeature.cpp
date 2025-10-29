#include "PlayerESPFeature.h"

//---------------------------------------------------------------------------

PlayerESPFeature::PlayerESPFeature() : 
  scaffold::Feature(xorstr_("Player ESP"))
{
};

//---------------------------------------------------------------------------

bool PlayerESPFeature::OnStart()
{
    // Player provider feature hacky solution!! ActorService allows data to be stored on actors via the key-value system and/or embedded storage. 
    // For now we'll keep the data provider at feature level until we can come back to this and improve it.
    // 
    // TODO: 27/02/2025 - Actor embedded storage will to be utilized over key-value for priority data. Replace this once we're using PlayerProviderV2Feature.
    PlayerProvider = (PlayerProviderFeature*)MainDomain::Get().GetFeatureManager().GetFeature(xorstr_("F_PROVIDER_PLAYER"));
    if (!PlayerProvider)
    {
        PLOG(error, "[PlayerESPFeature] Failed to get an instance of PlayerProviderFeature");
        return false;
    }

    return true;
}

//---------------------------------------------------------------------------

bool PlayerESPFeature::OnEnd()
{
    // Shouldn't need to do this but it'd be best we keep this feature with
    // no saved state in the case we decide to disable the provier feature via
    // remote client configuration.
    PlayerProvider = nullptr;

    return true;
}

//---------------------------------------------------------------------------

void PlayerESPFeature::OnReset(EFeatureResetReason InReason)
{

}

//---------------------------------------------------------------------------

void PlayerESPFeature::OnDiscard(float DeltaTime)
{

}

//---------------------------------------------------------------------------

bool PlayerESPFeature::CanExecute()
{
    return GetCfg().Visuals.Players.Enabled->Get() && GetController() && GetPlayer() && GetCamera();
}

//---------------------------------------------------------------------------

void PlayerESPFeature::PostTick(float DeltaTime)
{
}

//---------------------------------------------------------------------------

void PlayerESPFeature::Tick(float DeltaTime) {}

//---------------------------------------------------------------------------

void PlayerESPFeature::OnExecute(float DeltaTime)
{
    const auto Controller = GetController();
    const auto& Config = GetCfg();
    auto& Dm = DrawService::Get();

    Dm.PushLayer(DRAWLIST_VISUALS_FORE);

    for (const auto& Entry : PlayerProvider->GetPlayers())
    {
        AAthenaPlayerCharacter* Player = Entry.first;
        auto& Ctx = Entry.second;

        if (!Ctx.IsOnScreen)
            continue;

        if (Ctx.Team == PlayerProviderFeature::ETeam::Crew && !Config.Visuals.Players.DrawCrewMembers->Get())
            continue;

        ImColor BonesColor = Config.Visuals.Players.Colors.EnemyBonesVisible->Get();
        ImColor BoxColor = Config.Visuals.Players.Colors.EnemyBoxVisible->Get();

        // Get config colour based on the team
        if (Ctx.Team == PlayerProviderFeature::ETeam::Crew || Ctx.Team == PlayerProviderFeature::ETeam::Alliance)
        {
            BonesColor = Ctx.IsVisible ? Config.Visuals.Players.Colors.FriendlyBonesVisible->Get() : Config.Visuals.Players.Colors.FriendlyBonesInvisible->Get();
            BoxColor = Ctx.IsVisible ? Config.Visuals.Players.Colors.FriendlyBoxInvisible->Get() : Config.Visuals.Players.Colors.FriendlyBoxInvisible->Get();
        }
        else
        {
            BonesColor = Ctx.IsVisible ? Config.Visuals.Players.Colors.EnemyBonesVisible->Get() : Config.Visuals.Players.Colors.EnemyBonesInvisible->Get();
            BoxColor = Ctx.IsVisible ? Config.Visuals.Players.Colors.EnemyBoxVisible->Get() : Config.Visuals.Players.Colors.EnemyBoxInvisible->Get();
        }


        // TODO: Investigate if we can handle bones in a better way.
        // I'm not sure if this is good?
        if (Player->Mesh && Config.Visuals.Players.DrawBones->Get())
        {
            for (const std::vector<int32_t>& Part : SDK::Bones)
            {
                FVector2D PrevBoneLocation{};

                for (const int32_t Bone : Part)
                {
                    FVector OutPos;
                    FRotator OutRot;
                    FName BoneName = Player->Mesh->GetBoneName(Bone);
                    Player->Mesh->TransformFromBoneSpace(BoneName, { 0,0,0 }, { 0,0,0 }, OutPos, OutRot);

                    FVector2D ScreenPos;
                    if (Controller->ProjectWorldLocationToScreen(OutPos, ScreenPos))
                    {
                        if (OutPos.Size() == 0 || ScreenPos.Size() == 0)
                            continue;

                        if (PrevBoneLocation.Size() == 0)
                        {
                            PrevBoneLocation = ScreenPos;
                            continue;
                        }

                        Dm.AddLine(ScreenPos, PrevBoneLocation, BonesColor, 1.f);

                        PrevBoneLocation = ScreenPos;
                    }
                }
            }
        }


        // Eh...
        FBox2D BoundingBox = PlayerProviderFeature->GetBoundingBox(Player, Ctx.Extent);

        float BoundWidth = BoundingBox.Max.X - BoundingBox.Min.X;
        float BoundHeight = BoundingBox.Max.Y - BoundingBox.Min.Y;

        // Usernames should always be available as they're retrieved and assigned via playerlist.
        // In the case names don't replicate from server, we won't display a name at all.
        if (Config.Visuals.Players.DrawNames->Get() && !Ctx.Username.empty())
        {
            FVector2D NamePos = FVector2D(BoundingBox.Min.X + ((BoundingBox.Max.X - BoundingBox.Min.X) / 2), BoundingBox.Min.Y) - FVector2D(0, 8);
            Dm.AddBoxedText(Ctx.Username.data(), NamePos, ImColor(255, 255, 255, 255), Dm.ScaleFont(12.f), EAlign::Center, EFontFace::PoppinsBold, { 1, 1 }, 0.5, 8.0f);
        }

        // Health component should always exist, but in the edge case it isn't we'll just assume they have no health
        float Health = Player->HealthComponent ? Player->HealthComponent->CurrentHealthInfo.Health : 0.0f;

        // Draw health if enabled. We are also not going to draw it if they have no HP (dead)
        if (Config.Visuals.Players.DrawHealth->Get() && Health > 0.0f)
        {
            float HealthNormal = Health / 100.f;

            FVector2D PosA = BoundingBox.Min + FVector2D(-6, 0);
            FVector2D PosB = BoundingBox.Min + FVector2D(-4, BoundHeight);

            FVector2D HealthExtent = FVector2D(
                PosA.X,
                BoundingBox.Min.Y - (BoundHeight * HealthNormal)
            ) + FVector2D(0, BoundHeight);

            int G = HealthNormal * 255.f;
            int R = 255.f - G;
            int B = 128.f;

            Dm.AddFilledRectangle(PosA, PosB, ImColor(18, 22, 25, 255 / 2), 0.f);
            Dm.AddFilledRectangle(HealthExtent, PosB, ImColor(R, G, B, 255), 0.f);
        }

        if (Config.Visuals.Players.DrawBoxes->Get())
        {
            const float CornerExtentW = BoundWidth / 5.f;
            const float CornerExtentH = BoundWidth / 5.f;

            const FVector2D TopLeft = BoundingBox.Min;
            const FVector2D TopRight = BoundingBox.Min + FVector2D(BoundWidth, 0);
            const FVector2D BottomLeft = BoundingBox.Min + FVector2D(0, BoundHeight);
            const FVector2D BottomRight = BoundingBox.Max;

            // Top Left
            Dm.AddLine(TopLeft, TopLeft + FVector2D(CornerExtentW, 0), BoxColor, 1.0f);
            Dm.AddLine(TopLeft, TopLeft + FVector2D(0, CornerExtentH), BoxColor, 1.0f);

            // Top Right
            Dm.AddLine(TopRight, TopRight - FVector2D(CornerExtentW, 0), BoxColor, 1.0f);
            Dm.AddLine(TopRight, TopRight + FVector2D(0, CornerExtentH), BoxColor, 1.0f);

            // Bottom Left
            Dm.AddLine(BottomLeft, BottomLeft + FVector2D(CornerExtentW, 0), BoxColor, 1.0f);
            Dm.AddLine(BottomLeft, BottomLeft - FVector2D(0, CornerExtentH), BoxColor, 1.0f);

            // Bottom Right
            Dm.AddLine(BottomRight, BottomRight - FVector2D(CornerExtentW, 0), BoxColor, 1.0f);
            Dm.AddLine(BottomRight, BottomRight - FVector2D(0, CornerExtentH), BoxColor, 1.0f);
        }


        if (Config.Visuals.Players.Snaplines.Enabled->Get())
        {
            // Default the start pos to the screen center
            FVector2D LineStartPosScreen = ScreenSize / 2;

            switch ((ESnaplineType)Config.Visuals.Players.Snaplines.Mode->Get())
            {
                case ESnaplineType::Top:
                    LineStartPosScreen = FVector2D(ScreenSize.X / 2, 0.f);
                    break;
                case ESnaplineType::Bottom:
                    LineStartPosScreen = FVector2D(ScreenSize.X / 2, ScreenSize.Y);
                    break;
            }
 
            // Inherit the colour of the box ESP.
            // We can allow the colour to be configured if users suggest it.
            Dm.AddLine(LineStartPosScreen, Ctx.ScreenPosition, BoxColor, 1.0f);
        }
    }

    Dm.PopLayer();
}

//---------------------------------------------------------------------------
