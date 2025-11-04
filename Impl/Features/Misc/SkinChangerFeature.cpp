#include "SkinChangerV2Feature.h"

//---------------------------------------------------------------------------

SkinChangerV2Feature::SkinChangerV2Feature(std::shared_ptr<scaffold::Messenger> InMessenger, std::shared_ptr<HttpClient> InPrismarineServiceHttpClient) : 
  scaffold::Feature(xorstr_("Skin Changer V2")), 
  Dispatcher(std::move(InMessenger)),
  ServiceHttpClient(std::move(InPrismarineServiceHttpClient))
{
};

//---------------------------------------------------------------------------

bool SkinChangerV2Feature::OnStart()
{
	Dispatcher->Subscribe<ProcessRemoteFunctionEvent>(ProcessRemoteFunctionHandle, [this](const ProcessRemoteFunctionEvent& Event) { OnProcessRemoteFunction(Event); });
	if (!ProcessRemoteFunctionHandle.IsRegistered())
	{
		PLOG(error, "[SkinChangerV2Feature] Failed to register ProcessRemoteFunctionHandle");
		return false;
	}

	Dispatcher->Subscribe<ConfigUpdateEvent>(ConfigUpdateEventHandle, [this](const ConfigUpdateEvent& Event) { OnConfigUpdateEvent(Event); });
	if (!ConfigUpdateEventHandle.IsRegistered())
	{
		PLOG(error, "[SkinChangerV2Feature] Failed to register ConfigUpdateEventHandle");
		return false;
	}

	/*Dispatcher->Subscribe<ReceivedRemoteAssetContentEvent>(ReceivedAssetContentHandle, [this](const ReceivedRemoteAssetContentEvent& Event) { OnReceivedRemoteAssetContent(Event); });
	if (!ReceivedAssetContentHandle.IsRegistered())
	{
		PLOG(error, "[SkinChangerV2Feature] Failed to register ReceivedAssetContentHandle");
		return false;
	}*/

	if (!ServerWieldFunction)
		ServerWieldFunction = UObject::FindObject<UFunction>(xorstr_("Function Athena.WieldedItemComponent.Server_WieldRPC"));

	// Set all the skins as null
	AppliedSkins =
	{
		{ ESkinChangerWieldableType::Blunderbuss, nullptr },
		{ ESkinChangerWieldableType::EyeOfReach, nullptr },
		{ ESkinChangerWieldableType::ThrowingKnives, nullptr }, // Not supported at this time...
		{ ESkinChangerWieldableType::DoubleBarrel, nullptr },
		{ ESkinChangerWieldableType::Pistol, nullptr },
		{ ESkinChangerWieldableType::Sword, nullptr }
	};

	std::filesystem::path UserSkinsPath = ConfigService::Get().GetWorkingDirectory() / FEATURE_SKIN_CHANGER_CUSTOM_SKINS_FILE;

	// Check if the custom skins folder exists and if it doesn't create it...
	if (!std::filesystem::exists(UserSkinsPath))
	{
		std::ofstream File;
		File.open(UserSkinsPath);
		File << xorstr_("[]") << std::endl;
		File.close();
	}

	// Apply functions

	auto GenericSkeletalMeshApplyFunction = [this](AWieldableItem* InItem, SkinChangerWeaponSkinEntry* InSkin)
	{
			if (!InItem || !InSkin)
				return false;

			// Now lets begin to change the wieldables skin
			USkeletalMeshMemoryConstraintComponent* MeshComponent = (USkeletalMeshMemoryConstraintComponent*)InItem->FirstPersonMesh;

			if (!MeshComponent || InSkin->Meshes.empty())
				return false;

			// Get the first mesh in the vector
			auto MeshToApply = InSkin->Meshes[0];

			// Check if the mesh is already applied and if so just return TRUE. We'd want to assume we loaded regardless
			// if we did or not since a failed attempt is treated as an error.
			if (MeshComponent->SkeletalMesh == MeshToApply)
				return true;

			// Clear the current materials
			if (MeshComponent->OverrideMaterials.IsValid())
				MeshComponent->OverrideMaterials.Clear();

			// Set skeletal mesh
			MeshComponent->SetSkeletalMesh((USkeletalMesh*)MeshToApply);

			// If this skin has materials, apply them
			if (InSkin->bHasMaterials)
			{
				for (const auto& Entry : InSkin->Materials)
				{
					MeshComponent->SetMaterial(Entry.Index, Entry.Material);
				}
			}

			PLOG(debug, "[SkinChangerV2Feature][GenericSkeletalMeshApplyFunction] Applied skin");
			return true;
	};

	auto GenericStaticMeshApplyFunction = [this](AWieldableItem* InItem, SkinChangerWeaponSkinEntry* InSkin)
	{
		if (!InItem || !InSkin)
			return false;

		// Now lets begin to change the wieldables skin
		UStaticMeshMemoryConstraintComponent* MeshComponent = (UStaticMeshMemoryConstraintComponent*)InItem->FirstPersonMesh;


		if (!MeshComponent || InSkin->Meshes.empty())
			return false;

		// Get the first mesh in the vector
		auto MeshToApply = InSkin->Meshes[0];

		// Check if the mesh is already applied and if so just return TRUE. We'd want to assume we loaded regardless
		// if we did or not since a failed attempt is treated as an error.
		if (MeshComponent->StaticMesh == MeshToApply)
			return true;

		// Clear the current materials
		if (MeshComponent->OverrideMaterials.IsValid())
			MeshComponent->OverrideMaterials.Clear();

		// Set skeletal mesh
		MeshComponent->SetStaticMesh((UStaticMesh*)MeshToApply);

		// If this skin has materials, apply them
		if (InSkin->bHasMaterials)
		{
			for (const auto& Entry : InSkin->Materials)
			{
				MeshComponent->SetMaterial(Entry.Index, Entry.Material);
			}
		}


		PLOG(debug, "[SkinChangerV2Feature][GenericSkeletalMeshApplyFunction] Applied skin");
		return true;
	};

	auto ThrowableKnifeApplyFunction = [this](AWieldableItem* InItem, SkinChangerWeaponSkinEntry* InSkin)
	{
			if (!InItem && !InSkin)
				return false;

			if (InSkin->Meshes.size() < 2 || InSkin->Materials.size() < 2)
				return false;

			auto PouchMesh = InSkin->Meshes[0];
			auto KnifeMesh = InSkin->Meshes[1];

			auto& PouchMaterialDef = InSkin->Materials[0];
			auto& KnifeMaterialDef = InSkin->Materials[1];

			AThrowableMeleeWeapon* ThrowableKnifeWeapon = (AThrowableMeleeWeapon*)InItem;

			return true;
	};


	// Remove functions
	auto GenericSkeletalMeshRemoveFunction = [this](AWieldableItem* InItem)
	{
			if (!InItem)
				return false;

			USkeletalMeshMemoryConstraintComponent* FirstPersonMesh = (USkeletalMeshMemoryConstraintComponent*)InItem->FirstPersonMesh;
			USkeletalMeshMemoryConstraintComponent* ThirdPersonMesh = (USkeletalMeshMemoryConstraintComponent*)InItem->ThirdPersonMesh;

			if (FirstPersonMesh == nullptr || ThirdPersonMesh == nullptr)
				return false;

			if (FirstPersonMesh->SkeletalMesh == nullptr || ThirdPersonMesh->SkeletalMesh == nullptr)
				return false;

			// The original skin is already set. We'll compare classes here since they'll always stay the same.
			// Not sure if there's different mesh instances used for both first and third.
			if (FirstPersonMesh->SkeletalMesh == ThirdPersonMesh->SkeletalMesh)
				return true;

			PLOG(debug, "[SkinChangerV2Feature][GenericSkeletalMeshRemoveFunction] Unapplied skin");

			FirstPersonMesh->SetSkeletalMesh(ThirdPersonMesh->SkeletalMesh);

			FirstPersonMesh->ResetDefaultMaterials();

			return true;
	};

	auto GenericStaticMeshRemoveFunction = [this](AWieldableItem* InItem)
	{
		if (!InItem)
			return false;

		UStaticMeshMemoryConstraintComponent* FirstPersonMesh = (UStaticMeshMemoryConstraintComponent*)InItem->FirstPersonMesh;
		UStaticMeshMemoryConstraintComponent* ThirdPersonMesh = (UStaticMeshMemoryConstraintComponent*)InItem->ThirdPersonMesh;

		if (FirstPersonMesh == nullptr || ThirdPersonMesh == nullptr)
			return false;

		if (FirstPersonMesh->StaticMesh == nullptr || ThirdPersonMesh->StaticMesh == nullptr)
			return false;

		// The original skin is already set. We'll compare classes here since they'll always stay the same.
		// Not sure if there's different mesh instances used for both first and third.
		if (FirstPersonMesh->StaticMesh == ThirdPersonMesh->StaticMesh)
			return true;

		PLOG(debug, "[SkinChangerV2Feature][GenericStaticMeshRemoveFunction] Unapplied skin");

		FirstPersonMesh->SetStaticMesh(ThirdPersonMesh->StaticMesh);

		FirstPersonMesh->ResetDefaultMaterials();

		return true;
	};
	
	SkinLifecycle[ESkinChangerWieldableType::Blunderbuss] = SkinLifecycleEntry(GenericSkeletalMeshApplyFunction, GenericSkeletalMeshRemoveFunction);
	SkinLifecycle[ESkinChangerWieldableType::Pistol] = SkinLifecycleEntry(GenericSkeletalMeshApplyFunction, GenericSkeletalMeshRemoveFunction);
	SkinLifecycle[ESkinChangerWieldableType::EyeOfReach] = SkinLifecycleEntry(GenericSkeletalMeshApplyFunction, GenericSkeletalMeshRemoveFunction);
	SkinLifecycle[ESkinChangerWieldableType::DoubleBarrel] = SkinLifecycleEntry(GenericSkeletalMeshApplyFunction, GenericSkeletalMeshRemoveFunction);
	SkinLifecycle[ESkinChangerWieldableType::Sword] = SkinLifecycleEntry(GenericStaticMeshApplyFunction, GenericStaticMeshRemoveFunction);
	SkinLifecycle[ESkinChangerWieldableType::ThrowingKnives] = SkinLifecycleEntry(ThrowableKnifeApplyFunction, GenericStaticMeshRemoveFunction);


#if FEATURE_SKIN_CHANGER_EXPORT_SKINS == 1
	// Export all game skins to (WORKING_DIR)\SkinsExport.json
	FindAndExportDefaultSkinDefinitions();
#else
	//const auto SkinsDefaultRemoteFile = xorstr_("DefaultSkinsV2.json");
	//NetworkService::Get().ServerRequestRepoContent(SkinsDefaultRemoteFile, FEATURE_SKIN_CHANGER_NONCE);

	// Retrieve skins from the service
	GetSkinsDataFromEndpoint();
#endif

	return true;
}

//---------------------------------------------------------------------------

bool SkinChangerV2Feature::OnEnd()
{
	if (!Dispatcher->Unsubscribe(ProcessRemoteFunctionHandle))
	{
		PLOG(error, "[SkinChangerV2Feature] Failed to deregister ProcessRemoteFunctionHandle");
	}

	if (!Dispatcher->Unsubscribe(ConfigUpdateEventHandle))
	{
		PLOG(error, "[SkinChangerV2Feature] Failed to deregister ConfigUpdateEventHandle");
	}

	/*if (!Dispatcher->Unsubscribe(ReceivedAssetContentHandle))
	{
		PLOG(error, "[SkinChangerV2Feature] Failed to deregister ReceivedAssetContentHandle");
	}*/

	AppliedSkins.clear();
	SkinLifecycle.clear();
	SkinsCache.clear();
	IconLoadQueue = {};
	LastSkinLoad = 0;
	DefaultSkinAssetDataBuffer.clear();
	bCustomSkinsLoaded = false;
	bForceUpdateNextFrame = false;

	return true;
}

//---------------------------------------------------------------------------

void SkinChangerV2Feature::OnReset(EFeatureResetReason InResetReason) {}

//---------------------------------------------------------------------------

void SkinChangerV2Feature::PostTick(float DeltaTime) {}

//---------------------------------------------------------------------------

void SkinChangerV2Feature::Tick(float DeltaTime) 
{
#if FEATURE_SKIN_CHANGER_USE_TICK_QUEUE_LOADING == 1
	// Load skin icons every 50ms. This will prevent a large initial lag spike which
	// was quite annoying for some people in Prismarine4, especially on bad hardware.
	// This will load them over time and by the time they get into a server, they all should be done.
	if (IconLoadQueue.empty())
		return;

	if (Timestamp::GetTimeSince(LastSkinLoad) > 50)
	{
		auto Skin = IconLoadQueue.front();
		IconLoadQueue.pop();

		if (Skin->IconPath.empty())
			return;

		UTexture2D* Texture = (UTexture2D*)EngineService::Get().StaticLoadObject(UTexture2D::StaticClass(), Skin->IconPath.c_str(), true, true, nullptr, 0);

		if (Texture)
		{
			Skin->Icon = Texture;
		}
		else
		{
			PLOGF(error, "[SkinChangerFeature] Failed to load a skin icon! Skin: {0}", Skin->Name.c_str());
		}

		LastSkinLoad = Timestamp::Now();
	}
#endif
}

//---------------------------------------------------------------------------

bool SkinChangerV2Feature::CanExecute()
{
	return IsInGameplay() && GetPlayer();
}

//---------------------------------------------------------------------------

void SkinChangerV2Feature::OnConditionChange(bool bConditionChange) {}

//---------------------------------------------------------------------------

void SkinChangerV2Feature::OnExecute(float DeltaTime) 
{
	if (GetPlayer()->WieldedItemComponent && GetPlayer()->WieldedItemComponent->CurrentlyWieldedItem)
	{
		AWieldableItem* Wieldable = GetPlayer()->WieldedItemComponent->CurrentlyWieldedItem;

		ESkinChangerWieldableType Type = GetTypeFromWieldable(Wieldable);

		if (Type == ESkinChangerWieldableType::Invalid)
			return;

		auto SkinToApply = AppliedSkins[Type];

		bool bWasSuccess = false;

		// Check if we have a skin set for this. Just skip it if we don't.
		if (SkinToApply)
		{
			bWasSuccess = ApplyWieldableSkin(SkinToApply, Wieldable);
		}
		else
		{
			bWasSuccess = RemoveWieldableSkin(Wieldable);
		}

		if (bWasSuccess == false)
		{
			// Nothing at the moment
		}
	}



	bForceUpdateNextFrame = false;
	LastLoadoutSkinUpdate = Timestamp::Now();

}

//---------------------------------------------------------------------------

void SkinChangerV2Feature::OnDiscard(float DeltaTime) {}

//---------------------------------------------------------------------------

bool SkinChangerV2Feature::SetSkin(ESkinChangerWieldableType InType, SkinChangerWeaponSkinEntry* InSkin)
{
	if (InType == ESkinChangerWieldableType::Invalid)
		return false;

	AppliedSkins[InType] = InSkin;

	return true;
}

//---------------------------------------------------------------------------

bool SkinChangerV2Feature::ApplyWieldableSkin(SkinChangerWeaponSkinEntry* InSkin, AWieldableItem* InItem)
{
	if (!InItem)
		return false;

	ESkinChangerWieldableType Type = GetTypeFromWieldable(InItem);

	// Not a registered wieldable type
	if (Type == ESkinChangerWieldableType::Invalid)
		return false;

	// Load the skin if it's not loaded yet
	if (InSkin->bIsLoaded == false)
	{
		FullyLoadSkin(InSkin);
	}

	// Call the application function
	auto& ApplyFunction = SkinLifecycle[Type].ApplyFunction;

	if (!ApplyFunction)
		return false;

	return ApplyFunction(InItem, InSkin);
}

//---------------------------------------------------------------------------

bool SkinChangerV2Feature::RemoveWieldableSkin(AWieldableItem* InItem)
{
	if (!InItem)
		return false;

	ESkinChangerWieldableType Type = GetTypeFromWieldable(InItem);

	// Not a registered wieldable type
	if (Type == ESkinChangerWieldableType::Invalid)
		return false;

	// Call the removal function
	auto& RemoveFunction = SkinLifecycle[Type].RemoveFunction;

	if (!RemoveFunction)
		return false;

	return RemoveFunction(InItem);
}

//---------------------------------------------------------------------------

ESkinChangerWieldableType SkinChangerV2Feature::GetTypeFromWieldable(AWieldableItem* InItem)
{
	if (!InItem)
		return ESkinChangerWieldableType::Invalid;

	if (InItem->IsA(AProjectileWeapon::StaticClass())) // Guns
	{
		AProjectileWeapon* Weapon = (AProjectileWeapon*)InItem;

		switch (Weapon->ProjectileWeaponType) 
		{
			case EProjectileWeaponType::Pistol:
				return ESkinChangerWieldableType::Pistol;
			case EProjectileWeaponType::Blunderbuss:
				return ESkinChangerWieldableType::Blunderbuss;
			case EProjectileWeaponType::EyeOfReach:
				return ESkinChangerWieldableType::EyeOfReach;
			case EProjectileWeaponType::DoubleBarrel:
				return ESkinChangerWieldableType::DoubleBarrel;
		}
	}
	else if (InItem->IsA(AThrowableMeleeWeapon::StaticClass())) // Throwing Knives
	{
		/*
		* Disable support for throwing knives...
		* Their mesh system is complex and thus not really supported
		* at this current point in time. Instead we'll just return
		* the invalid wieldable type so we can ignore this.
		* 
		* We can come back to this later to implement it. We've
		* somewhat accounted for this by allowing multi-mesh
		* skin definitions.
		*/

		AThrowableMeleeWeapon* Weapon = (AThrowableMeleeWeapon*)InItem;

		return ESkinChangerWieldableType::Invalid;
	}
	else if (InItem->IsA(APlayerMeleeWeapon::StaticClass())) // Sword
	{
		APlayerMeleeWeapon* Weapon = (APlayerMeleeWeapon*)InItem;

		return ESkinChangerWieldableType::Sword;
	}

	return ESkinChangerWieldableType::Invalid;
}

//---------------------------------------------------------------------------

bool SkinChangerV2Feature::LoadIcons(ESkinChangerWieldableType InType)
{
	const auto& Skins = SkinsCache[InType];

	PLOGF(debug, "[SkinChangerFeature] Loading {0} icons for weapon type {1}", Skins.size(), (int)InType);

#if FEATURE_SKIN_CHANGER_USE_TICK_QUEUE_LOADING == 1

	for (const auto& Skin : Skins)
	{
		// Add skins into the load queue.
		IconLoadQueue.push(Skin.second);
	}

#else

    // ZC:
	// Deleted this as TaskScheduler is now deprecated in scaffold.
    // Originally this just created a task for each skin to load after
    // X amount of milliseconds.

#endif

	return true;
}

//---------------------------------------------------------------------------

bool SkinChangerV2Feature::FullyLoadSkin(SkinChangerWeaponSkinEntry* InSkin)
{
	// Check if skin is already loaded. Return true since the skin is loaded already.
	if (InSkin->bIsLoaded)
		return true;

	PLOGF(debug, "[SkinChangerV2Feature] Beginning to fully load skin '{0}'", InSkin->Name);

	for (auto& Path : InSkin->MeshPaths)
	{
		UObject* Mesh = (UObject*)EngineService::Get().StaticLoadObject(UObject::StaticClass(), Path.c_str(), true, true, nullptr, 0);

		if (Mesh) 
			InSkin->Meshes.push_back(Mesh);
	}

	if (InSkin->Meshes.size() != InSkin->MeshPaths.size())
	{
		InSkin->bInvalidSkin = true;
		return false;
	}

	InSkin->bIsMultiMesh = InSkin->Meshes.size() > 1;

	// Load materials if it has any
	if (InSkin->bHasMaterials)
	{
		bool bMaterialsFailedToLoad = false;

		for (auto& MaterialEntry : InSkin->Materials)
		{
			UMaterialInstance* MaterialObject = (UMaterialInstance*)EngineService::Get().StaticLoadObject(UMaterialInstance::StaticClass(), MaterialEntry.Path.c_str(), true, true, nullptr, 0);

			// Break out and mark as failed to load if we cannot load the materials for this skin.
			if (!MaterialObject)
			{
				bMaterialsFailedToLoad = true;
				break;
			}

			MaterialEntry.Material = MaterialObject;
		}

		if (bMaterialsFailedToLoad)
		{
			InSkin->bInvalidSkin = true;
			return false;
		}
	}

	PLOGF(debug, "[SkinChangerV2Feature] Loaded skin '{0}' with {1} meshes and {2} materials", InSkin->Name, InSkin->Meshes.size(), InSkin->Materials.size());

	InSkin->bIsLoaded = true;

	return true;
}

//---------------------------------------------------------------------------

void SkinChangerV2Feature::OnProcessRemoteFunction(const ProcessRemoteFunctionEvent& Event)
{
	if (Event.Function != ServerWieldFunction)
		return;

	// Force update the skin rather than waiting for the update queue
	ForceSkinApplyUpdate();
}

//---------------------------------------------------------------------------

void SkinChangerV2Feature::OnConfigUpdateEvent(const ConfigUpdateEvent& Event)
{
	// Force an update
	SetFromConfig(true);
}

//---------------------------------------------------------------------------

void SkinChangerV2Feature::OnReceivedRemoteAssetContent(const ReceivedRemoteAssetContentEvent& Event)
{
	if (Event.Nonce != FEATURE_SKIN_CHANGER_NONCE)
		return;

	if (Event.Content.empty())
	{
		PLOG(error, "[SkinChangerV2Feature] Empty content buffer returned from remote service");
		return;
	}

	LoadDefaultSkinDefinitions();

	LoadCustomSkinDefinitions();

	// We received skin asset data, lets set from the config
	SetFromConfig(true);

	// Free asset buffer since we've now loaded it
	DefaultSkinAssetDataBuffer.clear();
}

//---------------------------------------------------------------------------

bool SkinChangerV2Feature::LoadDefaultSkinDefinitions()
{
	if (DefaultSkinAssetDataBuffer.empty())
	{
		PLOG(error, "[SkinChangerV2Feature] Tried to load default skins without default skin asset loaded");

		return false;
	}

	nlohmann::json Parsed = nlohmann::json::parse(DefaultSkinAssetDataBuffer);

	uint32_t LoadCount = 0;

	for (auto& Entry : Parsed)
	{
		SkinChangerWeaponSkinEntry* Skin = new SkinChangerWeaponSkinEntry;

		auto TypeStr = xorstr_("type");

		ESkinChangerWieldableType Type = (ESkinChangerWieldableType)Entry[TypeStr].get<int>();

		auto MeshesStr = xorstr_("meshes");

		for (auto& MeshPathEntry : Entry[MeshesStr])
		{
			auto Path = MeshPathEntry.get<std::string>();
			auto PathW = std::wstring(Path.begin(), Path.end());
			Skin->MeshPaths.push_back(PathW);
		}

		auto IconStr = xorstr_("icon");
		auto NameStr = xorstr_("name");
		auto MaterialsStr = xorstr_("materials");
		auto PathStr = xorstr_("path");
		auto IndexStr = xorstr_("index");

		std::string IconPath = Entry[IconStr].get<std::string>();

		Skin->IconPath = std::wstring(IconPath.begin(), IconPath.end());
		Skin->Name = Entry[NameStr].get<std::string>();

		// Create a hash ID for matching in configs
		uint64_t Hash = XXHash64::hash(Skin->Name.data(), Skin->Name.size(), 0);
		Skin->Id = Hash;

		for (auto& MatEntry : Entry[MaterialsStr])
		{
			SkinChangerMaterialEntry Material;

			std::string MatPath = MatEntry[PathStr].get<std::string>();

			Material.Path = std::wstring(MatPath.begin(), MatPath.end());
			Material.Index = MatEntry[IndexStr].get<int>();

			Skin->Materials.push_back(Material);
		}

		// If skin has materials, mark it as such
		if (!Skin->Materials.empty())
		{
			Skin->bHasMaterials = true;
		}

		if (!SkinsCache.contains(Type))
		{
			SkinsCache.insert({ Type, {} });
		}

		SkinsCache[Type].insert({ Hash, Skin });
		LoadCount++;
	}

	PLOGF(debug, "[SkinChangerV2Feature] Loaded {0} skin assets", LoadCount);

#if FEATURE_SKIN_CHANGER_IMMEDIATE_ICON_LOAD == 1
	// Load skins immediately on construction.
	LoadIcons(ESkinChangerWieldableType::EyeOfReach);
	LoadIcons(ESkinChangerWieldableType::Blunderbuss);
	LoadIcons(ESkinChangerWieldableType::Pistol);
	LoadIcons(ESkinChangerWieldableType::DoubleBarrel);
	LoadIcons(ESkinChangerWieldableType::Sword);
	//LoadIcons(ESkinChangerWieldableType::ThrowingKnives); // Not supported yet so we won't load them
#endif

	return true;
}

//---------------------------------------------------------------------------

bool SkinChangerV2Feature::LoadCustomSkinDefinitions()
{
	// Erase any existing custom skin definitions before we load
	for (auto CategoryIt = SkinsCache.begin(); CategoryIt != SkinsCache.end();)
	{
		auto& SkinCategoryEntry = *CategoryIt;

		for (auto It = SkinCategoryEntry.second.begin(); It != SkinCategoryEntry.second.end();)
		{
			auto& Skin = *It;

			if (Skin.second->bIsCustom)
			{
				// Free/delete the skin
				delete Skin.second;

				It = SkinCategoryEntry.second.erase(It);
				continue;
			}

			It++;
		}

		CategoryIt++;
	}

	for (auto& Skin : AppliedSkins)
	{
		if (Skin.second && Skin.second->bIsCustom)
		{
			// Set as null since we're removing all custom skins
			Skin.second = nullptr;
		}
	}

	std::filesystem::path UserSkinsPath = ConfigService::Get().GetWorkingDirectory() / FEATURE_SKIN_CHANGER_CUSTOM_SKINS_FILE;
	
	// Check if the custom skins folder exists
	if (!std::filesystem::exists(UserSkinsPath))
		return false;

	std::ifstream File;
	File.open(UserSkinsPath);
	

	if (!File.is_open())
		return false;

	std::string Content;
	std::string Line;

	while (std::getline(File, Line))
	{
		Content.append(Line);
	}

	File.close();

	if (Content.empty())
		return false;

	uint32_t LoadCount = 0;


	nlohmann::json Parsed = nlohmann::json::parse(Content, nullptr, false, true);

	if (Parsed.is_discarded())
		return false;

	if (!Parsed.is_array())
		return false;

	for (auto& Entry : Parsed)
	{
		auto TypeObj = Entry.find(xorstr_("type"));
		if (TypeObj == Entry.end())
			continue;

		ESkinChangerWieldableType Type = (ESkinChangerWieldableType)TypeObj.value().get<int>();

		auto MeshObj = Entry.find(xorstr_("mesh"));
		if (MeshObj == Entry.end())
			continue;

		auto MeshRaw = MeshObj.value().get<std::string>();
		auto MeshPath = std::wstring(MeshRaw.begin(), MeshRaw.end());

		auto NameObj = Entry.find(xorstr_("name"));
		if (NameObj == Entry.end())
			continue;

		auto Name = NameObj.value().get<std::string>();
		auto Id = XXHash64::hash(Name.data(), Name.size(), 0);

		if (Name.empty())
			continue;


		auto MaterialObj = Entry.find(xorstr_("material"));
		if (MaterialObj == Entry.end())
			continue;

		std::vector<SkinChangerMaterialEntry> Materials;
		if (!MaterialObj.value().is_null())
		{
			auto MaterialPathRaw = MaterialObj.value().get<std::string>();
			auto MaterialPath = std::wstring(MaterialPathRaw.begin(), MaterialPathRaw.end());
			if (!MaterialPathRaw.empty())
			{
				SkinChangerMaterialEntry Material;
				Material.Index = 0;
				Material.Path = MaterialPath;
				Materials.push_back(Material);
			}
		}

		SkinChangerWeaponSkinEntry* Skin = new SkinChangerWeaponSkinEntry;

		Skin->Name = Name;
		Skin->Id = Id;
		Skin->MeshPaths.push_back(MeshPath);
		Skin->Materials = Materials;

		Skin->bIsCustom = true;
		Skin->bHasMaterials = Materials.size() > 0;

		SkinsCache[Type].insert({ Id, Skin });
		LoadCount++;
	}

	PLOGF(debug, "[SkinChangerV2Feature] Loaded {0} custom skin assets", LoadCount);

	return true;
}

//---------------------------------------------------------------------------

bool SkinChangerV2Feature::FindAndExportDefaultSkinDefinitions()
{
	nlohmann::json Output;

	PLOG(debug, "[SkinChangerV2Feature] Beginning to export default skin assets");

	int32_t RemoveCount = 0;
	int32_t SkinLoadCount = 0;

    auto Library = EngineService::Get().FindAndLoadAssets("/Game/Blueprints/GameObjects/Props/Weapons", true, false);
	int32_t LoadCount = Library.Count;

	for (UObject* Object : Library.Objects)
	{
		UClass* Class = (UClass*)Object;
		UObject* DefaultObject = Class->CreateDefaultObject();

		if (DefaultObject->IsA(UItemDesc::StaticClass()))
		{
			UItemDesc* Desc = (UItemDesc*)DefaultObject;
			if (!Desc->InstanceClassType.AssetLongPathname.IsValid() || !Desc->Title.String)
				continue;

			std::string Name = Desc->Title.String->ToString();
			UClass* InfoClass = (UClass*)EngineService::Get().StaticLoadObject(UClass::StaticClass(), Desc->InstanceClassType.AssetLongPathname.wc_str(), true, true, GetWorld(), 0);

			if (!InfoClass) continue;

			AItemInfo* Info = (AItemInfo*)InfoClass->CreateDefaultObject();

			if (!Info) continue;

			AWieldableItem* Wieldable = (AWieldableItem*)Info->WieldableType->CreateDefaultObject();

			if (!Wieldable || !Desc->IconInvPath.AssetLongPathname.IsValid())
				continue;

			ESkinChangerWieldableType Type = ESkinChangerWieldableType::Invalid;

			if (Wieldable->IsA(AProjectileWeapon::StaticClass()))
			{
				AProjectileWeapon* Projectile = (AProjectileWeapon*)Wieldable;

				if (Projectile->ProjectileWeaponType == EProjectileWeaponType::Pistol)
					Type = ESkinChangerWieldableType::Pistol;
				else if (Projectile->ProjectileWeaponType == EProjectileWeaponType::Blunderbuss)
					Type = ESkinChangerWieldableType::Blunderbuss;
				else if (Projectile->ProjectileWeaponType == EProjectileWeaponType::EyeOfReach)
					Type = ESkinChangerWieldableType::EyeOfReach;
				else if (Projectile->ProjectileWeaponType == EProjectileWeaponType::DoubleBarrel)
					Type = ESkinChangerWieldableType::DoubleBarrel;
				else
					continue;
			}
			else if (Wieldable->IsA(AThrowableMeleeWeapon::StaticClass()))
			{
				//Type = ESkinChangerWieldableType::ThrowingKnives;
				continue;
			}
			else if (Wieldable->IsA(AMeleeWeapon::StaticClass()))
			{
				Type = ESkinChangerWieldableType::Sword;
			}
			else
			{
				continue;
			}

			if (!Wieldable->DetailAppearence.Mesh.AssetLongPathname.IsValid())
				continue;


			nlohmann::json EntryObject;

			std::vector<std::string> MeshPaths;

			MeshPaths.push_back(Wieldable->DetailAppearence.Mesh.AssetLongPathname.ToString());

			EntryObject["type"] = Type;
			EntryObject["name"] = Name;
			EntryObject["meshes"] = MeshPaths;
			EntryObject["icon"] = Desc->IconInvPath.AssetLongPathname.ToString();
			

			std::vector< nlohmann::json> Materials;

			if (Wieldable->DetailAppearence.Materials.IsValid())
			{
				for (int i = 0; i < Wieldable->DetailAppearence.Materials.Num(); i++)
				{
					const auto& Material = Wieldable->DetailAppearence.Materials[i];

					if (!Material.Material.AssetLongPathname.IsValid())
						continue;

					nlohmann::json MaterialObject;
					MaterialObject["index"] = Material.MaterialIndex;
					MaterialObject["path"] = Material.Material.AssetLongPathname.ToString();
					Materials.push_back(MaterialObject);
				}
			}

			EntryObject["materials"] = Materials;

			Output.push_back(EntryObject);
			SkinLoadCount++;
		}

	}

	PLOGF(debug, "[SkinChangerV2Feature] Exporting {0} skin assets", SkinLoadCount);

	
	std::filesystem::path PathInc = ConfigService::Get().GetWorkingDirectory() / xorstr_("SkinsExport.json");

	std::ofstream File;
	File.open(PathInc);
	File << Output.dump() << std::endl;
	File.close();

	return true;
}

//---------------------------------------------------------------------------

uint64_t* SkinChangerV2Feature::GetSkinIdConfigPtr(ESkinChangerWieldableType InType)
{
	auto& Config = GetCfg();

	// No match for invalid weapon
	if (InType == ESkinChangerWieldableType::Invalid)
		return nullptr;

	switch (InType)
	{
	case ESkinChangerWieldableType::Pistol:
		return Config.Other.Skins.Pistol->GetPtr();
	case ESkinChangerWieldableType::Blunderbuss:
		return Config.Other.Skins.Blunderbuss->GetPtr();
	case ESkinChangerWieldableType::EyeOfReach:
		return Config.Other.Skins.EyeOfReach->GetPtr();
	//case ESkinChangerWieldableType::ThrowingKnives:
		//return Config.Other.Skins.ThrowingKnives->GetPtr();
	case ESkinChangerWieldableType::DoubleBarrel:
		return Config.Other.Skins.DoubleBarrel->GetPtr();
	case ESkinChangerWieldableType::Sword:
		return Config.Other.Skins.Sword->GetPtr();
	}
}

//---------------------------------------------------------------------------

bool SkinChangerV2Feature::SetFromConfig(bool bForceUpdate)
{

	if (AppliedSkins.empty())
		return false;

	for (auto& Skin : AppliedSkins)
	{
		// Set as null first up
		Skin.second = nullptr;

		auto SkinType = Skin.first;
		auto SkinValuePtr = GetSkinIdConfigPtr(Skin.first);

		auto& TypeSkins = SkinsCache[Skin.first];

		if (TypeSkins.empty())
			continue;

		auto Found = TypeSkins.find(*SkinValuePtr);
		if (Found != TypeSkins.end())
		{
			// the skin exists, we'll apply it now
			SetSkin(SkinType, Found->second);
		}
		else
		{
			SetSkin(SkinType, nullptr);
		}
	}

	// Force an update if we requested one
	if (bForceUpdate)
	{
		ForceSkinApplyUpdate();
	}

	return true;
}

//---------------------------------------------------------------------------

bool SkinChangerV2Feature::GetSkinsDataFromEndpoint()
{
	
	HttpRequest Req;
	Req.Uri = xorstr_("api/client/skins");

    // Get the Aufority token so we can authenticate with prismarine API.
    // This will not work in development builds as a token is required for client endpoints.
    // We can generate a dev token via the Aufority discord bot: /token create
    // Just make sure to set a user claim, as it's required.
	// TODO: We should probably receive a token via an event.
	// Should not be accessing the domain directly from here.
	auto& Token = MainDomain::Get().GetShulkerbox()->GetAuforityToken();
	//auto& SessionId = MainDomain::Get().GetShulkerbox()->GetSessionId(); Not needed anymore since moving to aufority auth

	if (!Token.empty())
	{
		Req.Headers = { 
			{ xorstr_("Authorization"), Token }
		};
	}

	Req.Method = EHttpRequestMethod::Get;

	ServiceHttpClient->SendRequestAsync(Req, 
		[this](const HttpResponse& Response) {

			if (Response.IsSuccessful() && Response.GetStatusCode() == EHttpStatusCode::OK)
			{
				DefaultSkinAssetDataBuffer = Response.Body;

                // Save time, just call the original event handler lol
				ReceivedRemoteAssetContentEvent FakeEv(
                    FEATURE_SKIN_CHANGER_NONCE, 
                    DefaultSkinAssetDataBuffer
                );

				OnReceivedRemoteAssetContent(FakeEv);

				DefaultSkinAssetDataBuffer.clear();
			}
		}
	);
}


//---------------------------------------------------------------------------
