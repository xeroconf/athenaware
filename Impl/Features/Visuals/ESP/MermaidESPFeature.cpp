#include "MermaidESPFeature.h"

//---------------------------------------------------------------------------

MermaidESPFeature::MermaidESPFeature(std::shared_ptr<scaffold::Messenger> InMessenger) : 
  scaffold::Feature(xorstr_("Mermaid ESP")), 
  Dispatcher(std::move(InMessenger))
{
};

//---------------------------------------------------------------------------

bool MermaidESPFeature::OnStart()
{
	if (!Dispatcher->Subscribe<ActorAggregationUpdateEvent>(AggregationUpdateHandle, [this](const ActorAggregationUpdateEvent& Event) { OnActorAggregationUpdate(Event); }))
	{
		return false;
	}

	return true;
}

//---------------------------------------------------------------------------

bool MermaidESPFeature::OnEnd()
{
	if (!Dispatcher->Unsubscribe(AggregationUpdateHandle))
	{
		return false;
	}

	return true;
}

//---------------------------------------------------------------------------

void MermaidESPFeature::OnReset(EFeatureResetReason InResetReason) { Mermaids.clear(); }

//---------------------------------------------------------------------------

void MermaidESPFeature::PostTick(float DeltaTime) {}

//---------------------------------------------------------------------------

void MermaidESPFeature::Tick(float DeltaTime) {}

//---------------------------------------------------------------------------

bool MermaidESPFeature::CanExecute()
{
	return GetCfg().Visuals.Creatures.DrawMermaids->Get() && GetPlayer() && GetController();
}

//---------------------------------------------------------------------------

void MermaidESPFeature::OnConditionChange(bool bConditionChange) {}

//---------------------------------------------------------------------------

void MermaidESPFeature::OnExecute(float DeltaTime) 
{
	auto& Config = GetCfg();

	auto& Dm = DrawService::Get();
	Dm.PushLayer(DRAWLIST_VISUALS_BACK);

	for (auto& Entry : Mermaids)
	{
		AMermaid* Mermaid = Entry.first;

		// Don't show mermaids that are diving/disappearing.
		// As they can deceive and are not interactable in this state.
		if (Mermaid->MermaidState == 2)
			continue;

		FVector Location = Mermaid->K2_GetActorLocation();

		FVector2D Screen;
		if (!GetController()->ProjectWorldLocationToScreen(Location, Screen))
			continue;

		FVector2D ScreenCenter = ScreenSize / 2;

		const int DistanceMeters = GetPlayer()->K2_GetActorLocation().DistanceMeter(Location);
		const bool bIsHovered = ScreenCenter.Distance(Screen) < 300.f;

		Dm.AddLabeledText(bIsHovered, ICON_FA_OTTER, std::vformat(xorstr_("{}m"), std::make_format_args(DistanceMeters)).c_str(), GetCfg().Visuals.Creatures.Colors.Mermaids->Get(), GetCfg().Visuals.Creatures.Colors.Mermaids->Get(), Screen, EFontFace::PoppinsBold, EFontFace::PoppinsBold, DrawService::Get().ScaleFont(14.f), EAlign::Center, { 2.f, 2.f }, 0.5f, 4.0f);
	}

	Dm.PopLayer();
}

//---------------------------------------------------------------------------

void MermaidESPFeature::OnDiscard(float DeltaTime) {}

//---------------------------------------------------------------------------

void MermaidESPFeature::OnActorAggregationUpdate(const ActorAggregationUpdateEvent& Event)
{
	if (Event.AssignedClass != AMermaid::StaticClass())
		return;

	AMermaid* Mermaid = (AMermaid*)Event.Actor;

	if (Event.UpdateType == ActorAggregationUpdateEvent::EType::Added)
	{
		auto Found = Mermaids.find(Mermaid);
		if (Found != Mermaids.end())
		{
			return;
		}

		AddMermaid(Mermaid);
	}
	else if (Event.UpdateType == ActorAggregationUpdateEvent::EType::Removed)
	{
		auto Found = Mermaids.find(Mermaid);
		if (Found == Mermaids.end())
		{
			return;
		}

		RemoveMermaid(Mermaid);
	}
}

//---------------------------------------------------------------------------

void MermaidESPFeature::AddMermaid(AMermaid* InMermaid)
{
	Mermaids.insert({ InMermaid, ContextData() });
}

//---------------------------------------------------------------------------

void MermaidESPFeature::RemoveMermaid(AMermaid* InMermaid)
{
	Mermaids.erase(InMermaid);
}


//---------------------------------------------------------------------------