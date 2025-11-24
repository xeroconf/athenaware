#include "PlayerListFeature.h"

//---------------------------------------------------------------------------

PlayerListFeature::PlayerListFeature(scaffold::Messenger* InMessenger, HttpClient* InPrismarineServiceHttpClient) : 
  scaffold::Feature(xorstr_("Player List")), 
  Dispatcher(InMessenger),
  HttpClient(InPrismarineServiceHttpClient)
{
};

//---------------------------------------------------------------------------

bool PlayerListFeature::OnStart()
{
	MenuFeatureInstance = MainDomain::Get().GetFeatureManager()->GetFeature(xorstr_("F_INTERFACE_MENU_MAIN"));
	if (!MenuFeatureInstance)
	{
		return false;
	}

	if (!Dispatcher->Subscribe<RemoteReceiveResolvedXuidsEvent>(ReceivedResolvedGamertagsHandle, [this](const RemoteReceiveResolvedXuidsEvent& Event) { OnReceiveResolvedGamertags(Event); }))
	{
		return false;
	}

	if (!Dispatcher->Subscribe<ActiveInGameWorldEvent>(ActiveInGameWorldHandle, [this](const ActiveInGameWorldEvent& Event) { OnActiveInGameWorld(Event); }))
	{
		return false;
	}

	if (!Dispatcher->Subscribe<XboxAuthTokenEvent>(ReceiveXboxTokenHandle, [this](const XboxAuthTokenEvent& Event) { OnRecieveXboxToken(Event); }))
	{
		return false;
	}

	if (!Dispatcher->Subscribe<ConfigUpdateEvent>(ConfigLoadedHandle, [this](const ConfigUpdateEvent& Event) { OnConfigLoaded(Event); }))
	{
		return false;
	}

	if (!Dispatcher->Subscribe<SessionStateChangeEvent>(SessionUpdateHandle, [this](const SessionStateChangeEvent& Event) { OnSessionUpdate(Event); }))
	{
		return false;
	}

	if (!Dispatcher->Subscribe<ActorAggregationUpdateEvent>(ActorAggregationHandle, [this](const ActorAggregationUpdateEvent& Event) { OnActorAggregationUpdate(Event); }))
	{
		return false;
	}

	UPlayerActivityIconCatalogueDataAsset* ActivityData = (UPlayerActivityIconCatalogueDataAsset*)EngineService::Get().StaticLoadObject(UPlayerActivityIconCatalogueDataAsset::StaticClass(), xorstr_(L"/Game/DataAssets/PlayerActivity/activityicondataasset.ActivityIconDataAsset"), true, true);
	
	uint32_t TextureIconLoadFailedCount = 0;
	if (ActivityData)
	{
		for (int i = 0; i < ActivityData->PlayerActivityIcons.Num(); i++)
		{
			auto& Icon = ActivityData->PlayerActivityIcons[i];
			UTexture2D* Texture = (UTexture2D*)EngineService::Get().StaticLoadObject(UTexture2D::StaticClass(), Icon.ActivityIcon.AssetLongPathname.wc_str(), true, true);

			if (Texture)
			{
				ActivityIcons.insert({ (uint8_t)Icon.ActivityType, Texture });
			}
			else
			{
				TextureIconLoadFailedCount++;
				PLOGF(warn, "[PlayerListFeature] Failed to load icon texture for activity type {0}", (uint32_t)Icon.ActivityType);
			}
		}
	}
	else
	{
		PLOG(warn, "[PlayerListFeature] Failed to load UPlayerActivityIconCatalogueDataAsset");
	}
	
	// Any failure will mark the icons as not loaded. 
	// If there were no errors, then we'll mark as loaded.
    // We'll use this to determine if we need to render icons in the UI.
	if (TextureIconLoadFailedCount == 0)
	{
		bIconsLoaded = true;
	}

	// Update server preference on feature initialization.
	// We also update this when we update the saved config
	UpdateServerWithSelfMarkPreference();

	// Moved to OnConditionChange
	// Read the UserMarks.txt file into the array
	// ReadMarkConfiguration(); 

	return true;
}

//---------------------------------------------------------------------------

bool PlayerListFeature::OnEnd()
{
	Dispatcher->Unsubscribe(ReceivedResolvedGamertagsHandle);
	Dispatcher->Unsubscribe(ActiveInGameWorldHandle);
	Dispatcher->Unsubscribe(ReceiveXboxTokenHandle);
	Dispatcher->Unsubscribe(ConfigLoadedHandle);
	Dispatcher->Unsubscribe(SessionUpdateHandle);
	Dispatcher->Unsubscribe(ActorAggregationHandle);

	ActivityIcons.clear();

	return true;
}

//---------------------------------------------------------------------------

void PlayerListFeature::OnReset(EFeatureResetReason InResetReason)
{
	PlayerMappings.clear();
	PreSessionQueue.clear();
}

//---------------------------------------------------------------------------

void PlayerListFeature::OnDiscard(float DeltaTime)
{

}

//---------------------------------------------------------------------------

bool PlayerListFeature::CanExecute()
{
	return GetCfg().Visuals.HUD.PlayerList.Enabled->Get() && GetPlayer();
}

//---------------------------------------------------------------------------

void PlayerListFeature::OnConditionChange(bool bNewCondition)
{
	if (bNewCondition)
	{
		// Allow support for reloading the custom mark list by refreshing the list
		// when we toggle the player list feature.
		ReadMarkConfiguration();
	}
}

//---------------------------------------------------------------------------

void PlayerListFeature::PostTick(float DeltaTime)
{

}

//---------------------------------------------------------------------------

void PlayerListFeature::Tick(float DeltaTime)
{
	// Only process the queue(s) every 2500 milliseconds
	if (Timestamp::GetTimeSince(LastQueueResolve) > 2500)
	{
		// Process the replicaiton queue. We need to wait until the XUID is replicated over the network as
		// it's not always availabe at the time of actor aggregation - which is where we acquire the state from...
		for (auto It = PlayerStateAwaitReplicateQueue.begin(); It != PlayerStateAwaitReplicateQueue.end();)
		{
			auto PlayerState = *It;

			// ZC: Acquire the XUID from the playerstate. This is not in the SDK, so we'll
			// need to ensure the offset is correct. There is some logic to this, so I've moved it all
			// into AAthenaPlayerState::GetXUID so we can use in other features as well.
            // If the offset is invalid, GetXUID will return nil.
			const auto kUniqueId = PlayerState->GetXUID();

			// This is our own player state, so ignore it.
			if (GetController() && GetController()->PlayerState == PlayerState)
			{
				It = PlayerStateAwaitReplicateQueue.erase(It);
				return;
			}

			if (kUniqueId)
			{
				PreSessionQueue.insert(kUniqueId);
				It = PlayerStateAwaitReplicateQueue.erase(It);
			}
			else
			{
				++It;
			}
		}


		if (!PreSessionQueue.empty())
		{
			// Send queued XUIDs to resolve
			SendResolveRequest(PreSessionQueue);
			PreSessionQueue.clear();
		}

		LastQueueResolve = Timestamp::Now();
	}

	
}

//---------------------------------------------------------------------------

void PlayerListFeature::OnExecute(float DeltaTime)
{
	auto& Config = GetCfg();

	if (InputService::Get().WasKeyPressed(Config.Visuals.HUD.Keybinds.PlayerListMode->Get(), true))
	{
		if (ListMode == EPlayerListMode::Crews)
			ListMode = EPlayerListMode::Players;
		else
			ListMode = EPlayerListMode::Crews;
	}

	if (bWindowPositionRequiresUpdate)
	{
		const auto kNewPos = reinterpret_cast<ImVec2&>(Config.Visuals.HUD.PlayerList.Location->Get());
		ImGui::SetNextWindowPos(kNewPos);
		bWindowPositionRequiresUpdate = false;
	}

	// UI CODE REMOVED
}

//---------------------------------------------------------------------------

void PlayerListFeature::DrawPlayerList()
{
	// UI CODE REMOVED
}

//---------------------------------------------------------------------------

void PlayerListFeature::DrawCrewList()
{
	// UI CODE REMOVED
}

//---------------------------------------------------------------------------

void PlayerListFeature::DrawNoSessionList()
{
	// UI CODE REMOVED
}

//---------------------------------------------------------------------------

void PlayerListFeature::SendResolveRequest(std::unordered_set<uint64_t>& InXuidList)
{
	if (XboxAuthToken.empty())
	{
		PLOGF(warn, "[PlayerListFeature] Mark check not using user token for checking {0:d} users due to invalid ticket", InXuidList.size());
	}

	PLOGF(debug, "[PlayerListFeature] Issuing mark check on {0:d} users", InXuidList.size());

	/// TODO: Switch to prismarine service for checking marks as it supports caching.
	NetworkService::Get().ServerResolveXuids(InXuidList, XboxAuthToken);
}

//---------------------------------------------------------------------------

void PlayerListFeature::OnReceiveResolvedGamertags(const RemoteReceiveResolvedXuidsEvent& Event)
{
	PTRACE_DEBUG();

	uint32_t MarkedUsersDiscovered = 0;
	uint32_t MarkedUsersWithInfo = 0;
	uint32_t MarkedUsersToNotifyFor = 0;
	uint32_t NamesAssignedToStates = 0;
	
	for (auto& Entry : Event.Entries)
	{
		auto PlayerState = GetPlayerStateFromXuid(Entry.Xuid);

		// Check if the player state still exists. If not, this player probably left
		// the session during the resolve request. We'll just ignore this player.
		if (!PlayerState)
		{
			continue;
		}

		EMarkType MarkType = (EMarkType)Entry.MarkType;

		// Check if this user is in our custom mark list and if so we'll
		// override their mark type with the custom one
		if (CustomMarkList.size() > 0)
		{
			auto Found = CustomMarkList.find(Entry.Gamertag);
			if (Found != CustomMarkList.end())
			{
				MarkType = EMarkType::Custom;
			}
		}

		const auto [TypeEnabled, Color] = GetMarkTypeConfigOptions(MarkType);

		if (Entry.MarkType > 0)
		{
			MarkedUsersDiscovered++;
		}

		if (TypeEnabled)
		{
			MarkedUsersToNotifyFor++;
		}

		PlayerListEntry ListEntry
		(
			Entry.Gamertag,
			MarkType,
			Entry.MarkInfo
		);


		if (Entry.Gamertag.size() > 0)
			ListEntry.GamertagWide = std::wstring(Entry.Gamertag.begin(), Entry.Gamertag.end());

		PlayerMappings[Entry.Xuid] = ListEntry;

		// Assign a name to the player if it doesn't have one already
		if (PlayerState->PlayerName.Num() == 0)
		{
			// Pretend this name was sent from the server.
			GetController()->SendPlayerName(PlayerState->PlayerId, FString(ListEntry.GamertagWide));
			NamesAssignedToStates++;
		}
	}

	PLOGF(debug, "[PlayerListFeature] Acquired {0:d} and assigned {1:d}", Event.Entries.size(), NamesAssignedToStates); 

	// Raise a message to the user when a mark is discovered during gameplay
	if (MarkedUsersDiscovered > 0 && GetCfg().Visuals.HUD.PlayerList.Enabled->Get())
	{
		// Only fire the notification for marked users that we have enabled
		if (MarkedUsersToNotifyFor > 0)
		{
			std::string Contents = std::vformat(xorstr_("Encountered {} marked user(s) in this session."), std::make_format_args(MarkedUsersDiscovered));

			// Notify the user what to press if they're not viewing the mark list.
			if (ListMode == EPlayerListMode::Crews && GetCfg().Visuals.HUD.PlayerList.Enabled->Get() && GetCfg().Visuals.HUD.Keybinds.PlayerListMode->Get() != ImGuiKey_None)
			{
				const char* Name = ImGui::GetKeyName((ImGuiKey)GetCfg().Visuals.HUD.Keybinds.PlayerListMode->Get());
				Contents.append(std::vformat(xorstr_(" Press {} to view the marked user list."), std::make_format_args(Name)));
			}

			NotificationEvent NotifyEvent
			(
				8000,
				this-GetName(),
				Contents,
				"\xEE\x87\x8F",
				ImColor(0, 255, 255, 255),
				NotificationEvent::ESound::Celebrate
			);

			Dispatcher->Emit(NotifyEvent);
		}

		PLOGF(debug, "[PlayerListFeature] Encountered {0:d} marked users in the current session", MarkedUsersDiscovered);
	}

	// Primarily for auto-hop feature
	EncounteredMarkedUserEvent EncounterEvent
	(
		MarkedUsersDiscovered, 
		MarkedUsersToNotifyFor
	);

	Dispatcher->Emit(EncounterEvent);

	PTRACE_DEBUG();
}

//---------------------------------------------------------------------------

void PlayerListFeature::OnActiveInGameWorld(const ActiveInGameWorldEvent& Event)
{

}

//---------------------------------------------------------------------------

void PlayerListFeature::OnRecieveXboxToken(const XboxAuthTokenEvent& Event)
{
	if (!Event.IsTitleToken)
		return;

	// Is this token new?
	const bool bIsNew = (XboxAuthToken.empty() == true);

	if (!bIsNew)
		return;

	PLOG(debug, "[PlayerListFeature] Received ticket for xuid resolution");

	// Set the token we received
	XboxAuthToken = Event.Token;
}

//---------------------------------------------------------------------------

void PlayerListFeature::OnConfigLoaded(const ConfigUpdateEvent& Event)
{
	// Inform the UI that the position may have changed due to a config change.
	bWindowPositionRequiresUpdate = true;

	// Update the server with our mark preference when we load a config
	UpdateServerWithSelfMarkPreference();
}

//---------------------------------------------------------------------------

void PlayerListFeature::OnSessionUpdate(const SessionStateChangeEvent& Ev)
{
	// I believe this has been fixed. No need to do this anymore since we're
	// gathering XUIDs straight from the game.
	/*
	// Player 
	if (!(Ev.State == SessionStateChangeEvent::EState::Ended || Ev.State == SessionStateChangeEvent::EState::Update))
		return;

	// Fix for player-list saving players during server change.
	// Manually remove any lingering player entries if they don't
	// have a respective APlayerState to back them.
	for (auto It = PlayerMappings.begin(); It != PlayerMappings.end();)
	{
		auto PlayerState = GetPlayerStateFromXuid(It->first);

		if (!PlayerState)
		{
			It = PlayerMappings.erase(It);
		}
		else
		{
			++It;
		}
	}

	// We've switching session, clear the replication queue.
	PlayerStateAwaitReplicateQueue.clear();
	*/
}

//---------------------------------------------------------------------------

void PlayerListFeature::OnActorAggregationUpdate(const ActorAggregationUpdateEvent& Ev)
{
	if (Ev.AssignedClass != AAthenaPlayerState::StaticClass())
		return;

	AAthenaPlayerState* State = (AAthenaPlayerState*)Ev.Actor;

	const bool bHasUniqueId = State->GetXUID() ? true : false;


	if (Ev.UpdateType == ActorAggregationUpdateEvent::EType::Added)
	{
		PlayerStateAwaitReplicateQueue.insert(State);
	}
	else
	{
		// This should never happen. Covering this edge case.
		if (!bHasUniqueId)
		{
			PlayerStateAwaitReplicateQueue.erase(State);

            
#ifdef PRISMARINE_USE_FEATURE_ERROR_TELEMETRY
            // Tracking this error
            FeatureErrorEvent ErrorEv
            (
                GetId(),
                xorstr_("PlayerListFeature::OnActorAggregationUpdate invalid user XUID")
            );

            Dispatcher->Emit(ErrorEv);
#endif // PRISMARINE_USE_FEATURE_ERROR_TELEMETRY

			return;
		}

		uint64_t Xuid = State->GetXUID();

		PLOGF(debug, "[PlayerListFeature] Deregistering '{0}' from mark list", Xuid);
		
		PlayerMappings.erase(Xuid);
		PreSessionQueue.erase(Xuid);
		PlayerStateAwaitReplicateQueue.erase(State);
	}
}

//---------------------------------------------------------------------------

std::pair<bool, uint32_t> PlayerListFeature::GetMarkTypeConfigOptions(EMarkType InType)
{
	auto& Config = GetCfg();

	switch (InType)
	{
	case EMarkType::Blacklist:
		return { Config.Visuals.HUD.PlayerList.AlertBlacklist->Get(), Config.Visuals.HUD.Colors.BlacklistMark->Get() };
	case EMarkType::Streamer:
		return { Config.Visuals.HUD.PlayerList.AlertStreamer->Get(), Config.Visuals.HUD.Colors.StreamerMark->Get() };
	case EMarkType::Cheater:
		return { Config.Visuals.HUD.PlayerList.AlertCheater->Get(), Config.Visuals.HUD.Colors.CheaterMark->Get() };
	case EMarkType::Female:
		return { Config.Visuals.HUD.PlayerList.AlertFemale->Get(), Config.Visuals.HUD.Colors.FemaleMark->Get() };
	case EMarkType::RareAssociate:
		return { Config.Visuals.HUD.PlayerList.AlertRareAssociate->Get(), Config.Visuals.HUD.Colors.RareAssociateMark->Get() };
	case EMarkType::YouTuber:
		return { Config.Visuals.HUD.PlayerList.AlertYouTuber->Get(), Config.Visuals.HUD.Colors.YouTuberMark->Get() };
	case EMarkType::AthenawareUser:
		return { Config.Visuals.HUD.PlayerList.AlertAthenawareUser->Get(), Config.Visuals.HUD.Colors.AthenawareUserMark->Get() };
	case EMarkType::NewAccount:
		return { Config.Visuals.HUD.PlayerList.AlertNewAccount->Get(), Config.Visuals.HUD.Colors.NewAccountMark->Get() };
	case EMarkType::Custom:
		return { Config.Visuals.HUD.PlayerList.AlertCustom->Get(), Config.Visuals.HUD.Colors.CustomMark->Get() };
	default:
		return { false , ImColor(255, 255, 255, 255) };
	}
}

//---------------------------------------------------------------------------

AAthenaPlayerState* PlayerListFeature::GetPlayerStateFromXuid(uint64_t InXuid)
{
	if (InXuid == 0)
		return nullptr;

	AAthenaGameState* GameState = (AAthenaGameState*)GetWorld()->GameState;

	if (!GameState)
		return nullptr;

	ACrewService* CrewService = GameState->CrewService;

	if (!CrewService)
		return nullptr;


	AAthenaPlayerState* Result = nullptr;
	for (auto& PlayerState : GameState->PlayerArray)
	{
		if (PlayerState && PlayerState->GetXUID())
		{
			if (PlayerState->GetXUID() == InXuid)
			{
				Result = (AAthenaPlayerState*)PlayerState;
				break;
			}
		}
	}

	return Result;
}

//---------------------------------------------------------------------------

void PlayerListFeature::UpdateServerWithSelfMarkPreference()
{
	const auto bIsSelfMarked = GetCfg().Visuals.HUD.PlayerList.SelfMark->Get();
	NetworkService::Get().ServerSelfMark(bIsSelfMarked);
}

//---------------------------------------------------------------------------

uint32_t PlayerListFeature::RelevantMarksCount()
{
	// Relevant marks are marked users that have their notification toggle ENABLED.
	uint32_t ReturnAmount = 0;

	for (auto& Mapping : PlayerMappings)
	{
		const auto [Enabled, Color] = GetMarkTypeConfigOptions(Mapping.second.MarkType);
		if (Enabled)
		{
			ReturnAmount++;
		}
	}

	return ReturnAmount;
}

//---------------------------------------------------------------------------

bool PlayerListFeature::ReadMarkConfiguration()
{
	// Clear list
	CustomMarkList.clear();

	std::filesystem::path UserMarksPath = ConfigService::Get().GetWorkingDirectory() / xorstr_("UserCustomMarks.txt");

	// Create file if it doesn't exist
	if (!std::filesystem::exists(UserMarksPath))
	{
		std::ofstream File;
		File.open(UserMarksPath);
		File.close();
	}

	// Check if the custom marks file exists and was created from the prior step. If not then break out.     
	if (!std::filesystem::exists(UserMarksPath))
	{
		return false;
	}

	std::ifstream File;

	File.open(UserMarksPath);

	if (!File.is_open())
	{
#ifdef PRISMARINE_USE_FEATURE_ERROR_TELEMETRY
        FeatureErrorEvent ErrorEv
        (
            GetId(),
            xorstr_("PlayerListFeature::ReadMarkConfiguration is_open() failed")
        );

        Dispatcher->Emit(ErrorEv);
#endif // PRISMARINE_USE_FEATURE_ERROR_TELEMETRY

		return false;
	}

	std::string Line;

	while (std::getline(File, Line))
	{
		CustomMarkList.insert(Line);
	}

	File.close();

	return CustomMarkList.size() > 0;
}

//---------------------------------------------------------------------------
