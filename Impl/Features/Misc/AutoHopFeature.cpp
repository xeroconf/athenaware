#include "AutoHopFeature.h"

//---------------------------------------------------------------------------

AutoHopFeature::AutoHopFeature(std::shared_ptr<scaffold::Messenger> InMessenger) : 
  scaffold::Feature("Auto Hop"), 
  Dispatcher(std::move(InMessenger)) 
{
};

//---------------------------------------------------------------------------

bool AutoHopFeature::OnStart()
{
    // TODO: Use event to communicate or something. This is bad practice. Feature should never know about its owner.
	PlayerListInstance = (PlayerListFeature*)MainDomain::Get().GetFeatureManager().GetFeature(xorstr_("F_INTERFACE_PLAYER_LIST"));
	if (!PlayerListInstance)
	{
		PLOG(error, "[AutoHopFeature] Failed to get an instance of PlayerListFeature");
		return false;
	}

    // TODO: Use event to communicate or something. This is bad practice. Feature should never know about its owner.
	DependencyLoaderInstance = (DependencyLoaderFeature*)MainDomain::Get().GetFeatureManager().GetFeature(xorstr_("F_GENERAL_DEPENDENCY_LOADER"));
	if (!DependencyLoaderInstance)
	{
		PLOG(error, "[AutoHopFeature] Failed to get an instance of DependencyLoaderFeature");
		return false;
	}

	if (!Dispatcher->Subscribe<EncounteredMarkedUserEvent>(MarkedUserEncounterHandle, [this](const EncounteredMarkedUserEvent& Event) { OnMarkedUsersEncountered(Event); }))
	{
		PLOG(error, "[AutoHopFeature] Failed to register event handler for MarkedUserEncounterHandle");
		return false;
	}

	// DependencyLoaderInstance

	if (!FortOfDamnedSignal)
	{
		FortOfDamnedSignal = (UClass*)DependencyLoaderInstance->GetDependency(xorstr_("BP_SkellyFort_RitualSkullCloud_C"));
		if (!FortOfDamnedSignal)
		{
			return false;
		}
	}
	
	if (!FortOfFortuneSignal)
	{
		FortOfFortuneSignal = (UClass*)DependencyLoaderInstance->GetDependency(xorstr_("BP_LegendSkellyFort_SkullCloud_C"));
		if (!FortOfFortuneSignal)
		{
			return false;
		}
	}

	CurrentState = EState::AwaitingSuitabilityCheck;

	return true;
}

//---------------------------------------------------------------------------

bool AutoHopFeature::OnEnd()
{
	Dispatcher->Unsubscribe(MarkedUserEncounterHandle);
	CurrentSessionFoundAutomatically = false;

	return true;
}

//---------------------------------------------------------------------------

void AutoHopFeature::OnReset(EFeatureResetReason InResetReason) 
{ 
	CurrentState = EState::AwaitingSuitabilityCheck;
}

//---------------------------------------------------------------------------

void AutoHopFeature::PostTick(float DeltaTime) {}

//---------------------------------------------------------------------------

void AutoHopFeature::Tick(float DeltaTime) {}

//---------------------------------------------------------------------------

bool AutoHopFeature::CanExecute()
{
	return GetController() && IsActive && IsInGameplay();
}

//---------------------------------------------------------------------------

void AutoHopFeature::OnConditionChange(bool bConditionChange) 
{
}

//---------------------------------------------------------------------------

void AutoHopFeature::OnExecute(float DeltaTime) 
{
	// We're active within the server, so we'll now check if it's
	// a suitable server or not. If so, we'll notify the user (and webhook if applicable).
	if (CurrentState == EState::AwaitingSuitabilityCheck)
	{
		if (!CurrentSessionFoundAutomatically && HopsCount > 0 && CheckIfSuitableServer(true))
		{
			// Reset feature and disable it
			IsActive = false;
			CurrentSessionFoundAutomatically = true;
			OnReset(EFeatureResetReason::Internal);
			HopsCount++;
		}
		else
		{
			HopsCount++;
			CurrentState = EState::TryingToScuttle;
		}
	}

	// We determined this server is not suitable, so we'll disregard it and try
	// scuttle the boat. We run this on a 2second timer to prevent RPC spam.
	else if (CurrentState == EState::TryingToScuttle)
	{
		if (Timestamp::GetTimeSince(LastScuttleAttempt) >= 2000)
		{
			if (GetController()->PlayerCrewComponent)
			{
				GetController()->PlayerCrewComponent->Server_VoteOnScuttleShipAndChangeSeas(true);
				LastScuttleAttempt = Timestamp::Now();
			}
		}
	}

	// There is no need to account for any other states as this feature
	// will reset itself when we go to find a new session, thus making it
	// go through the process again.
}

//---------------------------------------------------------------------------

void AutoHopFeature::OnDiscard(float DeltaTime) {}

//---------------------------------------------------------------------------

void AutoHopFeature::OnActiveInWorld(const ActiveInGameWorldEvent& Ev)
{
	// Not needed
}

//---------------------------------------------------------------------------

bool AutoHopFeature::CheckIfSuitableServer(bool bNotify)
{
	uint32_t ReasonsCount = 0;
	std::vector<std::string> Reasons;

	if (GetCfg().Other.Misc.AutoHop.FindMarkedUsers->Get())
	{
		auto MarkCount = PlayerListInstance->RelevantMarksCount();

		if (MarkCount > 0)
		{
			ReasonsCount++;


			std::string MarkUserStr = "";
			bool bIsFirst = true;
			for (auto& Entry : PlayerListInstance->GetPlayerMappings())
			{
				if (Entry.second.MarkType == EMarkType::None)
					continue;

				if (!bIsFirst)
					MarkUserStr.append(" ");

				MarkUserStr.append(Entry.second.Gamertag);

				bIsFirst = false;
			}

			std::string ReasonStr = std::vformat(xorstr_("- {} marked user(s) ({})"), std::make_format_args(MarkCount, MarkUserStr));

			Reasons.push_back(ReasonStr);
		}
	}

	auto& Events = ActorService::Get().GetActorsOfType<AGameplayEventSignal>();

	if (GetCfg().Other.Misc.AutoHop.FindFortOfTheDamned->Get())
	{
		
		for (auto Event : Events)
		{
			if (Event->IsA(FortOfDamnedSignal))
			{
				uint32_t DistanceMeters = (uint32_t)Event->K2_GetActorLocation().DistanceMeter(GetPlayer()->K2_GetActorLocation());

				ReasonsCount++;
				Reasons.push_back(std::vformat(xorstr_("- Fort of the Damned ({}m away)"), std::make_format_args(DistanceMeters)));
				break;
			}
		}
	}

	if (GetCfg().Other.Misc.AutoHop.FindFortOfFortune->Get())
	{
		for (auto Event : Events)
		{
			if (Event->IsA(FortOfFortuneSignal))
			{
				uint32_t DistanceMeters = (uint32_t)Event->K2_GetActorLocation().DistanceMeter(GetPlayer()->K2_GetActorLocation());

				ReasonsCount++;
				Reasons.push_back(std::vformat(xorstr_("- Fort of Fortune ({}m away)"), std::make_format_args(DistanceMeters)));
				break;
			}
		}
	}

	if (ReasonsCount > 0)
	{
		NotifyUser(Reasons);

		if (GetCfg().Other.Misc.AutoHop.NotifyViaWebhook->Get() && !NotifyWebhook(Reasons))
		{
			NotificationEvent NotifyEvent(
				6000,
				xorstr_("Auto Hop"),
				xorstr_("Failed to notify the webhook. Ensure the ID and token are correct."),
				ICON_FA_MAGNIFYING_GLASS,
				ImColor(255, 0, 0, 255),
				NotificationEvent::ESound::Alert
			);

			Dispatcher->Emit(NotifyEvent);
		}

		PLOGF(debug, "[AutoHopFeature] Suitable server detected due to {0} reasons", Reasons.size());

		return true;
	}
	else
	{
		PLOG(debug, "[AutoHopFeature] Server is not suitable, proceeding to scuttle.")

		return false;
	}
}

//---------------------------------------------------------------------------

void AutoHopFeature::OnMarkedUsersEncountered(const EncounteredMarkedUserEvent& Ev)
{
}

//---------------------------------------------------------------------------

bool AutoHopFeature::NotifyWebhook(std::vector<std::string> InReasons)
{
	if (InReasons.empty())
		return false;

	std::string& WebhookId = GetCfg().Other.Misc.AutoHop.WebhookId->Get();
	std::string& WebhookToken = GetCfg().Other.Misc.AutoHop.WebhookToken->Get();

	if (WebhookId.empty())
	{
		PLOG(warn, "[AutoHopFeature] Unable to notify webhook due to empty webhook ID");
		return false;
	}

	if (WebhookToken.empty())
	{
		PLOG(warn, "[AutoHopFeature] Unable to notify webhook due to empty webhook token");
		return false;
	}

	nlohmann::json Content;

	auto& WebhookMsg = GetCfg().Other.Misc.AutoHop.WebhookMessage->Get();

	if (!WebhookMsg.empty())
	{
		Content[xorstr_("content")] = WebhookMsg;
	}

	Content[xorstr_("embeds")] = nlohmann::json::array();
	Content[xorstr_("attachments")] = nlohmann::json::array();

	nlohmann::json Embed;

	std::string DescString(xorstr_("# Server Found\n\n## Reason(s)\n"));

	for (auto& Reason : InReasons)
	{
		DescString.append(Reason).append("\n");
	}

	Embed[xorstr_("description")] = DescString;
	Embed[xorstr_("footer")][xorstr_("text")] = xorstr_("Automatic message sent by the Athenaware auto-hopper");

	std::vector< nlohmann::json> Embeds = { Embed };

	Content[xorstr_("embeds")] = Embeds;

	std::string ReqContent = Content.dump();

	HttpRequest Req;
	Req.Body = std::vector<uint8_t>(ReqContent.begin(), ReqContent.end());
	Req.Url = xorstr_("https://discord.com/") + std::vformat(xorstr_("/api/webhooks/{}/{}"), std::make_format_args(WebhookId, WebhookToken));
	Req.Method = EHttpRequestMethod::Post;
	Req.Headers = { {xorstr_("Content-Type"), xorstr_("application/json")} };
	
	HttpResponse Res;
	if (!HttpService::Get().SendRequest(Req, &Res))
	{
		PLOG(error, "[AutoHopFeature] Webhook request failed");
		return false;
	}

	if (Res.StatusCode != EHttpStatusCode::NoContent)
	{
		PLOGF(error, "[AutoHopFeature] Webhook request failed and did not return 204 instead returned {0}", (int)Res.StatusCode);
		return false;
	}


	return true;
}

//---------------------------------------------------------------------------

bool AutoHopFeature::NotifyUser(std::vector<std::string> InReasons)
{
	NotificationEvent NotifyEvent(
		6000,
		xorstr_("Auto Hop"),
		std::string(xorstr_("You found a suitable session! Stopping the auto-hop.")),
		ICON_FA_MAGNIFYING_GLASS,
		ImColor(0, 255, 255, 255),
		NotificationEvent::ESound::Celebrate
	);

	Dispatcher->Emit(NotifyEvent);

	return true;
}

//---------------------------------------------------------------------------