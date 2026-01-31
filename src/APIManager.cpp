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

	if (!DTR) {
		DTR = reinterpret_cast<DTR_API::IVDTR1*>(DTR_API::RequestPluginAPI(DTR_API::InterfaceVersion::V1));
		if (DTR) {
			log::info("Obtained DTR API - {0:x}", reinterpret_cast<uintptr_t>(DTR));
		} else {
			log::info("Failed to obtain DTR API");
		}
	}

	if (!TrueDirectionalMovementV1) {
		TrueDirectionalMovementV1 = reinterpret_cast<TDM_API::IVTDM1*>(TDM_API::RequestPluginAPI(TDM_API::InterfaceVersion::V1));
		if (TrueDirectionalMovementV1) {
			log::info("Obtained TrueDirectionalMovement API (V1) - {0:x}", reinterpret_cast<uintptr_t>(TrueDirectionalMovementV1));
		}
	}

}

