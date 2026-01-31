#pragma once

#include "API/FCFW_API.h"
#include "API/DTR_API.h"
#include "API/TrueDirectionalMovementAPI.h"


struct APIs
{
    static inline FCFW_API::IVFCFW1* FCFW = nullptr;

    static inline DTR_API::IVDTR1* DTR = nullptr;

    static inline TDM_API::IVTDM1* TrueDirectionalMovementV1 = nullptr;

	static void RequestAPIs();
};
