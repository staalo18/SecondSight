#include "Hooks.h"
#include "_ts_SKSEFunctions.h"
#include "FreeCameraManager.h"

namespace Hooks
{
	void Install()
	{
		log::info("Hooking...");

		FreeCameraStateHook::Hook();

		log::info("...success");
	}

	void FreeCameraStateHook::Update(RE::FreeCameraState* a_this, RE::BSTSmartPointer<RE::TESCameraState>& a_nextState)
	{

		_Update(a_this, a_nextState);

		SecondSight::FreeCameraManager::GetSingleton().Update();
	}
} // namespace Hooks
