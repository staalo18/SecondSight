#pragma once

namespace SecondSight {
    class CrosshairTargetManager : public RE::BSTEventSink<SKSE::CrosshairRefEvent>
	{
	public:
        static CrosshairTargetManager& GetSingleton() {
            static CrosshairTargetManager instance;
            return instance;
        }  

		static void	Register();
		
        RE::Actor* GetActorUnderCrosshair();

		virtual RE::BSEventNotifyControl ProcessEvent(const SKSE::CrosshairRefEvent* a_event, RE::BSTEventSource<SKSE::CrosshairRefEvent>* a_eventSource) override;

	private:
		CrosshairTargetManager() = default;
		CrosshairTargetManager(const CrosshairTargetManager&) = delete;
		CrosshairTargetManager(CrosshairTargetManager&&) = delete;
		virtual ~CrosshairTargetManager() = default;

		CrosshairTargetManager& operator=(const CrosshairTargetManager&) = delete;
		CrosshairTargetManager& operator=(CrosshairTargetManager&&) = delete;

		RE::ObjectRefHandle m_cachedRef;
	};
}