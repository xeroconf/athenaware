#include "AutoSwapFeature.h"

//---------------------------------------------------------------------------

AutoSwapFeature::AutoSwapFeature(std::shared_ptr<scaffold::Messenger> InMessenger) : 
  scaffold::Feature("Auto Swap"), 
  Dispatcher(std::move(InMessenger)) 
{
};

//---------------------------------------------------------------------------

bool AutoSwapFeature::OnStart()
{
	if (!Dispatcher->Subscribe<SendSerializedRpcEvent>(
		SendSerializedRpcHandle,
		[this](const SendSerializedRpcEvent& Event)
		{
			OnSendSerializedRpc(Event);
		}
	))
	{
		return false;
	}

	return true;
}

//---------------------------------------------------------------------------

bool AutoSwapFeature::OnEnd()
{
	Dispatcher->Unsubscribe(SendSerializedRpcHandle);

	return true;
}

//---------------------------------------------------------------------------

void AutoSwapFeature::OnReset(EFeatureResetReason InResetReason) 
{
	SwapWeaponNextFrame = false;
}

//---------------------------------------------------------------------------

void AutoSwapFeature::PostTick(float DeltaTime) {}

//---------------------------------------------------------------------------

void AutoSwapFeature::Tick(float DeltaTime) {}

//---------------------------------------------------------------------------

bool AutoSwapFeature::CanExecute()
{
	const auto Player = GetPlayer();

	return GetCfg().Aim.Weapon.AutoSwap.Enabled->Get() && 
		Player &&
		Player->IsDead() == false &&
		Player->InventoryManipulatorComponent &&
		Player->WieldedItemComponent &&
		Player->WieldedItemComponent->CurrentlyWieldedItem &&
		Player->WieldedItemComponent->CurrentlyWieldedItem->IsA(AProjectileWeapon::StaticClass());
}

//---------------------------------------------------------------------------

void AutoSwapFeature::OnConditionChange(bool bConditionChange) 
{
    Dispatcher->Emit(
        FeatureToggledEvent(
            GetId(),
            GetFriendlyName(),
            bConditionChange
        )
	);
}

//---------------------------------------------------------------------------

void AutoSwapFeature::OnExecute(float DeltaTime) 
{
	const auto Player = GetPlayer();
	if (SwapWeaponNextFrame)
	{
		SwapWeaponNextFrame = false;

		Player->InventoryManipulatorComponent->CycleItemCategory(UWeaponItemCategory::StaticClass());
	}
}

//---------------------------------------------------------------------------

void AutoSwapFeature::OnDiscard(float DeltaTime) {}

//---------------------------------------------------------------------------

void AutoSwapFeature::OnSendSerializedRpc(const SendSerializedRpcEvent& Ev)
{
	if (Ev.ContentsType != FFireWeaponOnServerRpc::StaticStruct())
		return;

	if (!GetCfg().Aim.Weapon.AutoSwap.Enabled->Get())
		return;

	SwapWeaponNextFrame = true;
}

//---------------------------------------------------------------------------
