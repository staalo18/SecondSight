#pragma once

#include "RE/F/FreeCameraState.h"
namespace Hooks
{
	class FreeCameraStateHook
	{
	public:
		static void Hook()
		{
			REL::Relocation<std::uintptr_t> FreeCameraStateVtbl{ RE::VTABLE_FreeCameraState[0] };
			_Update = FreeCameraStateVtbl.write_vfunc(0x3, Update);
		}

	private:
        static void Update(RE::FreeCameraState* a_this, RE::BSTSmartPointer<RE::TESCameraState>& a_nextState);
		static inline REL::Relocation<decltype(Update)> _Update;
	};

	void Install();
} // namespace Hooks	

