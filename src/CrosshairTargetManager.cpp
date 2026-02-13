#include "CrosshairTargetManager.h"

namespace SecondSight {
	void CrosshairTargetManager::Register() {
		auto crosshair = SKSE::GetCrosshairRefEventSource();
        if (!crosshair) {
            log::error("{}: Failed to get crosshair reference event source", __FUNCTION__);
            return;
        }
		crosshair->AddEventSink(&CrosshairTargetManager::GetSingleton());
		log::info("{}: Registered {}"sv, __FUNCTION__, typeid(SKSE::CrosshairRefEvent).name());
	}

    RE::Actor* CrosshairTargetManager::GetActorUnderCrosshair() {
        if (m_cachedRef) {
            if (auto crosshairRefPtr = m_cachedRef.get()) {
                auto crosshairActor = RE::ActorPtr(m_cachedRef.get()->As<RE::Actor>());
                if (crosshairActor && crosshairActor->GetHandle()) {
                    RE::Actor* actor = crosshairActor->GetHandle().get().get();
                    return actor;
                }
            }
        }
        return nullptr;
    }

	RE::BSEventNotifyControl CrosshairTargetManager::ProcessEvent(const SKSE::CrosshairRefEvent* a_event, RE::BSTEventSource<SKSE::CrosshairRefEvent>*) {
		m_cachedRef = a_event && a_event->crosshairRef ? a_event->crosshairRef->CreateRefHandle() : RE::ObjectRefHandle();
log::info("{}: Crosshair reference changed. New ref: 0x{:X}", __FUNCTION__, m_cachedRef.get() ? m_cachedRef.get()->GetFormID() : 0);
		return RE::BSEventNotifyControl::kContinue;
	}
}