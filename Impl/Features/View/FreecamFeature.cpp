#include "FreecamFeature.h"

//---------------------------------------------------------------------------

FreecamFeature::FreecamFeature(std::shared_ptr<scaffold::Messenger> InMessenger) : 
  scaffold::Feature(xorstr_("Freecam")), 
  Dispatcher(std::move(InMessenger)) 
{
};

//---------------------------------------------------------------------------

bool FreecamFeature::OnStart()
{
	return true;
}

//---------------------------------------------------------------------------

bool FreecamFeature::OnEnd()
{
	// Fire the disabled toggle event if this feature is disabled.
	// This is so we can support toggle if this feature is disabled via remote config.
	Dispatcher->Emit(FreecamToggleEvent(false, FVector()));

	return true;
}

//---------------------------------------------------------------------------

void FreecamFeature::OnReset(EFeatureResetReason InResetReason)
{
	OriginalCamera = nullptr;
	DummyCamera = nullptr;
	bActive = false;
	DisabledFreecamAt = 0;
	EnabledFreecamAt = 0;
	bUsingServerFreecam = false;
}

//---------------------------------------------------------------------------

void FreecamFeature::PostTick(float DeltaTime) {}

//---------------------------------------------------------------------------

void FreecamFeature::Tick(float DeltaTime)
{
	
}

//---------------------------------------------------------------------------

bool FreecamFeature::CanExecute()
{
	return IsInGameplay()
		&& GetPlayer()
		&& GetController()
		&& !GetPlayer()->GetAttachParentActor();
}

//---------------------------------------------------------------------------

void FreecamFeature::OnConditionChange(bool bConditionChange)
{
	// Disable the camera if the execution conditions change
	if (bActive && !bConditionChange && DisableCamera())
	{
		PLOG(warn, "[FreecamFeature] Freecam disabled automatically due to a change in feature execution conditions");
	}
}

//---------------------------------------------------------------------------

void FreecamFeature::OnDiscard(float DeltaTime)
{
	
}

//---------------------------------------------------------------------------

void FreecamFeature::OnExecute(float DeltaTime)
{
	if (InputService::Get().WasKeyPressed(GetCfg().Visuals.View.Keybinds.ToggleFreecam->Get()))
	{
		bool bStateChangeSuccess = ToggleCamera();

		// Check the change result
		if (!bStateChangeSuccess)
		{
			// Reset feature for safety if there is a failure + report the user for tracking

			NotificationEvent Event(
				5000,
				xorstr_("Freecam"),
				xorstr_("Failed to change freecam state. This should not happen! Please contact support and report this."),
				ICON_FA_USERS_VIEWFINDER,
				ImColor(255, 0, 0, 255),
				NotificationEvent::ESound::Warning
			);

			Dispatcher->Emit(Event);

			PLOG(error, "[FreecamFeature] An error occurred setting new freecam state");
			OnReset(EFeatureResetReason::Internal);
		}
	}

	if (bActive)
	{
		HandleFreecam();
	}
}

//---------------------------------------------------------------------------

bool FreecamFeature::ToggleCamera()
{
	bool bResult = false;

	if (bActive)
	{
		bResult = DisableCamera();
	}
	else
	{
		bResult = EnableCamera();
	}

	FeatureToggledEvent ToggleEvent(this->GetId(), this->GetFriendlyName(), bActive);
	Dispatcher->Emit(ToggleEvent);

	return bResult;
}

//---------------------------------------------------------------------------

bool FreecamFeature::EnableCamera()
{
	if (bActive)
		return true;

	DummyCamera = static_cast<APlayerCameraManager*>(UObjectUtilities::GetDefaultObject(APlayerCameraManager::StaticClass()));

	if (!DummyCamera)
	{
		PLOG(error, "[FreecamFeature] Failed to create APlayerCameraManager default object");
		return false;
	}

	FVector ViewLocation;
	FRotator ViewRotation;

	GetPlayer()->GetActorEyesViewPoint(ViewLocation, ViewRotation);

	// Set view target to new camera
	GetController()->ClientSetViewTarget(DummyCamera, FViewTargetTransitionParams{});

	// Teleport dummy camera to our view location
	DummyCamera->K2_TeleportTo(ViewLocation, ViewRotation);

	GetController()->SetIgnoreMoveInput(true);


	// Set the start locaitons
	CameraLocation = ViewLocation;
	StartLocation = ViewLocation;



#if FREECAM_FEATURE_USING_CINEMATIC_CAMERA == 1
	bUsingServerFreecam = GetCfg().Other.Exploits.EnableServerFreecam->Get();

	if (bUsingServerFreecam)
	{
		FToggleCinematicModeRpc Rpc;
		Rpc.bInCinematicMode = true;
		Rpc.PlayerController = GetController();


		GetSession().SendSerializedRpc(GetController()->BoxedRpcDispatcherComponent, &Rpc);
		GetPlayer()->StreamingLocationComponent->StreamingLocationActive = true;

		UpdateState = EFreecamUpdateState::Current;
	}
#endif

	PLOG(debug, "[FreecamFeature] Enabled freecam mode with relevancy mode ENABLED");

	bActive = true;

	// Fire start event
	Dispatcher->Emit(FreecamToggleEvent(true, StartLocation));

	return true;
}

//---------------------------------------------------------------------------

bool FreecamFeature::DisableCamera()
{
	if (!bActive)
		return true;

	if (DummyCamera == nullptr)
		return false;

#if FREECAM_FEATURE_USING_CINEMATIC_CAMERA == 1
	bUsingServerFreecam = false;

	FToggleCinematicModeRpc Rpc;
	Rpc.bInCinematicMode = false;
	Rpc.PlayerController = GetController();
	GetSession().SendSerializedRpc(GetController()->BoxedRpcDispatcherComponent, &Rpc);
	GetPlayer()->StreamingLocationComponent->StreamingLocationActive = false;
#endif

	GetController()->ClientSetViewTarget(GetPlayer(), FViewTargetTransitionParams{});

	DummyCamera = nullptr;

	GetController()->SetIgnoreMoveInput(false);


	PLOG(debug, "[FreecamFeature] Freecam has been disabled");

	bActive = false;

	// Fire start event
	Dispatcher->Emit(FreecamToggleEvent(false, StartLocation));

	return true;
}

//---------------------------------------------------------------------------

void FreecamFeature::HandleFreecam()
{
	auto& InputService = InputService::Get();

	FRotator EyeRotation; FVector EyeLocation;
	GetPlayer()->GetActorEyesViewPoint(EyeLocation, EyeRotation);


	FVector Forward = UKismetMathLibrary::GetForwardVector(EyeRotation);
	FVector Right = UKismetMathLibrary::GetRightVector(EyeRotation);

	float Speed = 10; // Initial speed multiplier

	if (InputService.IsKeyPressed(ImGuiKey_LeftShift)) // LSHIFT - SPEED
	{
		Speed *= (40 * GetCfg().Visuals.View.Freecam.SpeedMultiplier->Get());
	}
	if (InputService.IsKeyPressed(ImGuiKey_LeftCtrl)) // LCONTROL - DOWN
	{
		CameraLocation.Z -= Speed;
	}
	if (InputService.IsKeyPressed(ImGuiKey_Space)) // SPACEBAR - UP
	{
		CameraLocation.Z += Speed;
	}
	if (InputService.IsKeyPressed(GetCfg().Overrides.W->Get())) // W - FORWARD
	{
		CameraLocation += Forward * Speed;
	}
	if (InputService.IsKeyPressed(GetCfg().Overrides.A->Get())) // A - LEFT
	{
		CameraLocation -= Right * Speed;
	}
	if (InputService.IsKeyPressed(GetCfg().Overrides.D->Get())) // D - RIGHT
	{
		CameraLocation += Right * Speed;
	}
	if (InputService.IsKeyPressed(GetCfg().Overrides.S->Get())) // S - BACK
	{
		CameraLocation -= Forward * Speed;
	}


#if FREECAM_FEATURE_USING_CINEMATIC_CAMERA == 1
	if (bUsingServerFreecam)
	{
		if (Timestamp::GetTimeSince(LastRemoteUpdate) > GetCfg().Overrides.ServerFreecamUpdateRateMs->Get())
		{
#if FREECAM_FEATURE_CINEMATIC_CAMERA_UPDATE_LOCAL_POS == 1
			if (IsUpdateLocalPos())
			{
				if (UpdateState == EFreecamUpdateState::Start)
					UpdateState = EFreecamUpdateState::Current;
				else
					UpdateState = EFreecamUpdateState::Start;
			}
#endif
			LastRemoteUpdate = Timestamp::Now();


			FUpdateCameraPositionRpc Rpc;
			Rpc.AthenaPlayerController = GetController();
			Rpc.Location = UpdateState == EFreecamUpdateState::Start ? StartLocation : CameraLocation;
			Rpc.Rotation = EyeRotation;

			GetSession().SendSerializedRpc(GetController()->BoxedRpcDispatcherComponent, &Rpc);

			GetPlayer()->StreamingLocationComponent->StreamingLocationOverride.Location = CameraLocation;
			GetController()->bInCinematicMode = false;
		}
	}
#endif

	DummyCamera->K2_SetActorLocationAndRotation(CameraLocation, EyeRotation, false, nullptr, true);
}

//---------------------------------------------------------------------------

bool FreecamFeature::IsUpdateLocalPos()
{
	return GetCfg().Other.Exploits.Keybinds.FunnyMagicShip->Get() != ImGuiKey_None || GetCfg().Other.Exploits.Keybinds.FunnyMagicMark->Get() != ImGuiKey_None;
}