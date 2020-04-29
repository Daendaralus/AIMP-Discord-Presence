#define _CRT_SECURE_NO_WARNINGS

#include "aimpPlugin.h"
#include <time.h>

AIMPRemote* aimpRemote;
discord::Core* core = nullptr;
long appid = 705094545645502574;
HRESULT __declspec(dllexport) WINAPI AIMPPluginGetHeader(IAIMPPlugin** Header)
{
	*Header = new Plugin();
	return S_OK;
}

HRESULT WINAPI Plugin::Initialize(IAIMPCore* Core)
{
	HWND AIMPRemoteHandle = FindWindowA(AIMPRemoteAccessClass, AIMPRemoteAccessClass);
	if (AIMPRemoteHandle == NULL)
	{
		return E_ABORT;
	}

	aimpRemote = new AIMPRemote();

	AIMPEvents Events = { 0 };
	Events.State = this->UpdatePlayerState;
	Events.TrackInfo = this->UpdateTrackInfo;
	Events.TrackPosition = this->UpdateTrackPosition;

	aimpRemote->AIMPSetEvents(&Events);
	aimpRemote->AIMPSetRemoteHandle(&AIMPRemoteHandle);

	return S_OK;
}

HRESULT WINAPI Plugin::Finalize()
{
	if (aimpRemote)
	{
		delete aimpRemote;
	}
	if (core)
	{
		delete core;
	}
	return S_OK;
}

VOID Plugin::DiscordReady(const DiscordUser* connectedUser)
{
	UpdateTrackInfo(&aimpRemote->AIMPGetTrackInfo());
}

VOID Plugin::UpdatePlayerState(INT AIMPRemote_State)
{
	if (AIMPRemote_State == AIMPREMOTE_PLAYER_STATE_PLAYING)
	{
		if (!core)
		{
			//Hacky workaround since ClearActivity() doesn't fully clear the app presence - only the details. To get completely rid of the rich presence I have to call ~Core down below
			auto result = discord::Core::Create(705094545645502574, DiscordCreateFlags_NoRequireDiscord, &core);
			if (result != discord::Result::Ok)
				core = nullptr;
		}
		
		UpdateTrackInfo(&aimpRemote->AIMPGetTrackInfo());
	}
	else
	{
		if (core)
		{
			//core->ActivityManager().ClearActivity([](discord::Result result) {
			//});
			//Workaround as described above
			core->~Core();
			core = nullptr;
		}
		
	}
}

VOID Plugin::UpdateTrackInfo(PAIMPTrackInfo AIMPRemote_TrackInfo)
{
	if (!core)
	{
		return;
	}
	{
		std::size_t length = AIMPRemote_TrackInfo->Artist.length();
		if (length >= 128) AIMPRemote_TrackInfo->Artist.replace(length - 3, length, "...");
		else if (length == 1) AIMPRemote_TrackInfo->Artist.append(" ");
	}
	{
		std::size_t length = AIMPRemote_TrackInfo->Title.length();
		if (length >= 128) AIMPRemote_TrackInfo->Title.replace(length - 3, length, "...");
		else if (length == 1) AIMPRemote_TrackInfo->Title.append(" ");
	}
	{
		std::size_t length = AIMPRemote_TrackInfo->Album.length();
		if (length >= 128) AIMPRemote_TrackInfo->Album.replace(length - 3, length, "...");
		else if (length == 1) AIMPRemote_TrackInfo->Album.append(" ");
	}
	discord::Activity activity{};
	activity.SetType(discord::ActivityType::Listening);
	activity.SetDetails(AIMPRemote_TrackInfo->Title.c_str());
	activity.SetState(AIMPRemote_TrackInfo->Artist.c_str());
	activity.GetAssets().SetLargeText(AIMPRemote_TrackInfo->Album.c_str());
	activity.GetAssets().SetLargeImage("aimpicon");
	activity.GetAssets().SetLargeText("Listening to music through AIMP");
	if (aimpRemote->AIMPGetPropertyValue(AIMP_RA_PROPERTY_PLAYER_STATE) == AIMPREMOTE_PLAYER_STATE_PLAYING)
	{
		int Duration =
			aimpRemote->AIMPGetPropertyValue(AIMP_RA_PROPERTY_PLAYER_DURATION) / 1000;
		if (Duration != 0)
		{
			activity.GetTimestamps().SetStart(time(0));
			activity.GetTimestamps().SetEnd(time(0)+((time_t)Duration- aimpRemote->AIMPGetPropertyValue(AIMP_RA_PROPERTY_PLAYER_POSITION) / 1000));
		}

	}

	if (AIMPRemote_TrackInfo->FileName.compare(0, 7, "http://") == 0 || AIMPRemote_TrackInfo->FileName.compare(0, 8, "https://") == 0)
	{
		if (!AIMPRemote_TrackInfo->Album[0])
		{
			activity.GetAssets().SetLargeText("Listening to URL");
		}

	}
	core->ActivityManager().UpdateActivity(activity, [](discord::Result result) {

	});
}

VOID Plugin::UpdateTrackPosition(PAIMPPosition AIMPRemote_Position)
{
	if(core)
		core->RunCallbacks();
}