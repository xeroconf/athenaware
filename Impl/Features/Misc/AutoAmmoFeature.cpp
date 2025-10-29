#include "AutoAmmoFeature.h"

//---------------------------------------------------------------------------

AutoAmmoFeature::AutoAmmoFeature() : 
  scaffold::Feature("Auto Ammo")
{
};

//---------------------------------------------------------------------------

bool AutoAmmoFeature::OnStart()
{
	return true;
}

//---------------------------------------------------------------------------

bool AutoAmmoFeature::OnEnd()
{
	return true;
}

//---------------------------------------------------------------------------

void AutoAmmoFeature::OnReset(EFeatureResetReason InResetReason) {}

//---------------------------------------------------------------------------

void AutoAmmoFeature::PostTick(float DeltaTime) {}

//---------------------------------------------------------------------------

void AutoAmmoFeature::Tick(float DeltaTime) {}

//---------------------------------------------------------------------------

bool AutoAmmoFeature::CanExecute()
{
	if (
		GetCfg().Other.Misc.Interactables.AutoAmmo->Get()
		&& GetPlayer()
		&& GetPlayer()->IsDead() == false
		&& GetPlayer()->WieldedItemComponent
		&& GetPlayer()->WieldedItemComponent->CurrentlyWieldedItem
		&& GetPlayer()->WieldedItemComponent->CurrentlyWieldedItem->IsA(AProjectileWeapon::StaticClass()))
	{
		AProjectileWeapon* Weapon = (AProjectileWeapon*)GetPlayer()->WieldedItemComponent->CurrentlyWieldedItem;

		if (Weapon->AmmoLeft < Weapon->WeaponParameters.AmmoClipSize)
			return true;
	}

	return false;
}

//---------------------------------------------------------------------------

void AutoAmmoFeature::OnConditionChange(bool bConditionChange) {}

//---------------------------------------------------------------------------

void AutoAmmoFeature::OnExecute(float DeltaTime) 
{

	AProjectileWeapon* Weapon = (AProjectileWeapon*)GetPlayer()->WieldedItemComponent->CurrentlyWieldedItem;

	// Delay interaction so we're not spamming interact RPC which can cause desync/throttling
	if (Timestamp::GetTimeSince(LastInteractionCheck) >= 1000)
	{
		auto& Boxes = ActorService::Get().GetActorsOfType<AAmmoChest>();

		// Odd state, don't push an interaction.
		if (
			Weapon->State == EProjectileWeaponState::Equipping || 
			Weapon->State == EProjectileWeaponState::Reloading || 
			Weapon->State == EProjectileWeaponState::InterruptedReload
		)
		{
			LastInteractionCheck = Timestamp::Now();
			return;
		}

		for (auto Box : Boxes)
		{
			if (!GetPlayer()->IsInteractionValid(Box))
				continue;

			// Update client side weapon ammo clip
			Weapon->AmmoLeft = Weapon->WeaponParameters.AmmoClipSize;

			GetPlayer()->Interact(Box);

			// Update client side weapon ammo clip
			Weapon->AmmoLeft = Weapon->WeaponParameters.AmmoClipSize;

			break;  // Break out since we've interacted with a box already
		}

		LastInteractionCheck = Timestamp::Now();
	}
}

//---------------------------------------------------------------------------

void AutoAmmoFeature::OnDiscard(float DeltaTime) {}

//---------------------------------------------------------------------------