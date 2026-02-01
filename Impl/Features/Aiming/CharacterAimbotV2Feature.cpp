#include "CharacterAimbotV2Feature.h"

//---------------------------------------------------------------------------

CharacterAimbotV2Feature::CharacterAimbotV2Feature(std::shared_ptr<scaffold::Messenger> InMessenger) : 
  scaffold::Feature(xorstr_("Character Aimbot (Version 2)")), 
  Dispatcher(std::move(InMessenger)) 
{
};

//---------------------------------------------------------------------------

bool CharacterAimbotV2Feature::OnStart()
{
    // TODO: To replace
    PlayerProvider = (PlayerProviderFeature*)MainDomain::Get().GetFeatureManager()->GetFeature(xorstr_("F_PROVIDER_PLAYER"));
    if (!PlayerProvider)
    {
        PLOG(error, "[CharacterAimbotV2Feature] Failed to get an instance of PlayerProviderFeature");
        return false;
    }

    // TODO: Use event for airfreeze state rather than getting the feature instance.
#if FEATURE_CHARACTER_AIMBOT_ENABLE_AIR_FREEZE_SUPPORT == 1
    AirFreezeFeatureInstance = (AirFreezeFeature*)MainDomain::Get().GetFeatureManager()->GetFeature(xorstr_("F_AIR_FREEZE"));
    if (!AirFreezeFeatureInstance)
    {
        PLOG(error, "[CharacterAimbotV2Feature] Failed to get an instance of AirFreezeFeature");
        return false;
    }
#endif

#if FEATURE_CHARACTER_AIMBOT_ENABLE_WALLBANG == 1
    if (!Dispatcher->Subscribe<SendSerializedRpcEvent>(SendSerialisedRpcHandle, [this](SendSerializedRpcEvent& Event) { OnSendSerialisedRpc(Event); }))
    {
        PLOG(warn, "[CharacterAimbotV2Feature] Failed to register SendSerialisedRpcHandle");
    }
#endif

    if (!Dispatcher->Subscribe<ReceivedSecurityAction>(ReceivedSecurityActionHandle, [this](ReceivedSecurityAction& Event) { OnReceiveSecurityAction(Event); }))
    {
        PLOG(warn, "[CharacterAimbotV2Feature] Failed to register ReceivedSecurityActionHandle");
    }

    if (WeaponCategoryDelegates.size() == 0)
    {
        
        auto ProjectileWeaponIsGoodTargetCheck = [this](AWieldableItem* InWeapon, AActor* InTarget) { return IsGoodTarget_ProjectileWeapon(InWeapon, InTarget); };
        auto ProjectileWeaponGetAimAngleCheck = [this](AWieldableItem* InWeapon, AActor* InTarget, FVector InFireLocation, FVector InTargetLocation, FRotator* OutAimAngle) { return GetAimAngle_ProjectileWeapon(InWeapon, InTarget, InFireLocation, InTargetLocation, OutAimAngle); };

        auto ThrowKnifeWeaponIsGoodTargetCheck = [this](AWieldableItem* InWeapon, AActor* InTarget) { return IsGoodTarget_ThrowableKnife(InWeapon, InTarget); };
        auto ThrowKnifeGetAimAngleCheck = [this](AWieldableItem* InWeapon, AActor* InTarget, FVector InFireLocation, FVector InTargetLocation, FRotator* OutAimAngle) { return GetAimAngle_ThrowableKnife(InWeapon, InTarget, InFireLocation, InTargetLocation, OutAimAngle); };

        auto BlowpipeIsGoodTargetCheck = [this](AWieldableItem* InWeapon, AActor* InTarget) { return IsGoodTarget_Blowpipe(InWeapon, InTarget); };
        auto BlowpipeGetAimAngleCheck = [this](AWieldableItem* InWeapon, AActor* InTarget, FVector InFireLocation, FVector InTargetLocation, FRotator* OutAimAngle) { return GetAimAngle_Blowpipe(InWeapon, InTarget, InFireLocation, InTargetLocation, OutAimAngle); };

        WeaponCategoryDelegates = {
            {
                UBlunderbussItemCategory::StaticClass(),
                WeaponAimbotDelegates(ProjectileWeaponIsGoodTargetCheck, ProjectileWeaponGetAimAngleCheck)
            },
            {
                UEyeOfReachItemCategory::StaticClass(),
                WeaponAimbotDelegates(ProjectileWeaponIsGoodTargetCheck, ProjectileWeaponGetAimAngleCheck)
            },
            {
                UFlintlockItemCategory::StaticClass(),
                WeaponAimbotDelegates(ProjectileWeaponIsGoodTargetCheck, ProjectileWeaponGetAimAngleCheck)
            },
            {
                UFlintlockDoubleBarrelItemCategory::StaticClass(),
                WeaponAimbotDelegates(ProjectileWeaponIsGoodTargetCheck, ProjectileWeaponGetAimAngleCheck)
            },
            {
                UThrowingKnifeItemCategory::StaticClass(),
                WeaponAimbotDelegates(ThrowKnifeWeaponIsGoodTargetCheck, ThrowKnifeGetAimAngleCheck)
            },
            {
                UBlowpipeItemCategory::StaticClass(),
                WeaponAimbotDelegates(BlowpipeIsGoodTargetCheck, BlowpipeGetAimAngleCheck)
            }
        };
    }

    if (WallbangableWeapons.size() == 0)
    {
        WallbangableWeapons = {
            UBlunderbussItemCategory::StaticClass(),
            UEyeOfReachItemCategory::StaticClass(),
            UFlintlockItemCategory::StaticClass(),
            UFlintlockDoubleBarrelItemCategory::StaticClass()
        };
    }

	return true;
}

//---------------------------------------------------------------------------

bool CharacterAimbotV2Feature::OnEnd()
{
    Dispatcher->Unsubscribe(SendSerialisedRpcHandle);
    Dispatcher->Unsubscribe(ReceivedSecurityActionHandle);
	return true;
}

//---------------------------------------------------------------------------

void CharacterAimbotV2Feature::OnReset(EFeatureResetReason InResetReason)
{
    ResetWallbangState();
}

//---------------------------------------------------------------------------

void CharacterAimbotV2Feature::PostTick(float DeltaTime) 
{
    if (WallbangTarget)
    {
        ResetWallbangState();
    }
}

//---------------------------------------------------------------------------

void CharacterAimbotV2Feature::Tick(float DeltaTime) {}

//---------------------------------------------------------------------------

bool CharacterAimbotV2Feature::CanExecute()
{
	return GetCfg().Aim.Weapon.Enabled->Get() 
        && GetPlayer()
        && GetCamera() 
        && GetController() 
        && !GetPlayer()->GetAttachParentActor() // not attached to object
        && !GetPlayer()->IsDead() // not dead
        && InputService::Get().IsKeyPressed(GetCfg().Aim.Keybinds.WieldableLockOnToggle->Get());
}

//---------------------------------------------------------------------------

void CharacterAimbotV2Feature::OnConditionChange(bool bConditionChange) {}

//---------------------------------------------------------------------------

void CharacterAimbotV2Feature::OnExecute(float DeltaTime) 
{
#if FEATURE_CHARACTER_AIMBOT_ENABLE_DEBUG == 1
    if (!LastDirectionPoints.empty())
    {
        auto& Dm = DrawService::Get();
        Dm.PushLayer(DRAWLIST_VISUALS_FORE);

        int32_t Idx = -1;
        for (auto& Dir : LastDirectionPoints)
        {
            Idx++;

            ImColor DrawColor = Dir.second ? ImColor(0, 255, 0, 255) : ImColor(255, 0, 0, 255);

            FVector2D ScreenPos;
            if (!GetController()->ProjectWorldLocationToScreen(Dir.first, ScreenPos))
                continue;

            Dm.AddBoxedText(std::to_string(Idx).data(), ScreenPos, DrawColor, Dm.ScaleFont(14.f), EAlign::Center, EFontFace::PoppinsBold, {2,2}, 0.8f, 4.0f);
        }

        Dm.PopLayer();

    }
#endif

    const auto CurrentWeapon = GetWieldedWeapon();

    if (!CurrentWeapon || !CurrentWeapon->ItemCategory)
        return;

    auto Found = WeaponCategoryDelegates.find(CurrentWeapon->ItemCategory);
    if (Found == WeaponCategoryDelegates.end())
    {
        return;
    }

    SuitableTargetResult VisibleResult;
    SuitableTargetResult InvisibleResult;

    if (!GetSuitableTarget(&VisibleResult, &InvisibleResult))
    {
        return;
    }

#if FEATURE_CHARACTER_AIMBOT_ENABLE_WALLBANG == 1
    // See if we can get a potential target for wallbang
    bool ShouldTryWallbang = false;

    if (GetCfg().Other.Exploits.Wieldables.EnableWallbang->Get())
    {
        float MaxFOV = GetCamera()->CameraCache.POV.FOV; //  / 2.35f

        // Convert to horizontal FOV
        float VerticalFOVRadians = UKismetMathLibrary::DegreesToRadians(MaxFOV);
        float HalfVerticalFOV = VerticalFOVRadians / 2.0f;
        float HorizontalFOVRadians = 2 * atan(tan(HalfVerticalFOV) * GetCamera()->CameraCache.POV.AspectRatio);
        MaxFOV = UKismetMathLibrary::RadiansToDegrees(HorizontalFOVRadians);

        if (VisibleResult.Exists() && InvisibleResult.Exists())
        {
            // In the case we have a good visible and invisible target, we'd need to select
            // a suitable target that won't cause aim issues with unintended targets.
            // We'll wallbang targets where the invisible target is closer than the visible one.

            // We also want to only target those that are actually within our games FOV and not those outside.
            // This is so we don't accidently shoot someone behind a wall while trying to aim at another target.
            // Should never happen regardless due to visible priority but we'll cover this edge case.
            if (InvisibleResult.AngleDegrees < 45.f && (VisibleResult.AngleDegrees > InvisibleResult.AngleDegrees))
            {
                ShouldTryWallbang = true;
            }

        }
        else if (!VisibleResult.Exists() && InvisibleResult.Exists() && InvisibleResult.AngleDegrees < MaxFOV)
        {
            // We only have an invisible target, so we'll just try wallbang this target anyway.
            ShouldTryWallbang = true;
        }
    }
    
    if (ShouldTryWallbang && TryWallbang(InvisibleResult))
    {
        // We found a wallbang target, break out so we don't proceed to lock on
        return;
    }

#endif

    if (VisibleResult.Exists())
    {
        // We have a visible target so we'll lock on
        TryAimbot(VisibleResult);
    }
    
}

//---------------------------------------------------------------------------

void CharacterAimbotV2Feature::OnDiscard(float DeltaTime) {}

//---------------------------------------------------------------------------

bool CharacterAimbotV2Feature::TryWallbang(CharacterAimbotV2Feature::SuitableTargetResult& InSuitableTarget)
{
    auto CurrentWeapon = GetWieldedWeapon();

    // Should never happen but we'll check just in case.
    // We only try wallbang if there is a valid invisible target
    if (!InSuitableTarget.Exists())
        return false;

    auto Found = WallbangableWeapons.find(CurrentWeapon->ItemCategory);

    // Need a valid weapon and only if we can wallbang with said weapon
    if (!CurrentWeapon || Found == WallbangableWeapons.end())
        return false;

    FVector TheirLocation = InSuitableTarget.Target->K2_GetActorLocation();
    FVector CameraLocation = GetLocalViewLocation();

    auto FireLocation = TheirLocation;

    bool GoodForTarget = false;
    EWallbangFireType FireType = EWallbangFireType::Origin;

    // Add support for targting people > 500cm by setting the fire location as CameraLoc + Extension.
    // Targets greater than 500cm will be clamped so we'll use that as our basis for checking
    // if the shot needs to be extended or not. Being 500cm, we're lowering the distance so we can ensure
    // there is no desync from server's clamped fire position.
    //
    // Edit: I lowered the distance to 2.9m (290cm) pending player feedback. Change me if users complain.
    if (TheirLocation.Distance(GetLocalViewLocation()) > 290.f)
    {
        // Find a good line of sight point from our player.
        if (FindBestLineOfSightFireLocation(InSuitableTarget.Target, CameraLocation, GetCamera()->CameraCache.POV.Rotation, &FireLocation, 265.f))
        {
            GoodForTarget = true;
            FireType = EWallbangFireType::Extended;
        }
    }
    else
    {
        // Below 650m, we'll just mark it good for target
        GoodForTarget = true;
    }

    // If this target isn't any good (such as not being possible to hit) then we'll disregard it
    if (!GoodForTarget)
        return false;

    FVector FireDirection = FVector();

    FRotator FinalAim;

    // If it's an extended wallbang, we'll predict the aim angle
    // then account for the change in direction so we can pass it to the RPC
    if (FireType == EWallbangFireType::Extended)
    {
        // Won't be able to predict their position, pull out of this wallbang attempt
        if (!PredictAimAngle(InSuitableTarget.Target, FireLocation, TheirLocation, &FinalAim))
        {
            return false;
        }

        FireDirection = UKismetMathLibrary::GreaterGreater_VectorRotator(FVector::ForwardVector, FinalAim);
    }

    // Account for the relative aim requirement when we're on a movement base.
    // We do this by setting our fire direction and location relative to the base origin.
    if (auto MoveBase = GetLocalPlayer()->BasedMovement.MovementBase)
    {
        if (GetLocalPlayer()->BasedMovement.MovementBase->Mobility == 2)
        {
            FTransform ComponentWorld = GetLocalPlayer()->BasedMovement.MovementBase->K2_GetComponentToWorld();
            FireLocation = UKismetMathLibrary::InverseTransformLocation(ComponentWorld, FireLocation);

            if (FireType == EWallbangFireType::Extended)
            {
                FireDirection = MoveBase->K2_GetComponentToWorld().Rotation.UnrotateVector(FireDirection);
            }
        }
    }

    // Assign wallbang state for this frame
    WallbangTarget = InSuitableTarget.Target;
    WallbangFireType = FireType;
    WallbangFireDirection = FireDirection;
    WallbangFireLocation = FireLocation;

    // Indicator to notify user they're conducting a wallbang on a valid target
    // This just shows an icon below the crosshair
    FVector2D ScreenCenter = ScreenSize / 2;

    // Account for the crosshair size for the wallbang indicator
    const float CrosshairSize = GetCfg().Visuals.HUD.CrosshairSize->Get() + 25.f;

    auto& Dm = DrawService::Get();
    Dm.PushLayer(DRAWLIST_OVERLAY); // Originally UI
    Dm.AddBoxedText("\xEE\x96\x8A", { ScreenCenter.X, ScreenCenter.Y + CrosshairSize }, GetCfg().Visuals.HUD.Colors.Theme->Get(), Dm.ScaleFont(14.f), EAlign::Center, EFontFace::PoppinsBold, { 2,2 }, 0.5f, 4.0f);
    Dm.PopLayer();

    // Fire the event for wallbangable targets.
    // Only fire event if we have triggerbot on.
    if (GetCfg().Aim.Weapon.Triggerbot.TargetWallbangs->Get())
    {
        Dispatcher->Emit(
            TargetLockedOnEvent(
                InSuitableTarget.Target,
                FireLocation,
                FinalAim
            )
        );
    }

    return true;
}

//---------------------------------------------------------------------------

bool CharacterAimbotV2Feature::TryAimbot(CharacterAimbotV2Feature::SuitableTargetResult& InSuitableTarget)
{
    if (!InSuitableTarget.Exists())
        return false;

    auto FireLocation = GetLocalViewLocation();
    FVector AimLocation = InSuitableTarget.IsUsingLocationOverride ? InSuitableTarget.OverrideLocation : InSuitableTarget.Target->K2_GetActorLocation();
    
    // OutRotation
    FRotator FinalAim;

    if (PredictAimAngle(InSuitableTarget.Target, FireLocation, AimLocation, &FinalAim))
    {
        if (GetCfg().Aim.Weapon.Mode->Get() == EWieldableAimMode::Snap)
        {
            GetController()->ClientSetRotation(FinalAim, false);
        }
        else
        {
            // HACK: Set the cached rotation directly. This will affect our screenspace projections and cause weird ESP bugs sometimes.
            // Unfortunately due to the hook we chose to tick the domain, this will cause weird bugs with the ESP.
            GetCamera()->CameraCache.POV.Rotation = FinalAim;
        }

        // Fire event that we've locked on to a target
        Dispatcher->Emit(
            TargetLockedOnEvent(
                InSuitableTarget.Target,
                AimLocation,
                FinalAim
            )
        );

        return true;
    }

    return false;
}

//---------------------------------------------------------------------------

bool CharacterAimbotV2Feature::GetSuitableTarget(CharacterAimbotV2Feature::SuitableTargetResult* OutVisibleTarget, CharacterAimbotV2Feature::SuitableTargetResult* OutInvisibleTarget)
{
    auto CurrentWeapon = GetWieldedWeapon();
    if (!CurrentWeapon)
        return false;

    FVector CameraPos = GetLocalViewLocation();
    FVector2D ScreenCenter = ScreenSize / 2;

    auto CameraForward = UKismetMathLibrary::GetForwardVector(GetCamera()->CameraCache.POV.Rotation);
    auto MaxFOVValue = GetCamera()->CameraCache.POV.FOV; //  / 2.35f

    if (GetCfg().Aim.Weapon.UseCustomAimRadius->Get())
    {
        MaxFOVValue = GetCfg().Aim.Weapon.AimRadius->Get() / 2.00f;
    }
    else
    {
        // Convert Vertical FOV to radians
        float VerticalFOVRadians = UKismetMathLibrary::DegreesToRadians(MaxFOVValue);

        // Calculate the half vertical FOV
        float HalfVerticalFOV = VerticalFOVRadians / 2.0f;

        // Calculate the horizontal FOV in radians
        float HorizontalFOVRadians = 2 * atan(tan(HalfVerticalFOV) * GetCamera()->CameraCache.POV.AspectRatio);

        // Convert back to degrees
        MaxFOVValue = UKismetMathLibrary::RadiansToDegrees(HorizontalFOVRadians);
    }

    bool bHasWeaponValidationFunc = false;
    auto Found = WeaponCategoryDelegates.find(CurrentWeapon->ItemCategory);

    TGoodTargetFunction ValidationFunc;

    if (Found != WeaponCategoryDelegates.end())
    {
        ValidationFunc = Found->second.ValidationFunction;
        bHasWeaponValidationFunc = true;
    }

    float BestInvisibleDistance = MaxFOVValue;
    float BestVisibleDistance = MaxFOVValue;

    CharacterAimbotV2Feature::SuitableTargetResult Result;
    
    if (GetCfg().Aim.Weapon.TargetAI->Get())
    {
        for (auto Character : ActorService::Get().GetActorsOfType<AAthenaCharacter>())
        {
            if (!Character->IsA(AAthenaAICharacter::StaticClass()))
                continue;

            if (Character->bHidden || Character->bActorIsBeingDestroyed)
                continue;

            if (bHasWeaponValidationFunc && ValidationFunc && !ValidationFunc(CurrentWeapon, Character))
                continue;

            FVector TheirPos = Character->K2_GetActorLocation();

            bool bIsVisible = GetController()->LineOfSightTo(Character, FVector(0, 0, 0), false);
            FVector DirectionToPlayer = (TheirPos - CameraPos).GetSafeNormal();
            float DotProduct = UKismetMathLibrary::Dot_VectorVector(CameraForward, DirectionToPlayer);
            float AngleDegrees = UKismetMathLibrary::RadiansToDegrees(acosf(DotProduct));

            if (bIsVisible && OutVisibleTarget)
            {
                if (AngleDegrees < BestVisibleDistance)
                {
                    BestVisibleDistance = AngleDegrees;
                    OutVisibleTarget->Target = Character;
                    OutVisibleTarget->AngleDegrees = AngleDegrees;
                }
            }
            else if (!bIsVisible && OutInvisibleTarget)
            {
                if (AngleDegrees < BestInvisibleDistance)
                {
                    BestInvisibleDistance = AngleDegrees;
                    OutInvisibleTarget->Target = Character;
                    OutInvisibleTarget->AngleDegrees = AngleDegrees;
                }
            }
        }
    }

    if (GetCfg().Aim.Weapon.TargetPlayers->Get())
    {
        for (auto& Entry : PlayerProvider->GetPlayers())
        {
            auto Player = Entry.first;
            auto& Ctx = Entry.second;
            
#if FEATURE_CHARACTER_AIMBOT_ENABLE_DEBUG == 0
            if (Player == GetLocalPlayer())
                continue;

            if (Ctx.Team == PlayerProviderFeature::ETeam::Crew)
                continue;


            if (Ctx.Team == PlayerProviderFeature::ETeam::Alliance && !GetCfg().Aim.Weapon.TargetAlliance->Get())
                continue;
#endif

            if (bHasWeaponValidationFunc && ValidationFunc && !ValidationFunc(CurrentWeapon, Player))
                continue;

            FVector TheirPosition = Player->K2_GetActorLocation();

            FVector DirectionToPlayer = (TheirPosition - CameraPos).GetSafeNormal();

            float DotProduct = UKismetMathLibrary::Dot_VectorVector(CameraForward, DirectionToPlayer);
            float AngleDegrees = UKismetMathLibrary::RadiansToDegrees(acosf(DotProduct));

            if (Ctx.IsVisible && OutVisibleTarget)
            {
                if (AngleDegrees < BestVisibleDistance)
                {
                    BestVisibleDistance = AngleDegrees;
                    OutVisibleTarget->Target = Player;
                    OutVisibleTarget->AngleDegrees = AngleDegrees;

                    for (int i = 0; i < 7; i++)
                    {
                        if (Ctx.IsBoneVisible(i))
                        {
                            FVector OutPos;
                            FRotator OutRot;
                            FName BoneName = Player->Mesh->GetBoneName((int32_t)PlayerProvider->VisibleBoneTargets[i]);
                            Player->Mesh->TransformFromBoneSpace(BoneName, { 0,0,0 }, { 0,0,0 }, OutPos, OutRot);

                            OutVisibleTarget->OverrideLocation = OutPos;
                            OutVisibleTarget->IsUsingLocationOverride = true;
                            break;
                        }
                    }

                }
            }
            else if (!Ctx.IsVisible && OutInvisibleTarget)
            {
                if (AngleDegrees < BestInvisibleDistance)
                {
                    BestInvisibleDistance = AngleDegrees;
                    OutInvisibleTarget->Target = Player;
                    OutInvisibleTarget->AngleDegrees = AngleDegrees;
                }
            }
        }
    }
    

#if FEATURE_CHARACTER_AIMBOT_ENABLE_DEBUG == 1
    if (OutVisibleTarget && OutVisibleTarget->Target)
    {
        UKismetSystemLibrary::DrawDebugLine(GetWorld(), OutVisibleTarget->Target->K2_GetActorLocation(), OutVisibleTarget->Target->K2_GetActorLocation() + FVector(0, 0, 10000), FLinearColor(0, 1.0, 0, 1.0), 0.f, 10);
    }

    if (OutInvisibleTarget && OutInvisibleTarget->Target)
    {
        UKismetSystemLibrary::DrawDebugLine(GetWorld(), OutInvisibleTarget->Target->K2_GetActorLocation(), OutInvisibleTarget->Target->K2_GetActorLocation() + FVector(0, 0, 10000), FLinearColor(1.f, 0, 0, 1.0), 0.f, 10);
    }
#endif

    return (OutVisibleTarget && OutVisibleTarget->Exists()) || (OutInvisibleTarget && OutInvisibleTarget->Exists());
}

//---------------------------------------------------------------------------

bool CharacterAimbotV2Feature::PredictAimAngle(AActor* InTarget, FVector InFireLocation, FVector InTargetLocation, FRotator* OutAimRotation)
{
    auto CurrentWeapon = GetWieldedWeapon();

    if (!CurrentWeapon)
    {
        return false;
    }

    auto Found = WeaponCategoryDelegates.find(CurrentWeapon->ItemCategory);

    if (Found != WeaponCategoryDelegates.end())
    {
        return Found->second.GetAimAngleFunction(CurrentWeapon, InTarget, InFireLocation, InTargetLocation, OutAimRotation);
    }

    return false;
}

//---------------------------------------------------------------------------

bool CharacterAimbotV2Feature::GetAimAngle_ThrowableKnife(AWieldableItem* InWeapon, AActor* InTarget, FVector InFireLocation, FVector InTargetLocation, FRotator* OutAimAngle)
{
    if (!InWeapon) return false;

    /*
        Offset location TODO/resarch:
        "X": 15.0,
        "Y": 8.0,
        "Z": 75.0
    */

    AThrowableMeleeWeapon* Weapon = (AThrowableMeleeWeapon*)InWeapon;

    FVector TargetVelocity = InTarget->GetVelocity();

    bool bUseAngular = false;

    FVector AngularVelocity;

    // If the case it's a character, account for certain scenarios such as being on a ship, etc
    if (InTarget && InTarget->IsA(AAthenaCharacter::StaticClass()))
    {
        AAthenaCharacter* Character = (AAthenaCharacter*)InTarget;
        if (auto Ship = (AShip*)Character->GetCurrentShip(EShipTrackedType::OnShipDeckOrLadder))
        {
            // Get the movement proxy or default to the sampled velocity
            if (auto MovementProxy = Ship->GetMovementProxy())
            {
                TargetVelocity += MovementProxy->Movement.LinearVelocity;
            }
            else
            {
                TargetVelocity += Ship->GetSampledVelocity();
            }

            UPrimitiveComponent* Casted = (UPrimitiveComponent*)Ship->RootComponent;
            AngularVelocity = Casted->GetPhysicsAngularVelocity(FName());

            if (AngularVelocity.Size() > 0)
            {
                //bUseAngular = true;
            }
        }
    }

    FVector LocalVelocity = GetLocalPlayer()->GetVelocity();
    if (auto Ship = (AShip*)GetLocalPlayer()->GetCurrentShip(EShipTrackedType::OnShipDeckOrLadder))
    {
        // Get the movement proxy or default to the sampled velocity
        if (auto MovementProxy = Ship->GetMovementProxy())
        {
            LocalVelocity += MovementProxy->Movement.LinearVelocity;
        }
        else
        {
            LocalVelocity += Ship->GetSampledVelocity();
        }
    }

#if FEATURE_CHARACTER_AIMBOT_ENABLE_AIR_FREEZE_SUPPORT == 1
    if (AirFreezeFeatureInstance
        && AirFreezeFeatureInstance->IsEnabled()
        && AirFreezeFeatureInstance->IsActive())
    {
        LocalVelocity = { 0,0,0 };
    }
#endif

    //const auto GrenadeDataAsset = Weapon->GrenadeSetupDataAsset;
    float ProjectileSpeed = 5700.f;
    const float ProjectileGravityScale = 0.5f; // 0.5
    const FVector ThrowOffset = FVector(15.f, 8.f, 75.f);
    const FVector ThrowLocation = GetPlayer()->K2_GetActorLocation() + UKismetMathLibrary::GreaterGreater_VectorRotator(ThrowOffset, FRotator(0.f, GetPlayer()->GetCharacterRotation().Yaw, 0.f));

    //float LaunchSpeed = 5700.f;

    /*if (GrenadeDataAsset)
    {
        if (auto Curve = Weapon->GrenadeSetupDataAsset->PitchToProjectileSpeedCurve)
        {
            const auto ViewPitch = UAngleMaths::AngleMod180(GetPlayer()->GetViewPitch());
            ProjectileSpeed = Curve->GetFloatValue(ViewPitch);
        }
    }*/
    
    FRotator OutAngle;

    bool bIsHit = PredictionFunctionsPrivate::PredictMovingTargetWithGravity
    (
        InTargetLocation,
        TargetVelocity,
        AngularVelocity,
        InTarget ? InTarget->K2_GetActorRotation() : FRotator(),
        ProjectileSpeed,
        ProjectileGravityScale,
        ThrowLocation,
        LocalVelocity,
        false,
        bUseAngular,
        OutAngle
    );


    *OutAimAngle = OutAngle;

    return bIsHit;
}

//---------------------------------------------------------------------------

bool CharacterAimbotV2Feature::GetAimAngle_ProjectileWeapon(AWieldableItem* InWeapon, AActor* InTarget, FVector InFireLocation, FVector InTargetLocation, FRotator* OutAimAngle)
{
    PTRACE_DEBUG();

    if (!InWeapon) return false;

    AProjectileWeapon* Weapon = (AProjectileWeapon*)InWeapon;

    FVector TargetVelocity = FVector(0.f, 0.f, 0.f);

    // If the case it's a character, account for certain scenarios such as being on a ship, etc
    if (InTarget)
    {
        TargetVelocity = InTarget->GetVelocity();

        if (InTarget->IsA(AAthenaCharacter::StaticClass()))
        {
            AAthenaCharacter* Character = (AAthenaCharacter*)InTarget;
            if (auto Ship = Character->GetCurrentShip(EShipTrackedType::OnShipDeckOrLadder))
            {
                // Get the movement proxy or default to the sampled velocity
                if (auto MovementProxy = Ship->GetMovementProxy())
                {
                    TargetVelocity += MovementProxy->Movement.LinearVelocity;
                }
                else
                {
                    TargetVelocity += Ship->GetSampledVelocity();
                }
            }
        }
    }

    FVector LocalVelocity = FVector(0.f, 0.f, 0.f); // Zero out our velocity since it's not accounted for in the games shooting calculation

    if (auto Ship = GetLocalPlayer()->GetCurrentShip(EShipTrackedType::OnShipDeckOrLadder))
    {
        // Get the movement proxy or default to the sampled velocity
        if (auto MovementProxy = Ship->GetMovementProxy())
        {
            LocalVelocity += MovementProxy->Movement.LinearVelocity;
        }
        else
        {
            LocalVelocity += Ship->GetSampledVelocity();
        }
    }

#if FEATURE_CHARACTER_AIMBOT_ENABLE_AIR_FREEZE_SUPPORT == 1
    if (AirFreezeFeatureInstance
        && AirFreezeFeatureInstance->IsEnabled()
        && AirFreezeFeatureInstance->IsActive())
    {
        LocalVelocity = { 0,0,0 };
    }
#endif

    const float ProjectileSpeed = Weapon->WeaponParameters.AmmoParams.Velocity;

    FRotator OutAngle;

    bool bIsHit = PredictionFunctions::PredictMovingTarget
    (
        InTargetLocation,
        TargetVelocity,
        InTarget ? InTarget->K2_GetActorRotation() : FRotator(),
        ProjectileSpeed,
        InFireLocation,
        LocalVelocity,
        false,
        OutAngle
    );

    *OutAimAngle = OutAngle;

    return bIsHit;
}

//---------------------------------------------------------------------------

bool CharacterAimbotV2Feature::GetAimAngle_Blowpipe(AWieldableItem* InWeapon, AActor* InTarget, FVector InFireLocation, FVector InTargetLocation, FRotator* OutAimAngle)
{
    if (!InWeapon) return false;

    ABlowpipeWeapon* Weapon = (ABlowpipeWeapon*)InWeapon;

    FVector TargetVelocity = InTarget->GetVelocity();

    // If the case it's a character, account for certain scenarios such as being on a ship, etc
    if (InTarget && InTarget->IsA(AAthenaCharacter::StaticClass()))
    {
        AAthenaCharacter* Character = (AAthenaCharacter*)InTarget;
        if (auto Ship = (AShip*)Character->GetCurrentShip(EShipTrackedType::OnShipDeckOrLadder))
        {
            // Get the movement proxy or default to the sampled velocity
            if (auto MovementProxy = Ship->GetMovementProxy())
            {
                TargetVelocity += MovementProxy->Movement.LinearVelocity;
            }
            else
            {
                TargetVelocity += Ship->GetSampledVelocity();
            }
        }
    }

    FVector LocalVelocity = GetLocalPlayer()->GetVelocity();
    if (auto Ship = (AShip*)GetLocalPlayer()->GetCurrentShip(EShipTrackedType::OnShipDeckOrLadder))
    {
        // Get the movement proxy or default to the sampled velocity
        if (auto MovementProxy = Ship->GetMovementProxy())
        {
            LocalVelocity += MovementProxy->Movement.LinearVelocity;
        }
        else
        {
            LocalVelocity += Ship->GetSampledVelocity();
        }
    }

#if FEATURE_CHARACTER_AIMBOT_ENABLE_AIR_FREEZE_SUPPORT == 1
    if (AirFreezeFeatureInstance
        && AirFreezeFeatureInstance->IsEnabled()
        && AirFreezeFeatureInstance->IsActive())
    {
        LocalVelocity = { 0,0,0 };
    }
#endif

    float ProjectileSpeed = Weapon->ADSVelocity;


    FRotator OutAngle;

    bool bIsHit = PredictionFunctions::PredictMovingTarget
    (
        InTargetLocation,
        TargetVelocity,
        InTarget ? InTarget->K2_GetActorRotation() : FRotator(),
        ProjectileSpeed,
        InFireLocation,
        LocalVelocity,
        false,
        OutAngle
    );


    *OutAimAngle = OutAngle;

    return bIsHit;
}

//---------------------------------------------------------------------------

bool CharacterAimbotV2Feature::IsGoodTarget_ThrowableKnife(AWieldableItem* InWeapon, AActor* InTarget)
{
    return true;
}

//---------------------------------------------------------------------------

bool CharacterAimbotV2Feature::IsGoodTarget_ProjectileWeapon(AWieldableItem* InWeapon, AActor* InTarget)
{
    AProjectileWeapon* ProjectileWeapon = (AProjectileWeapon*)InWeapon;

    const auto Distance = InTarget->K2_GetActorLocation().Distance(GetLocalViewLocation());

    // Check if the distance of the target exceeds the gravity falloff
    //if (Distance > (ProjectileWeapon->DistanceBeforeGravity + 650.f))
        //return false;

    // bruh
    if (Distance > 25000)
        return false;

    return true;
}

//---------------------------------------------------------------------------

bool CharacterAimbotV2Feature::IsGoodTarget_Blowpipe(AWieldableItem* InWeapon, AActor* InTarget)
{
    ABlowpipeWeapon* Weapon = (ABlowpipeWeapon * )InWeapon;

    // Append 1 meter to make up for the player height
    // silly ah solution but who tf uses throwing knives anyway
    const float MaxAimDistanceWithTolerence = Weapon->DistanceBeforeGravity + 100.f;

    return GetPlayer()->K2_GetActorLocation().Distance(InTarget->K2_GetActorLocation()) < MaxAimDistanceWithTolerence;
}

//---------------------------------------------------------------------------

AWieldableItem* CharacterAimbotV2Feature::GetWieldedWeapon()
{

    if (auto Player = GetLocalPlayer())
    {
        if (auto WieldableComponent = Player->WieldedItemComponent)
        {
            if (auto CurrentlyWieldedItem = WieldableComponent->CurrentlyWieldedItem)
            {
                if (auto Category = CurrentlyWieldedItem->ItemCategory)
                {
                    if (UKismetMathLibrary::ClassIsChildOf(Category, UWeaponItemCategory::StaticClass()))
                    {
                        return CurrentlyWieldedItem;
                    }
                } 
            }
        }
    }

    return nullptr;
}

//---------------------------------------------------------------------------

AAthenaPlayerCharacter* CharacterAimbotV2Feature::GetLocalPlayer()
{
    return GetPlayer();
}

//---------------------------------------------------------------------------

void CharacterAimbotV2Feature::ResetWallbangState()
{
    WallbangTarget = nullptr;
    WallbangFireType = EWallbangFireType::Origin;
    WallbangFireDirection = FVector();
    WallbangFireLocation = FVector();
}

//---------------------------------------------------------------------------

void CharacterAimbotV2Feature::OnSendSerialisedRpc(SendSerializedRpcEvent& Ev)
{
    // We only want the fire/shoot RPC
    if (Ev.ContentsType != FFireWeaponOnServerRpc::StaticStruct())
        return;

    FFireWeaponOnServerRpc* Rpc = (FFireWeaponOnServerRpc*)Ev.Contents;

    if (!GetCfg().Other.Exploits.Wieldables.EnableWallbang->Get())
        return;

    // No target, disregard
    if (!WallbangTarget)
        return;

    // BAN PREVENTION SAFETY - Either crash the game (due to invalid function address) or the validation data is invalid and thus does not match.
    const auto ValidationData = FFireRequest::GetAimValidationData(Rpc->FireRequest);
    if (Rpc->ValidationData != ValidationData)
    {
        PLOGF(critical, "[CharacterAimbotFeature] Wallbang was unable to proceed due to mismatch in the validation data. ({0} != {1})", Rpc->ValidationData, ValidationData);
        return;
    }

    Rpc->FireRequest.RemoteAimData.AimData.AimPosition = WallbangFireLocation;
    if (WallbangFireType == EWallbangFireType::Extended)
    {
        Rpc->FireRequest.RemoteAimData.AimData.AimDirection = WallbangFireDirection;
    }

    // Update the validation data
    Rpc->ValidationData = FFireRequest::GetAimValidationData(Rpc->FireRequest);

    PLOGF(debug, "[CharacterAimbotFeature] Wallbanging target - IsExtended = {0}", (bool)(WallbangFireType == EWallbangFireType::Extended));
}

//---------------------------------------------------------------------------

bool CharacterAimbotV2Feature::FindBestLineOfSightFireLocation(AActor* InTarget, FVector InCameraLocation, FRotator InCameraRotation, FVector* OutFireLocation, float InMaximumDistance)
{
    // Need a target for this function since we validate target hit for raycast
    if (!InTarget)
        return false;

    FVector TargetLocation = InTarget->K2_GetActorLocation();

    // Get our direction vectors based on the given rotation
    FVector Forward = UKismetMathLibrary::GetForwardVector(InCameraRotation);
    FVector Right = UKismetMathLibrary::GetRightVector(InCameraRotation);
    FVector Up = UKismetMathLibrary::GetUpVector(InCameraRotation);

    // Directions to raycast from, extending from the camera position.
    // Array order based on the priority of checking. 
    // Priority shouldn't matter anyway as a hit is a hit
    // but we'll make the "more likely" direction go first.
    std::vector<FVector> Directions = 
    {
        Forward,                                      // In Front
        -Forward,                                     // Behind
        Up,                                           // Above
        -Up,                                          // Below
        Right,                                        // Right
        -Right,                                       // Left

        (Forward + Right).GetSafeNormal(),            // Front-Right diagonal
        (Forward - Right).GetSafeNormal(),            // Front-Left diagonal
        (-Forward + Right).GetSafeNormal(),           // Back-Right diagonal
        (-Forward - Right).GetSafeNormal(),           // Back-Left diagonal

        (Forward + Up).GetSafeNormal(),               // Front-Up diagonal
        (Forward - Up).GetSafeNormal(),               // Front-Down diagonal
        (-Forward + Up).GetSafeNormal(),              // Back-Up diagonal
        (-Forward - Up).GetSafeNormal(),              // Back-Down diagonal

        (Right + Up).GetSafeNormal(),                 // Right-Up diagonal
        (Right - Up).GetSafeNormal(),                 // Right-Down diagonal
        (-Right + Up).GetSafeNormal(),                // Left-Up diagonal
        (-Right - Up).GetSafeNormal()                 // Left-Down diagonal
    };

    bool bHasGoodDirection = false;
    FVector SuitableFireLocation;

    int32_t GoodDirIdx = -1;

    // Raycast each direction and see if there is a hit.
    // If we got a hit, we'll break out and return that good fire location.
    for (auto& Direction : Directions)
    {
        GoodDirIdx++;

        FVector FireLocation = InCameraLocation + (Direction * InMaximumDistance);

        FHitResult Hit;
        if (UKismetSystemLibrary::LineTraceSingle_DEPRECATED(
            GetPlayer(),
            FireLocation,
            TargetLocation,
            ECollisionChannel::ECC_GameTraceChannel1,
            false,
            TArray<AActor*>(),
            EDrawDebugTrace::None,
            Hit,
            true
        ))
        {
            // If we hit our target with the line trace, this is a good hit
            if (Hit.Actor.Get() && Hit.Actor.Get() == InTarget)
            {
                bHasGoodDirection = true;
                SuitableFireLocation = FireLocation;
            }
        }
    }

#if FEATURE_CHARACTER_AIMBOT_ENABLE_DEBUG == 1
    for (auto& Direction : Directions)
    {
        FVector FireLocation = InCameraLocation + (Direction * InMaximumDistance);

        if (bHasGoodDirection && FireLocation == SuitableFireLocation)
        {
            LastDirectionPoints.push_back({ FireLocation, true });
            //UKismetSystemLibrary::DrawDebugLine(GetWorld(), GetLocalViewLocation(), FireLocation, FLinearColor(0.f, 1.0f, 0.0f, 1.0f), 5.0f, 4.0f);
        }
        else
        {
            LastDirectionPoints.push_back({ FireLocation, false });
            //UKismetSystemLibrary::DrawDebugLine(GetWorld(), GetLocalViewLocation(), FireLocation, FLinearColor(1.f, 0.0f, 0.0f, 1.0f), 5.0f, 4.0f);
        }

    }
#endif

    // If we got a good fire location, we'll supply it to our out parameter
    if (bHasGoodDirection && OutFireLocation)
    {
        *OutFireLocation = SuitableFireLocation;
    }

    return bHasGoodDirection;
}

//---------------------------------------------------------------------------

FVector CharacterAimbotV2Feature::GetLocalViewLocation()
{
#if FEATURE_CHARACTER_AIMBOT_ENABLE_DEBUG == 0
    return GetLocalPlayer()->GetPawnViewLocation();
#else
    return GetCamera()->CameraCache.POV.Location;
#endif
}

//---------------------------------------------------------------------------
