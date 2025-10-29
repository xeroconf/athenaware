#include "IslandESPFeature.h"

//---------------------------------------------------------------------------

IslandESPFeature::IslandESPFeature() : 
  scaffold::Feature(xorstr_("Island ESP"))
{
};

//---------------------------------------------------------------------------

bool IslandESPFeature::OnStart()
{
	if (!CacheIslands())
	{
		PLOG(warn, "[IslandESPFeature] Failed to cache islands");
		return false;
	}

	return true;
}

//---------------------------------------------------------------------------

bool IslandESPFeature::OnEnd()
{
	return true;
}

//---------------------------------------------------------------------------

void IslandESPFeature::OnReset(EFeatureResetReason InResetReason)
{
	for (auto Island : Islands)
		delete Island;

	Islands.clear();
	bIslandsCached = false;

}

//---------------------------------------------------------------------------

void IslandESPFeature::PostTick(float DeltaTime) {}

//---------------------------------------------------------------------------

void IslandESPFeature::Tick(float DeltaTime)
{

}

//---------------------------------------------------------------------------

bool IslandESPFeature::CanExecute()
{
	AAthenaGameState* State = (AAthenaGameState*)GetWorld()->GameState;
	return State
		&& State->IslandService
		&& State->IslandService->IslandDataAsset
		&& GetController() && GetPlayer() && GetCamera();
}

//---------------------------------------------------------------------------

void IslandESPFeature::OnDiscard(float DeltaTime)
{

}

//---------------------------------------------------------------------------

void IslandESPFeature::OnExecute(float DeltaTime)
{
	auto& Config = GetCfg();
	auto& Dm = DrawService::Get();

	Dm.PushLayer(DRAWLIST_VISUALS_FORE);

	for (IslandEntry* Island : Islands)
	{
		FVector2D ScreenPos;
		if (!GetController()->ProjectWorldLocationToScreen(Island->Pos, ScreenPos))
			continue;

		
		if (Island->Type == EType::Outpost && !Config.Visuals.World.DrawOutpostIslands->Get())
			continue;

		if (Island->Type == EType::Reef && !Config.Visuals.World.DrawReefs->Get())
			continue;

		if (Island->Type == EType::Resource && !Config.Visuals.World.DrawResourceIslands->Get())
			continue;

		if (Island->Type == EType::Fortress && !Config.Visuals.World.DrawFortIslands->Get())
			continue;

		if (Island->Type == EType::Seapost && !Config.Visuals.World.DrawSeaposts->Get())
			continue;

		const int Distance = Island->Pos.DistanceMeter(GetCamera()->CameraCache.POV.Location);

		if (Distance > Config.Visuals.World.IslandMaxDistance->Get())
			continue;

		ImColor Color = Config.Visuals.World.Colors.Islands->Get();


		FVector2D ScreenCenter = ScreenSize / 2;

		bool bIsHovered = ScreenPos.Distance(ScreenCenter) < 650.f;
		float TargetFade = bIsHovered ? 1.0f : 0.35f;

		Island->RevealFade = UKismetMathLibrary::FInterpTo(Island->RevealFade, TargetFade, DeltaTime, 7.f);

		Dm.AddBoxedText(std::format("{} ({}m)", Island->Name, Distance).data(), ScreenPos, ImColor(Color.Value.x, Color.Value.y, Color.Value.z, Color.Value.w * Island->RevealFade), Dm.ScaleFont(14.f), EAlign::Center, EFontFace::PoppinsBold, {2,2}, 0.5f * Island->RevealFade, 4.0f);
	}

	Dm.PopLayer();
}

//---------------------------------------------------------------------------

bool IslandESPFeature::IsValidIsland(UIslandDataAssetEntry* InIsland)
{
	// Apply any island filtering here.. Copied from prismarine-4 source.

	if (InIsland->ShouldBeHiddenOnWorldMap == true)
		return false;

	if (!InIsland->WorldMapData)
		return false;

	FVector& IslandLocation = InIsland->WorldMapData->CaptureParams.WorldSpaceCameraPosition;

	if (IslandLocation.X == 0 && IslandLocation.Y == 0)
		return false;

	return true;
}

//---------------------------------------------------------------------------

bool IslandESPFeature::CacheIslands()
{
	AAthenaGameState* State = (AAthenaGameState*)GetWorld()->GameState;

	if (!State)
		return false;

	AIslandService* IslandService = State->IslandService;

	if (!IslandService)
		return false;

	UIslandDataAsset* IslandDataAsset = IslandService->IslandDataAsset;

	if (!IslandDataAsset)
		return false;

	if (IslandDataAsset->IslandDataEntries.Num() == 0)
		return false;

	for (UIslandDataAssetEntry* Island : IslandDataAsset->IslandDataEntries)
	{
		if (!IsValidIsland(Island))
			continue;

		FVector& IslandLocation = Island->WorldMapData->CaptureParams.WorldSpaceCameraPosition;

		IslandEntry* Entry = new IslandEntry;
		Entry->Name = Island->LocalisedName.String->ToString();
		Entry->Pos = IslandLocation;

		FIsland IslandData;
		GetFIsland(Island, IslandData);

		Entry->Type = GetIslandTypeFromGameType(IslandData.IslandType);
		Entry->BoundRadius = IslandData.IslandBoundsRadius;
		Entry->Rotation = IslandData.Rotation;

		Islands.insert(Entry);
	}

	return Islands.size() > 0;
}

//---------------------------------------------------------------------------

bool IslandESPFeature::GetFIsland(UIslandDataAssetEntry* InIsland, FIsland& OutIsland)
{
	AAthenaGameState* State = (AAthenaGameState*)GetWorld()->GameState;

	for (auto& Island : State->IslandService->IslandArray)
	{
		if (Island.IslandName.ComparisonIndex == InIsland->IslandName.ComparisonIndex)
		{
			OutIsland = Island;
			return true;
		}
	}
	return false;
}

//---------------------------------------------------------------------------

IslandESPFeature::EType IslandESPFeature::GetIslandTypeFromGameType(EIslandType InGameType)
{
	switch (InGameType)
	{
	case EIslandType::Outpost:
	case EIslandType::ReapersHideout:
		return IslandESPFeature::EType::Outpost;
	case EIslandType::SeaPost:
		return IslandESPFeature::EType::Seapost;
	case EIslandType::FortOfTheDamned:
	case EIslandType::Fort:
	case EIslandType::SeaFortPrime1:
	case EIslandType::SeaFortPrime2:
	case EIslandType::SeaFortOvergrown1:
	case EIslandType::SeaFortOvergrown2:
	case EIslandType::SeaFortPrison1:
	case EIslandType::SeaFortPrison2:
		return IslandESPFeature::EType::Fortress;
	case EIslandType::Reef:
	case EIslandType::Sunken:
	case EIslandType::SunkenKingdomNonStarlight:
		return IslandESPFeature::EType::Reef;
	default:
		return IslandESPFeature::EType::Resource;
	}
}

//---------------------------------------------------------------------------

std::set<IslandESPFeature::IslandEntry*>& IslandESPFeature::GetCachedIslands()
{
	return Islands;
}

//---------------------------------------------------------------------------

void IslandESPFeature::OnActorAggregationUpdate(const ActorAggregationUpdateEvent& Event)
{
	if (Event.AssignedClass != AGameplayEventSignal::StaticClass())
		return;

	AGameplayEventSignal* Signal = (AGameplayEventSignal*)Event.Actor;


}

//---------------------------------------------------------------------------