#include "ItemESPFeature.h"

//---------------------------------------------------------------------------

ItemESPFeature::ItemESPFeature() : 
  scaffold::Feature(xorstr_("Item ESP"))
{
};

//---------------------------------------------------------------------------

bool ItemESPFeature::OnStart()
{

	return true;
}

//---------------------------------------------------------------------------

bool ItemESPFeature::OnEnd()
{
	return true;
}

//---------------------------------------------------------------------------

void ItemESPFeature::OnReset(EFeatureResetReason InResetReason)
{
}

//---------------------------------------------------------------------------

void ItemESPFeature::PostTick(float DeltaTime) {}

//---------------------------------------------------------------------------

void ItemESPFeature::Tick(float DeltaTime) {}

//---------------------------------------------------------------------------

bool ItemESPFeature::CanExecute()
{
	return GetPlayer()
		&& GetController()
		&& (GetCfg().Visuals.World.DrawBooty->Get() || GetCfg().Visuals.World.DrawSupplies->Get());
}

//---------------------------------------------------------------------------

void ItemESPFeature::OnDiscard(float DeltaTime)
{

}

//---------------------------------------------------------------------------

void ItemESPFeature::OnExecute(float DeltaTime)
{
	
	auto& Dm = DrawService::Get();
	auto& Config = GetCfg();

	Dm.PushLayer(DRAWLIST_VISUALS_BACK);

	auto& CurrentItems = ActorService::Get().GetActorsOfType<AItemProxy>();
	for (AItemProxy* Item : CurrentItems)
	{
		AItemInfo* Info = Item->ItemInfo;

		if (!Info || !Info->Desc) continue;

		if (Info->CanBeStoredInInventory && !Config.Visuals.World.DrawSupplies->Get())
			continue;

		if (Info->CanBeStoredInInventory && !Config.Visuals.World.DrawBooty->Get())
			continue;

		FVector2D ScreenPos;
		if (!GetController()->ProjectWorldLocationToScreen(Item->K2_GetActorLocation(), ScreenPos))
			continue;

		ImColor Color = Info->CanBeStoredInInventory ? Config.Visuals.World.Colors.Supplies->Get() : Config.Visuals.World.Colors.Booty->Get();

		if (!Info->Desc->Title.String || !Info->Desc->Title.String->IsValid())
			continue;

		Dm.AddText(Info->Desc->Title.String->ToString(), ScreenPos, Color, EAlign::Center, EFontFx::Outline, Dm.ScaleFont(14.f), EFontFace::Poppins);
	}

	Dm.PopLayer();
}


//---------------------------------------------------------------------------


