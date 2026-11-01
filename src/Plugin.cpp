#include "common/IPrefix.h"

#include <string>

#include "skse/PluginAPI.h"
#include "skse/GameCamera.h"
#include "skse/GameData.h"
#include "skse/GameEvents.h"
#include "skse/GameMenus.h"
#include "skse/GameReferences.h"
#include "skse/skse_version.h"

#include "CameraDirector.h"
#include "Logger.h"

#include <Windows.h>

namespace
{
	constexpr char kPluginName[] = "CinematicConversationCameraLE";
	PluginHandle g_pluginHandle = kPluginHandle_Invalid;
	SKSEMessagingInterface* g_messaging = nullptr;
	void Log(const char* a_message)
	{
		Logger::Write(a_message);
	}

	void OnSKSEMessage(SKSEMessagingInterface::Message* a_message)
	{
		if (!a_message) {
			return;
		}

		if (a_message->type == SKSEMessagingInterface::kMessage_DataLoaded) {
			if (CameraDirector::Initialize()) {
				Log("SKSE DataLoaded received; dialogue menu frame hook installed.");
			} else {
				Log("SKSE DataLoaded received, but dialogue camera initialization failed.");
			}
		}
	}
}

extern "C"
{
	__declspec(dllexport) bool SKSEPlugin_Query(const SKSEInterface* a_skse, PluginInfo* a_info)
	{
		if (!a_skse || !a_info) {
			return false;
		}

		a_info->infoVersion = PluginInfo::kInfoVersion;
		a_info->name = kPluginName;
		a_info->version = 2;

		if (a_skse->isEditor || a_skse->runtimeVersion != RUNTIME_VERSION_1_9_32_0) {
			return false;
		}

		return true;
	}

	__declspec(dllexport) bool SKSEPlugin_Load(const SKSEInterface* a_skse)
	{
		if (!a_skse || a_skse->runtimeVersion != RUNTIME_VERSION_1_9_32_0) {
			return false;
		}
		Logger::Initialize();

		g_pluginHandle = a_skse->GetPluginHandle();
		g_messaging = static_cast<SKSEMessagingInterface*>(
			a_skse->QueryInterface(kInterface_Messaging));
		if (!g_messaging || g_messaging->interfaceVersion < SKSEMessagingInterface::kInterfaceVersion) {
			Log("SKSE messaging interface unavailable or too old.");
			return false;
		}

		if (!g_messaging->RegisterListener(g_pluginHandle, "SKSE", OnSKSEMessage)) {
			Log("Could not register for SKSE lifecycle messages.");
			return false;
		}

		Log("Loaded for Skyrim LE 1.9.32.");
		return true;
	}
}

