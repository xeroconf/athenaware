#include "LastDeathLocationFeature.h"

//---------------------------------------------------------------------------

LastDeathESPFeature::LastDeathESPFeature() : 
  scaffold::Feature(xorstr_("Last Death Location ESP"))
{
};

//---------------------------------------------------------------------------

bool LastDeathLocationFeature::OnStart()
{
	return true;
}

//---------------------------------------------------------------------------

bool LastDeathLocationFeature::OnEnd()
{
	return true;
}

//---------------------------------------------------------------------------

void LastDeathLocationFeature::OnReset(EFeatureResetReason InResetReason) 
{ 
	bLastDeathSet = false;
	Location = FVector();
}

//---------------------------------------------------------------------------

void LastDeathLocationFeature::PostTick(float DeltaTime) {}

//---------------------------------------------------------------------------

void LastDeathLocationFeature::Tick(float DeltaTime) {}

//---------------------------------------------------------------------------

bool LastDeathLocationFeature::CanExecute()
{
	return GetCfg().Visuals.Players.DrawLastDeath->Get() && GetPlayer() && GetController();
}

//---------------------------------------------------------------------------

void LastDeathLocationFeature::OnConditionChange(bool bConditionChange) {}

//---------------------------------------------------------------------------

void LastDeathLocationFeature::OnExecute(float DeltaTime) 
{
	if (GetPlayer()->IsDead())
	{
		Location = GetPlayer()->K2_GetActorLocation();
		bLastDeathSet = true;
	}
	
	if (!bLastDeathSet)
		return;

	auto& Dm = DrawService::Get();
	auto Layer = Dm.PushLayer(DRAWLIST_VISUALS_FORE);

	int Distance = (int)GetPlayer()->K2_GetActorLocation().DistanceMeter(Location);

	FVector2D Screen;
	if (!GetController()->ProjectWorldLocationToScreen(Location + FVector({ 0, 0, 15000 }), Screen))
		return;

	ImColor Color = GetCfg().Visuals.Players.Colors.LastDeath->Get();

	FVector2D ScreenCenter = ScreenSize / 2;
	const bool IsHovered = Screen.Distance(ScreenCenter) < 400.f;

	Dm.AddLabeledText(IsHovered, ICON_FA_SKULL, std::format("{}m", Distance).data(), Color, Color, Screen, EFontFace::PoppinsBold, EFontFace::PoppinsBold, Dm.ScaleFont(14.f), EAlign::Center, {2,2}, 0.5f, 4.0f);

	Dm.PopLayer();
}

//---------------------------------------------------------------------------

void LastDeathLocationFeature::OnDiscard(float DeltaTime) {}

//---------------------------------------------------------------------------