#include "DigSpotESPFeature.h"

//---------------------------------------------------------------------------

DigSpotESPFeature::DigSpotESPFeature(std::shared_ptr<scaffold::Messenger> InMessenger) : 
  scaffold::Feature(xorstr_("Dig Spot ESP")), 
  Dispatcher(std::move(InMessenger)) 
{
};

//---------------------------------------------------------------------------

bool DigSpotESPFeature::OnStart()
{
	
	if (!Dispatcher->Subscribe<ActorAggregationUpdateEvent>(ActorAggUpdateHandle, [this](const ActorAggregationUpdateEvent& Event) { OnActorAggregationUpdate(Event); }))
	{
		PLOG(error, "[DigSpotESPFeature] Failed to register ActorAggUpdateHandle");
		return false;
	}
	
	auto& Maps = ActorService::Get().GetActorsOfType<AWieldableItem>();
	for (AWieldableItem* Map : Maps)
	{
		if (!Map->IsA(AXMarksTheSpotMap::StaticClass()))
			continue;

		// Just pretend to be an event. Easiest option rather than writing another function.
		ActorAggregationUpdateEvent Event(
			Map,
			AXMarksTheSpotMap::StaticClass(),
			ActorAggregationUpdateEvent::EType::Added
		);

		OnActorAggregationUpdate(Event);
	}

	return true;
}

//---------------------------------------------------------------------------

bool DigSpotESPFeature::OnEnd()
{
	if (!Dispatcher->Unsubscribe(ActorAggUpdateHandle))
	{
		PLOG(error, "[DigSpotESPFeature] Failed to deregister ActorAggUpdateHandle");
		return false;
	}

	return true;
}

//---------------------------------------------------------------------------

void DigSpotESPFeature::OnReset(EFeatureResetReason InResetReason)
{
	RegistrationQueue.clear();
	DigSpots.clear();
	Maps.clear();
	LastUpdate = 0;
}

//---------------------------------------------------------------------------

void DigSpotESPFeature::PostTick(float DeltaTime) {}

//---------------------------------------------------------------------------

void DigSpotESPFeature::Tick(float DeltaTime)
{
}

//---------------------------------------------------------------------------

bool DigSpotESPFeature::CanExecute()
{
	AAthenaGameState* State = (AAthenaGameState*)GetWorld()->GameState;
	AIslandService* IslandService = State->IslandService;

	return GetCfg().Visuals.World.DrawDigSpots->Get()
		&& GetCamera()
		&& GetController()
		&& GetPlayer()
		&& State
		&& IslandService;
}

//---------------------------------------------------------------------------

void DigSpotESPFeature::OnDiscard(float DeltaTime)
{

}

//---------------------------------------------------------------------------

void DigSpotESPFeature::OnExecute(float DeltaTime)
{
	// Prevent running every frame. We delay to a 2 second interval as we don't need to
    // register maps/update every single frame. Performance impact is negligible.
	if (Timestamp::GetTimeSince(LastUpdate) > DIG_SPOT_FEATURE_UPDATE_INTERVAL)
	{
		// Process the map queue
		for (auto It = RegistrationQueue.begin(); It != RegistrationQueue.end();)
		{
			auto QueuedMap = *It;

			// Only register once we have a valid texture
			if (!QueuedMap->MapTexture.WeakPtr.IsValid())
			{
				++It;
			}
			else
			{
				const bool bRegisterMapResult = RegisterMap(QueuedMap);

				// Remove from the queue
				It = RegistrationQueue.erase(It);
			}
		}

		UpdateDigspots();

		LastUpdate = Timestamp::Now();
	}
	
	auto& Dm = DrawService::Get();
	Dm.PushLayer(DRAWLIST_VISUALS_FORE);

	const FVector2D ScreenCenter = FVector2D(ScreenSize.X, ScreenSize.Y);

	for (const auto& Digspot : DigSpots)
	{
		FVector2D ScreenPos;
		if (!GetController()->ProjectWorldLocationToScreen(Digspot, ScreenPos))
			continue;

		const int DistanceMeters = GetCamera()->K2_GetActorLocation().Distance(Digspot) * 0.01f;

		bool bIsHovered = ScreenCenter.Distance(ScreenPos) < 300.f;

		Dm.AddLabeledText(bIsHovered, ICON_FA_PERSON_DIGGING, std::vformat(xorstr_("{}m"), std::make_format_args(DistanceMeters)).c_str(), GetCfg().Visuals.World.Colors.DigSpots->Get(), GetCfg().Visuals.World.Colors.DigSpots->Get(), ScreenPos, EFontFace::Poppins, EFontFace::Poppins, Dm.ScaleFont(16.f), EAlign::Center, { 2, 2 }, 0.5f, 4.0f);
	}

	Dm.PopLayer();
}

//---------------------------------------------------------------------------

void DigSpotESPFeature::UpdateDigspots()
{
	DigSpots.clear();
	
	for (auto& MapEntry : Maps)
	{
		AXMarksTheSpotMap* Map = MapEntry.first;
		MapContext& Context = MapEntry.second;

		for (auto& Mark : Map->Marks)
		{
			FVector2D MarkPos = FVector2D(0.5f - Mark.Position.X, 0.5f - Mark.Position.Y);
			MarkPos = MarkPos.GetRotated(Map->Rotation + 180.f);

			FVector MarkWorldPosition = FVector
            (
				Context.IslandLocation.X + (MarkPos.X * Context.IslandCameraOrthoWidth),
				Context.IslandLocation.Y + (MarkPos.Y * Context.IslandCameraOrthoWidth),
				0
			);

			auto Start = FVector(MarkWorldPosition.X, MarkWorldPosition.Y, 35000.f);

			FHitResult Result;
			bool bHit = UKismetSystemLibrary::LineTraceSingle_NEW(GetWorld(), Start, MarkWorldPosition, ETraceTypeQuery::TraceTypeQuery3, false, TArray<AActor*>(), EDrawDebugTrace::None, Result, false);

			MarkWorldPosition.Z = Result.ImpactPoint.Z;

			DigSpots.push_back(MarkWorldPosition);
		}
	}
}

//---------------------------------------------------------------------------

bool DigSpotESPFeature::IsWieldingMap()
{
	if (!GetPlayer()->WieldedItemComponent)
		return false;

	if (!GetPlayer()->WieldedItemComponent->CurrentlyWieldedItem)
		return false;

	return GetPlayer()->WieldedItemComponent->CurrentlyWieldedItem->IsA(AXMarksTheSpotMap::StaticClass());
}

//---------------------------------------------------------------------------

bool DigSpotESPFeature::RegisterMap(AXMarksTheSpotMap* InMap)
{
	auto& MapTexture = InMap->MapTexture.WeakPtr;

	if (!InMap->Marks.IsValid())
	{
		PLOG(warn, "[DigSpotESPFeature] Tried registering a map with invalid mark array")
		return false;
	}

	if (!MapTexture.Get())
	{
		PLOG(warn, "[DigSpotESPFeature] Tried registering a map with invalid mark texture")
		return false;
	}

	MapContext Context;
	Context.TextureName = MapTexture.Get()->GetName();


	AAthenaGameState* State = (AAthenaGameState*)GetWorld()->GameState;
	AIslandService* IslandService = State->IslandService;

	UIslandDataAsset* IslandDataAsset = IslandService->IslandDataAsset;
	if (!IslandDataAsset)
	{
		PLOG(error, "[DigSpotESPFeature] Failed to register map due to invalid island data asset");
		return false;
	}

	if (IslandDataAsset->IslandDataEntries.Num() == 0)
	{
		PLOG(error, "[DigSpotESPFeature] Failed to register map due to invalid island data asset island entries");
		return false;
	}

	for (UIslandDataAssetEntry* Island : IslandDataAsset->IslandDataEntries)
	{
		FVector& IslandLocation = Island->WorldMapData->CaptureParams.WorldSpaceCameraPosition;

		// Filter out bad islands
		if (!Island || !Island->WorldMapData || Island->ShouldBeHiddenOnWorldMap || (IslandLocation.X == 0 && IslandLocation.Y == 0))
			continue;

		std::string IslandName = Island->IslandName.GetName();

		if (Context.TextureName.find(IslandName) == std::string::npos)
			continue;

		Context.IslandLocation = IslandLocation;
		Context.IslandCameraOrthoWidth = Island->WorldMapData->CaptureParams.CameraOrthoWidth;

		Maps.insert({ InMap, Context });
		PLOG(debug, "[DigSpotESPFeature] Added a map into the cache");
		LastUpdate = 0; // Force an update due to a new map being registered
		return true;
	}

	return false;
}

//---------------------------------------------------------------------------

void DigSpotESPFeature::OnActorAggregationUpdate(const ActorAggregationUpdateEvent& Event)
{
	if (!Event.Actor->IsA(AXMarksTheSpotMap::StaticClass()))
		return;

	AXMarksTheSpotMap* Map = (AXMarksTheSpotMap*)Event.Actor;

	auto Found = Maps.find(Map);

	if (Event.UpdateType == ActorAggregationUpdateEvent::EType::Added)
	{
		if (Event.Actor->GetOwner() != GetPlayer())
		{
			PLOG(warn, "[DigSpotESPFeature] Attempted to add a map that isn't owned by the local player");
			return;
		}

		if (RegistrationQueue.find(Map) != RegistrationQueue.end())
		{
			PLOG(warn, "[DigSpotESPFeature] Attempted to add a map that is in queue for registration");
			return;
		}

		if (Found != Maps.end())
		{
			PLOG(warn, "[DigSpotESPFeature] Attempted to add a map that was already registered");
			return;
		}

		RegistrationQueue.insert(Map);

		PLOG(debug, "[DigSpotESPFeature] Registered map to queue");
	}
	else
	{
		PLOG(debug, "[DigSpotESPFeature] Deregistering map");

		Maps.erase(Map);

		RegistrationQueue.erase(Map);

	}
}

//---------------------------------------------------------------------------