#include "TriggerBotFeature.h"

//---------------------------------------------------------------------------

TriggerBotFeature::TriggerBotFeature(std::shared_ptr<scaffold::Messenger> InMessenger) : 
  scaffold::Feature(xorstr_("Trigger Bot")), 
  Dispatcher(std::move(InMessenger)) 
{
};

//---------------------------------------------------------------------------

bool TriggerBotFeature::OnStart()
{
	if (!Dispatcher->Subscribe<TargetLockedOnEvent>(
		AimbotLockHandle,
		[this](const TargetLockedOnEvent& Event)
		{
			OnAimbotLock(Event);
		}
	))
	{
		return false;
	}

	return true;
}

//---------------------------------------------------------------------------

bool TriggerBotFeature::OnEnd()
{
	Dispatcher->Unsubscribe(AimbotLockHandle);

	return true;
}

//---------------------------------------------------------------------------

void TriggerBotFeature::OnReset(EFeatureResetReason InResetReason) {}

//---------------------------------------------------------------------------

void TriggerBotFeature::PostTick(float DeltaTime) {}

//---------------------------------------------------------------------------

void TriggerBotFeature::Tick(float DeltaTime) {}

//---------------------------------------------------------------------------

bool TriggerBotFeature::CanExecute()
{
	return GetCfg().Aim.Weapon.Triggerbot.Enabled->Get() && GetPlayer() && GetPlayer()->IsDead() == false;
}

//---------------------------------------------------------------------------

void TriggerBotFeature::OnConditionChange(bool bConditionChange) {}

//---------------------------------------------------------------------------

void TriggerBotFeature::OnExecute(float DeltaTime) {}

//---------------------------------------------------------------------------

void TriggerBotFeature::OnDiscard(float DeltaTime) {}

//---------------------------------------------------------------------------

void TriggerBotFeature::OnAimbotLock(const TargetLockedOnEvent& Ev)
{
	if (!GetCfg().Aim.Weapon.Triggerbot.Enabled->Get())
	{
		return;
	}

	if (GetPlayer() && GetPlayer()->IsDead() == true)
	{
		return;
	}

	if (auto Me = GetPlayer())
	{
		if (!Me->WieldedItemComponent)
		{
			return;
		}

		AProjectileWeapon* Weapon = (AProjectileWeapon*)Me->WieldedItemComponent->CurrentlyWieldedItem;

		if (!Weapon)
		{
			return;
		}

		if (!Weapon->IsA(AProjectileWeapon::StaticClass()))
		{
			return;
		}

		if (Weapon->State == EProjectileWeaponState::Aiming)
		{
			Me->UseItem(UPrimaryItemUsePressedNotificationInputId::StaticClass());
		}
	}
}

//---------------------------------------------------------------------------
