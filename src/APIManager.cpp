#include "APIManager.h"

void APIs::RequestAPIs()
{
	if (!FCFW) {
		FCFW = reinterpret_cast<FCFW_API::IVFCFW1*>(FCFW_API::RequestPluginAPI(FCFW_API::InterfaceVersion::V1));
		if (FCFW) {
			log::info("Obtained FCFW API - {0:x}", reinterpret_cast<uintptr_t>(FCFW));
		} else {
			log::info("Failed to obtain FCFW API");
		}
	}
}

