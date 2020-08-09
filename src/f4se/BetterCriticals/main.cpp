#include "f4se_common/f4se_version.h"
#include "f4se/PluginAPI.h"
#include <shlobj.h>
#include "Config.h"
#include "BetterCriticals.h"


IDebugLog						gLog;
PluginHandle					g_pluginHandle = kPluginHandle_Invalid;
F4SEMessagingInterface			* g_messaging = NULL;
F4SEPapyrusInterface			* g_papyrus = NULL;


void F4SEMessageHandler(F4SEMessagingInterface::Message* msg)
{
	if (msg->type == F4SEMessagingInterface::kMessage_GameDataReady) {
		if (msg->data) {
			BetterCriticals::LoadConfigData();
		}
	}
}


extern "C"
{
	bool F4SEPlugin_Query(const F4SEInterface * f4se, PluginInfo * info)
	{
		gLog.OpenRelative(CSIDL_MYDOCUMENTS, "\\My Games\\Fallout4\\F4SE\\BetterCriticals.log");

		info->infoVersion = PluginInfo::kInfoVersion;
		info->name = PLUGIN_NAME;
		info->version = PLUGIN_VERSION;
		
		// version check
		if (f4se->runtimeVersion != SUPPORTED_RUNTIME_VERSION) {
			char buf[512];
			sprintf_s(buf, "Better Criticals does not work with the installed F4SE version!\n\nExpected: %d.%d.%d.%d\nCurrent:  %d.%d.%d.%d",
				GET_EXE_VERSION_MAJOR(SUPPORTED_RUNTIME_VERSION),
				GET_EXE_VERSION_MINOR(SUPPORTED_RUNTIME_VERSION),
				GET_EXE_VERSION_BUILD(SUPPORTED_RUNTIME_VERSION),
				GET_EXE_VERSION_SUB(SUPPORTED_RUNTIME_VERSION),
				GET_EXE_VERSION_MAJOR(f4se->runtimeVersion),
				GET_EXE_VERSION_MINOR(f4se->runtimeVersion),
				GET_EXE_VERSION_BUILD(f4se->runtimeVersion),
				GET_EXE_VERSION_SUB(f4se->runtimeVersion));
			MessageBox(NULL, buf, "F4SE Version Error", MB_OK | MB_ICONEXCLAMATION);
			_FATALERROR("ERROR: F4SE version mismatch");
			return false;
		}
		else {
			_MESSAGE("Loading %s v%i.%i.%i.%i...", PLUGIN_NAME,
				GET_EXE_VERSION_MAJOR(PLUGIN_VERSION), GET_EXE_VERSION_MINOR(PLUGIN_VERSION), GET_EXE_VERSION_BUILD(PLUGIN_VERSION), GET_EXE_VERSION_SUB(PLUGIN_VERSION)
			);
		}
		
		g_pluginHandle = f4se->GetPluginHandle();
		g_messaging = (F4SEMessagingInterface *)f4se->QueryInterface(kInterface_Messaging);
		if (!g_messaging) {
			_FATALERROR("ERROR: Messaging query failed");
			return false;
		}
		g_papyrus = (F4SEPapyrusInterface *)f4se->QueryInterface(kInterface_Papyrus);
		if (!g_papyrus) {
			_FATALERROR("ERROR: Papyrus query failed");
			return false;
		}
		return true;
	}

	bool F4SEPlugin_Load(const F4SEInterface * f4se)
	{
		if (g_messaging) {
			g_messaging->RegisterListener(g_pluginHandle, "F4SE", F4SEMessageHandler);
		}
		if (g_papyrus) {
			g_papyrus->Register(PapyrusBetterCriticals::RegisterPapyrus);
		}
		return true;
	}
};

