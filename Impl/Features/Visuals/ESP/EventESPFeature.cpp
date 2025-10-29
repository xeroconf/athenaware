#include "EventESPFeature.h"

//---------------------------------------------------------------------------

EventESPFeature::EventESPFeature(std::shared_ptr<scaffold::Messenger> InMessenger) : 
  scaffold::Feature(xorstr_("Event Signal ESP")), 
  Dispatcher(std::move(InMessenger)) 
{
};

//---------------------------------------------------------------------------

bool EventESPFeature::OnStart()
{
    // Using engine service now
    //DependencyLoaderFeature* Loader = (DependencyLoaderFeature*)MainDomain::Get().GetFeatureManager()->GetFeature(xorstr_("F_GENERAL_DEPENDENCY_LOADER"));

    auto& es = EngineService::Get();

    if (!bSetInternalAssets)
    {
        FortOfFortuneSignal = (UClass*)es.GetDependency(xorstr_("BP_LegendSkellyFort_SkullCloud_C"));
        FortOfDamnedSignal = (UClass*)es.GetDependency(xorstr_("BP_SkellyFort_RitualSkullCloud_C"));
        FortSkeletonSignal = (UClass*)es.GetDependency(xorstr_("BP_SkellyFort_SkullCloud_C"));
        FleetSignal = (UClass*)es.GetDependency(xorstr_("BP_SkellyShip_ShipCloud_C"));
        AshenWindsSignal = (UClass*)es.GetDependency(xorstr_("BP_AshenLord_SkullCloud_C"));
        FlameheartSignal = (UClass*)es.GetDependency(xorstr_("BP_GhostShip_TornadoCloud_C"));

        // Register the event types
        EventTypes = {
            { EGameplayEventType::AshenWinds, GameplayEventData(xorstr_("Ashen Winds")) },
            { EGameplayEventType::Flameheart, GameplayEventData(xorstr_("Flameheart")) },
            { EGameplayEventType::Fleet, GameplayEventData(xorstr_("Skeleton Fleet")) },
            { EGameplayEventType::FortOfDamned, GameplayEventData(xorstr_("Fort of the Damned")) },
            { EGameplayEventType::FortOfFortune, GameplayEventData(xorstr_("Fort of Fortune")) },
            { EGameplayEventType::FortOfSkeleton, GameplayEventData(xorstr_("Skeleton Fort")) },
            { EGameplayEventType::Unknown, GameplayEventData(xorstr_("Event")) }
        };

        bSetInternalAssets = true;
    }

    if (!Dispatcher->Subscribe<ActorAggregationUpdateEvent>(AggregationUpdateHandle, [this](const ActorAggregationUpdateEvent& Ev) { OnActorAggregationUpdate(Ev); }))
    {
        return false;
    }

    // Prepare existing signals
    auto& Signals = ActorService::Get().GetActorsOfType<AGameplayEventSignal>();
    for (auto Signal : Signals)
    {
        OnActorAggregationUpdate(
            ActorAggregationUpdateEvent
            (
                Signal,
                AGameplayEventSignal::StaticClass(),
                ActorAggregationUpdateEvent::EType::Added
            )
        );
    }


	return true;
}

//---------------------------------------------------------------------------

bool EventESPFeature::OnEnd()
{
    if (!Dispatcher->Unsubscribe(AggregationUpdateHandle))
    {
        return false;
    }

	return true;
}

//---------------------------------------------------------------------------

void EventESPFeature::OnReset(EFeatureResetReason InResetReason) { ActiveEvents.clear(); }

//---------------------------------------------------------------------------

void EventESPFeature::PostTick(float DeltaTime) {}

//---------------------------------------------------------------------------

void EventESPFeature::Tick(float DeltaTime) {}

//---------------------------------------------------------------------------

bool EventESPFeature::CanExecute()
{
	return GetCfg().Visuals.World.DrawEvents->Get() 
        && GetController() 
        && GetPlayer() 
        && GetCamera();
}

//---------------------------------------------------------------------------

void EventESPFeature::OnConditionChange(bool bConditionChange) {}

//---------------------------------------------------------------------------

void EventESPFeature::OnExecute(float DeltaTime) 
{
    auto& Config = GetCfg();
    auto& Dm = DrawService::Get();

    Dm.PushLayer(DRAWLIST_VISUALS_FORE);

    for (auto& Eventry : ActiveEvents)
    {
        AGameplayEventSignal* Signal = Eventry.first;
        GameplayEventData& Data = EventTypes[Eventry.second];

        FVector Location = Signal->K2_GetActorLocation();

        FVector2D ScreenPos;
        if (!GetController()->ProjectWorldLocationToScreen(Location, ScreenPos))
            continue;

        const int Distance = Location.DistanceMeter(GetCamera()->CameraCache.POV.Location);

        ImColor Color = Config.Visuals.World.Colors.Events->Get();


        FVector2D ScreenCenter = ScreenSize / 2;

        bool bIsHovered = ScreenPos.Distance(ScreenCenter) < 450.f;

        Dm.AddLabeledText(bIsHovered, Data.Name.data(), std::format("{}m", Distance).data(), Color, Color, ScreenPos, EFontFace::PoppinsBold, EFontFace::PoppinsBold, Dm.ScaleFont(14.f), EAlign::Center, { 2,2 }, 0.5f, 4.0f);
    }

    Dm.PopLayer();
}

//---------------------------------------------------------------------------

void EventESPFeature::OnDiscard(float DeltaTime) {}

//---------------------------------------------------------------------------

void EventESPFeature::OnActorAggregationUpdate(const ActorAggregationUpdateEvent& Ev)
{
    if (Ev.AssignedClass != AGameplayEventSignal::StaticClass())
        return;

    AGameplayEventSignal* Signal = (AGameplayEventSignal*)Ev.Actor;

    auto Found = ActiveEvents.find(Signal);

    if (Ev.UpdateType == ActorAggregationUpdateEvent::EType::Added)
    {
        if (Found != ActiveEvents.end())
            return;

        ActiveEvents.insert({ Signal, GetEventTypeFromSignal(Signal) });
    }
    else
    {
        if (Found == ActiveEvents.end())
            return;

        ActiveEvents.erase(Found);
    }
}

//---------------------------------------------------------------------------

EventESPFeature::EGameplayEventType EventESPFeature::GetEventTypeFromSignal(AGameplayEventSignal* InSignal)
{
    
    if (InSignal->IsA(FortOfFortuneSignal))
    {
        return EventESPFeature::EGameplayEventType::FortOfFortune;
    }
    else if (InSignal->IsA(FortOfDamnedSignal))
    {
        return EventESPFeature::EGameplayEventType::FortOfDamned;
    }
    else if (InSignal->IsA(FortSkeletonSignal))
    {
        return EventESPFeature::EGameplayEventType::FortOfSkeleton;
    }
    else if (InSignal->IsA(FleetSignal))
    {
        return EventESPFeature::EGameplayEventType::Fleet;
    }
    else if (InSignal->IsA(AshenWindsSignal))
    {
        return EventESPFeature::EGameplayEventType::AshenWinds;
    }
    else if (InSignal->IsA(FlameheartSignal))
    {
        return EventESPFeature::EGameplayEventType::Flameheart;
    }
    else
    {
        return EventESPFeature::EGameplayEventType::Unknown;
    }
}

//---------------------------------------------------------------------------