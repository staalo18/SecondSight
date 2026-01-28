#include "_ts_SKSEFunctions.h"
#include "Hooks.h"
#include "FreeCameraManager.h"
#include "APIManager.h"

namespace SecondSight {
    namespace Interface {
        int GetSecondSightPluginVersion(RE::StaticFunctionTag*) {
            return 1;
        }

        void StartSecondSightEffect(RE::StaticFunctionTag*, RE::Actor* a_actor) {
            FreeCameraManager::GetSingleton().StartSecondSightEffect(a_actor); 
        }

        void StopSecondSightEffect(RE::StaticFunctionTag*) {
            FreeCameraManager::GetSingleton().StopSecondSightEffect(); 
        }

        RE::Actor* GetCrosshairTarget(RE::StaticFunctionTag*, float a_maxTargetDistance, float a_maxTargetScanAngle) {
            return _ts_SKSEFunctions::GetCrosshairTarget(a_maxTargetDistance, a_maxTargetScanAngle);
        }
        
        bool SecondSightFunctions(RE::BSScript::Internal::VirtualMachine * a_vm){
            a_vm->RegisterFunction("GetSecondSightPluginVersion", "_ts_SecondSightFunctions", GetSecondSightPluginVersion);
            a_vm->RegisterFunction("StartSecondSightEffect", "_ts_SecondSightFunctions", StartSecondSightEffect);
            a_vm->RegisterFunction("StopSecondSightEffect", "_ts_SecondSightFunctions", StopSecondSightEffect);
            a_vm->RegisterFunction("GetCrosshairTarget", "_ts_SecondSightFunctions", GetCrosshairTarget);
            return true;
        }
    } // namespace Interface
} // namespace SecondSight

/******************************************************************************************/
void MessageHandler(SKSE::MessagingInterface::Message* a_msg)
{
	// Try requesting APIs at multiple steps to try to work around the SKSE messaging bug
	switch (a_msg->type) {
	case SKSE::MessagingInterface::kDataLoaded:
		APIs::RequestAPIs();
		break;
	case SKSE::MessagingInterface::kPostLoad:
		APIs::RequestAPIs();
		break;
	case SKSE::MessagingInterface::kPostPostLoad:
		APIs::RequestAPIs();
		break;
	case SKSE::MessagingInterface::kPreLoadGame:
		break;
	case SKSE::MessagingInterface::kPostLoadGame:
	case SKSE::MessagingInterface::kNewGame:
		APIs::RequestAPIs();
        SecondSight::FreeCameraManager::GetSingleton().Initialize();
		break;
	}
}
/******************************************************************************************/
SKSEPluginInfo(
    .Version = Plugin::VERSION,
    .Name = Plugin::NAME,
    .Author = "Staalo",
    .RuntimeCompatibility = SKSE::PluginDeclaration::RuntimeCompatibility(SKSE::VersionIndependence::AddressLibrary),
    .MinimumSKSEVersion = { 2, 2, 3 } // or 0 if you want to support all
)

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* skse) {
    long logLevel = _ts_SKSEFunctions::GetValueFromINI(nullptr, 0, "LogLevel:Log", "SKSE/Plugins/SecondSight.ini", 3L);
    bool isLogLevelValid = true;
    if (logLevel < 0 || logLevel > 6) {
        logLevel = 2L; // info
        isLogLevelValid = false;
    }

	_ts_SKSEFunctions::InitializeLogging(static_cast<spdlog::level::level_enum>(logLevel));
    if (!isLogLevelValid) {
        log::warn("{}: LogLevel in INI file is invalid. Defaulting to info level.", __FUNCTION__);
    }
    log::info("{}: SecondSight Plugin version: {}", __FUNCTION__, SecondSight::Interface::GetSecondSightPluginVersion(nullptr));

    Init(skse);
    auto messaging = SKSE::GetMessagingInterface();
	if (!messaging->RegisterListener("SKSE", MessageHandler)) {
		return false;
	}

    if (!SKSE::GetPapyrusInterface()->Register(SecondSight::Interface::SecondSightFunctions)) {
        log::error("{}: Failed to register Papyrus functions.", __FUNCTION__);
        return false;
    } else {
        log::info("{}: Registered Papyrus functions", __FUNCTION__);
    }

    SKSE::AllocTrampoline(64);
    
    log::info("{}: Calling Install Hooks", __FUNCTION__);

    Hooks::Install();

    return true;
}
