#include "YellowbeardUserFeature.h"

//---------------------------------------------------------------------------

YellowbeardUserFeature::YellowbeardUserFeature(std::shared_ptr<scaffold::Messenger> InMessenger) : 
  scaffold::Feature(xorstr_("Yellowbeard User"), FEATURE_FLAG_TELEMETRY_SOURCE), 
  Dispatcher(std::move(InMessenger)) 
{
};

//---------------------------------------------------------------------------

bool YellowbeardUserFeature::OnStart()
{
	Dispatcher->Subscribe<FeatureHashUpdateEvent>(FeatureHashUpdateHandle, [this](const FeatureHashUpdateEvent& Ev) { OnFeatureHashUpdate(Ev); });
	Dispatcher->Subscribe<XboxAuthTokenEvent>(XboxAuthTokenHandle, [this](const XboxAuthTokenEvent& Ev) { OnXboxAuthToken(Ev); });

	UGeneralProjectSettings* ProjectSettings = (UGeneralProjectSettings*)UObjectUtilities::GetDefaultObject(UGeneralProjectSettings::StaticClass());

	if (!ProjectSettings)
	{
		return false;
	}
	
	if (!ProjectSettings->ProjectVersion.IsValid())
	{
		return false;
	}

	GameVersion = ProjectSettings->ProjectVersion.ToString();

	return true;
}

//---------------------------------------------------------------------------

bool YellowbeardUserFeature::OnEnd()
{
	Dispatcher->Unsubscribe(FeatureHashUpdateHandle);
	Dispatcher->Unsubscribe(XboxAuthTokenHandle);

	return true;
}

//---------------------------------------------------------------------------

void YellowbeardUserFeature::OnReset(EFeatureResetReason InResetReason) {}

//---------------------------------------------------------------------------

void YellowbeardUserFeature::PostTick(float DeltaTime) {}

//---------------------------------------------------------------------------

void YellowbeardUserFeature::Tick(float DeltaTime) 
{
}

//---------------------------------------------------------------------------

bool YellowbeardUserFeature::CanExecute()
{
	return TargetQueue.size() > 0 && Timestamp::GetTimeSince(TimeSinceLastProcess) > 2500;
}

//---------------------------------------------------------------------------

void YellowbeardUserFeature::OnConditionChange(bool bConditionChange) {}

//---------------------------------------------------------------------------

void YellowbeardUserFeature::OnExecute(float DeltaTime) 
{
	for (auto It = TargetQueue.begin(); It != TargetQueue.end();)
	{
		auto& User = *It;

		// If the request failed or we reached our sequence target, stop processing anymore
		if ( User.second == 6 )
		{
			PLOGF(error, "[YellowbeardUserFeature] Finished processing yellow for {0}", User.first);

			NotificationEvent NotifEvent(
				4000,
				"Yellow",
				std::format("Successfully yellowbearded {}! They will be banned soon.", User.first),
				"\xEF\x9E\x82",
				ImColor(0, 255, 0, 255),
				NotificationEvent::ESound::Celebrate
			);

			Dispatcher->Emit(NotifEvent);

			It = TargetQueue.erase(It);
			continue;
		}

		if ( !ProcessBanEvent(User.first, User.second) )
		{
			PLOGF(error, "[YellowbeardUserFeature] Stopped processing yellow for {0}", User.first);

			It = TargetQueue.erase(It);
			continue;
		}

		PLOGF(debug, "[YellowbeardUserFeature] Processed yellow for {0} with sequence ID {1}", User.first, User.second);

		// Increment the sequence by 1
		User.second = User.second + 1;
		It++;
	}

	TimeSinceLastProcess = Timestamp::Now();
}

//---------------------------------------------------------------------------

void YellowbeardUserFeature::OnDiscard(float DeltaTime) {}

//---------------------------------------------------------------------------

bool YellowbeardUserFeature::YellowbeardUser(uint64_t InTargetXuid)
{
	if (InTargetXuid == 0 || InTargetXuid < 2500)
	{
		PLOG(error, "[YellowbeardUserFeature] Failed to yellow user due to invalid XUID");
		return false;
	}

	if (FeatureHash.empty())
	{
		Dispatcher->Emit(NotificationEvent(
			4000,
			"Yellow",
			"Required data not obtained. You must at least search for a session once.",
			"\xEF\x9E\x82",
			ImColor(255, 255, 0, 255),
			NotificationEvent::ESound::Warning
		));

		PLOG(error, "[YellowbeardUserFeature] Failed to yellow user due to empty feature hash");
		return false;
	}

	if (XboxToken.empty())
	{
		PLOG(error, "[YellowbeardUserFeature] Failed to yellow user due to empty xbox ticket");
		return false;
	}

	if (!TargetQueue.empty())
	{
		NotificationEvent ErrorEvent(
			4000,
			"Yellow",
			"You are already banning someone. Please wait until you're notified it has finished.",
			"\xEF\x9E\x82",
			ImColor(255, 0, 0, 255),
			NotificationEvent::ESound::Warning
		);

		Dispatcher->Emit(ErrorEvent);

		PLOG(error, "[YellowbeardUserFeature] Failed to yellow user due a non empty queue");
		return false;
	}

	TargetQueue.insert({ InTargetXuid, 0 });

	PLOGF(debug, "[YellowbeardUserFeature] Issued yellow to {0}", InTargetXuid);

	

	Dispatcher->Emit(NotificationEvent(
		4000,
		"Yellow",
		std::format("Attempting to yellowbeard {}! Please wait...", InTargetXuid),
		"\xEF\x9E\x82",
		ImColor(255, 255, 0, 255),
		NotificationEvent::ESound::Success
	));

	FakeGameGuid = UKismetGuidLibrary::NewGuid().ToStringUUID();
	FakeSessionId = UKismetGuidLibrary::NewGuid().ToStringUUID();

	return true;
}

//---------------------------------------------------------------------------

bool YellowbeardUserFeature::ProcessBanEvent(uint64_t InTargetXuid, uint32_t InSequence)
{
	// Should never happen
	if (InTargetXuid == 0)
	{
		return false;
	}

	HINTERNET SessionHandle  = InternetOpenA("game=Athena, engine=UE4, version=0", INTERNET_OPEN_TYPE_PRECONFIG, 0, 0, 0);

	if (!SessionHandle)
	{
		PLOG(error, "[YellowbeardUserFeature] Failed to open SessionHandle");
		return false;
	}


	HINTERNET ConnectHandle  = InternetConnectA(SessionHandle, "athenaprod.maelstrom.gameservices.xboxlive.com", INTERNET_DEFAULT_HTTPS_PORT, 0, 0, INTERNET_SERVICE_HTTP, 0, 0); // athenaprod.maelstrom.gameservices.xboxlive.com
	if (!ConnectHandle)
	{
		PLOG(error, "[YellowbeardUserFeature] Failed to open ConnectHandle");
		InternetCloseHandle(SessionHandle);
		return false;
	}

	// Raw example data. We will change this later...
	static std::string ContentRaw = xorstr_(R"({"sequenceId":0,"typeId":916637836,"name":"PlayerDeveloperTelemetryEvent","timestampUtc":"2024-08-20T02:14:13Z","body":{"PlayerDeveloperTelemetryEvent":{"eventTag":"Panic Teleport","json":{"Reason":"FEventPlayerTeleportToSafety","IsBased":false,"IsTeleporting":false,"IsWaitingToSpawn":false,"BasedLocationX":0,"BasedLocationY":0,"BasedLocationZ":0,"WorldLocationX":-294300,"WorldLocationY":-259017,"WorldLocationZ":90.1236343383789,"MovementModeName":"Walking","ShipType":"None","ShipRegion":"None","WieldedItem":"None","CurrentWorldLocation":"bsp_outpost_1"}},"PlayerPositionTelemetryFragment":{"position":{"x":-294300,"y":-259017,"z":90.1236343383789},"worldLocationName":"bsp_outpost_1"},"PlayerBaseTelemetryFragment":{"playerGameId":"0","userId":"0","pirateId":"0","platformId":4,"deviceId":"0"},"ServerTelemetryFragment":{"serverSequenceId":6,"serverSessionId":"0","serverBuildId":"2.132.6332.0","serverLocation":"","serverPlayMode":"Adventure","playModeState":"","privateServerId":"00000000-0000-0000-0000-000000000000","serverIsPrivateServer":false,"serverIsActive":true,"serverIsXboxPadOnlySession":false},"BaseTelemetryFragment":{"dateLogged":"2024-08-20T01:58:17.975Z","sandbox":"RETAIL","featureConfigHash":"B174CE1F","stampId":"stamp6","lifeTime":42.57232666015625}}})");

	nlohmann::json EventJson = nlohmann::json::parse(ContentRaw, 0, false);

	if (EventJson.is_discarded())
	{
		PLOG(error, "[YellowbeardUserFeature] Body event data base could not be parsed");
		InternetCloseHandle(SessionHandle);
		InternetCloseHandle(ConnectHandle);
		return false;
	}

	std::string Resource = std::format("/tenants/athenaprodga/routes/game/{}", FakeSessionId);
	std::string CurrentTime = GetCurrentISOTime();

	
	// Create and send the POST request
	HINTERNET RequestHandle = HttpOpenRequestA(
		ConnectHandle,
		"POST",
		Resource.data(),
		NULL,
		NULL,
		NULL,
		INTERNET_FLAG_RELOAD | INTERNET_FLAG_SECURE,
		0 
	);

	if (!RequestHandle)
	{
		PLOGF(error, "[YellowbeardUserFeature] Failed to open RequestHandle {0}", GetLastError());

		InternetCloseHandle(SessionHandle);
		InternetCloseHandle(ConnectHandle);
		InternetCloseHandle(RequestHandle);
		return false;
	}

    // Modify the body data to be unique each call.
    // I'd assume they can identify fucky client telemetry. This is still not perfect.
	EventJson[xorstr_("timestampUtc")] = CurrentTime;
	EventJson[xorstr_("sequenceId")] = InSequence;

	EventJson[xorstr_("body")][xorstr_("BaseTelemetryFragment")][xorstr_("dateLogged")] = CurrentTime;
	EventJson[xorstr_("body")][xorstr_("BaseTelemetryFragment")][xorstr_("featureConfigHash")] = FeatureHash;
	EventJson[xorstr_("body")][xorstr_("BaseTelemetryFragment")][xorstr_("lifeTime")] = UKismetMathLibrary::RandomIntegerInRange(5, 400);

	EventJson[xorstr_("body")][xorstr_("PlayerBaseTelemetryFragment")][xorstr_("playerGameId")] = FakeGameGuid;
	EventJson[xorstr_("body")][xorstr_("PlayerBaseTelemetryFragment")][xorstr_("userId")] = InTargetXuid;
	EventJson[xorstr_("body")][xorstr_("PlayerBaseTelemetryFragment")][xorstr_("pirateId")] = FakeGameGuid;
	EventJson[xorstr_("body")][xorstr_("PlayerBaseTelemetryFragment")][xorstr_("deviceId")] = "F50CC8BE35CFFEF1";

	int32_t x = UKismetMathLibrary::RandomIntegerInRange(-30000, 30000);
	int32_t y = UKismetMathLibrary::RandomIntegerInRange(-30000, 30000);
	int32_t z = UKismetMathLibrary::RandomIntegerInRange(-30000, 30000);

	EventJson[xorstr_("body")][xorstr_("PlayerPositionTelemetryFragment")]["position"]["x"] = x;
	EventJson[xorstr_("body")][xorstr_("PlayerPositionTelemetryFragment")]["position"]["y"] = y;
	EventJson[xorstr_("body")][xorstr_("PlayerPositionTelemetryFragment")]["position"]["z"] = z;

	EventJson[xorstr_("body")][xorstr_("PlayerDeveloperTelemetryEvent")]["json"]["WorldLocationX"] = x;
	EventJson[xorstr_("body")][xorstr_("PlayerDeveloperTelemetryEvent")]["json"]["WorldLocationY"] = y;
	EventJson[xorstr_("body")][xorstr_("PlayerDeveloperTelemetryEvent")]["json"]["WorldLocationZ"] = z;

	nlohmann::json FinalJson;

	FinalJson["events"] = nlohmann::json::array();
	FinalJson["events"].push_back(EventJson);

	std::string Dumped = FinalJson.dump();

	

	std::string Headers = std::format("Content-Type: application/ms-maelstrom.v3+json;type=eventbatch;charset=utf-8\r\nAuthorization: {}\r\nX-Accept-Encoding: deflate\r\nConnection: Keep-Alive\r\nCache-Control: no-cache", XboxToken);

	// Specify the POST data
	BOOL bResults = HttpSendRequestA(
		RequestHandle, 
		Headers.data(), 
		Headers.size(), 
		Dumped.data(), 
		Dumped.size() 
	);

	if (!bResults)
	{
		PLOGF(error, "[YellowbeardUserFeature] Failed to send request {0}", GetLastError());
	}

	DWORD StatusCode = 0;
	DWORD StatusCodeSize = sizeof(StatusCode);

	// Query the status code from the response headers
	HttpQueryInfoA(RequestHandle, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &StatusCode, &StatusCodeSize, NULL);

	InternetCloseHandle(SessionHandle);
	InternetCloseHandle(ConnectHandle);
	InternetCloseHandle(RequestHandle);

	return (StatusCode == 202 && bResults);
}

//---------------------------------------------------------------------------

void YellowbeardUserFeature::OnFeatureHashUpdate(const FeatureHashUpdateEvent& Ev)
{
	char Data[9]; // 8 characters for the number + 1 for the null terminator

	sprintf(Data, "%08X", Ev.FeatureHash);

	PLOG(debug, "[YellowbeardUserFeature] Updated feature hash");

	FeatureHash = std::string(Data);
}

//---------------------------------------------------------------------------

void YellowbeardUserFeature::OnXboxAuthToken(const XboxAuthTokenEvent& Ev)
{
	if (Ev.IsTitleToken)
		return;

	XboxToken = Ev.Token;
}

//---------------------------------------------------------------------------

std::string YellowbeardUserFeature::GetCurrentISOTime()
{
	auto Now = std::chrono::system_clock::now();

	std::time_t NowTime = std::chrono::system_clock::to_time_t(Now);

	std::tm TimeUTC = *std::gmtime(&NowTime);

	auto NowUS = std::chrono::duration_cast<std::chrono::microseconds>(Now.time_since_epoch()) % 1000000;

	std::ostringstream OSS;
	OSS << std::put_time(&TimeUTC, "%Y-%m-%dT%H:%M:%S");
	OSS << '.' << std::setfill('0') << std::setw(6) << NowUS.count() << 'Z';

	return OSS.str();
}

//---------------------------------------------------------------------------