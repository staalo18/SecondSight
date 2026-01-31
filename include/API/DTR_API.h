#pragma once
#include <functional>
#include <stdint.h>

/*
* For modders: Copy this file into your own project if you wish to use this API
*/
namespace DTR_API {
	constexpr const auto DTRPluginName = "DynamicTargetReticle";

	// SKSE Messaging Interface - Timeline Event Types
	// Consumers can register a listener with SKSE::GetMessagingInterface()->RegisterListener()
	// and receive these messages from DTR
	enum class DTRMessage : uint32_t {
		// Dispatched when the reticle loses its target
		// Data: DTRTimelineEventData*
		kLostTarget = 0,

        // Dispatched when the reticle acquires a new target
        // Data: DTRTimelineEventData*
        kFoundTarget = 1
	};

	// Event data structure for timeline events
	// Cast the 'data' parameter in your message handler to this type
	struct DTRTimelineEventData {
		RE::Actor* target;  // current target
	};

	// Available DTR interface versions
	enum class InterfaceVersion : uint8_t {
		V1
	};

	// DTR's modder interface
	class IVDTR1 {
	public:
		/// <summary>
		/// Get the thread ID DTR is running in.
		/// You may compare this with the result of GetCurrentThreadId() to help determine
		/// if you are using the correct thread.
		/// </summary>
		/// <returns>TID</returns>
		[[nodiscard]] virtual unsigned long GetDTRThreadId() const noexcept = 0;

		/// <summary>
		/// Get the DTR plugin version as an integer.
		/// Encoded as: major * 10000 + minor * 100 + patch
		/// Example: version 1.2.3 returns 10203
		/// </summary>
		/// <returns>Version encoded as integer</returns>
		[[nodiscard]] virtual int GetDTRPluginVersion() const noexcept = 0;

        /// <summary>
        /// Check if the target reticle is currently active.
        /// </summary>
        /// <returns>True if the reticle is active, false otherwise</returns>
        [[nodiscard]] virtual bool IsReticleActive() const noexcept = 0;

        /// <summary>
        /// Show or hide the target reticle.
        /// </summary>
        /// <param name="a_show">True to show, false to hide</param>
        virtual void ShowReticle(bool a_show) const noexcept = 0;

        /// <summary>
        /// Get the current target actor.
        /// </summary>
        /// <returns>Pointer to the current target actor, or nullptr if no target</returns
        virtual RE::Actor* GetCurrentTarget() const noexcept = 0;
	};

	typedef void* (*_RequestPluginAPI)(const InterfaceVersion interfaceVersion);

	/// <summary>
	/// Request the DTR API interface.
	/// Recommended: Send your request during or after SKSEMessagingInterface::kMessage_PostLoad to make sure the dll has already been loaded
	/// </summary>
	/// <param name="a_interfaceVersion">The interface version to request</param>
	/// <returns>The pointer to the API singleton, or nullptr if request failed</returns>
	[[nodiscard]] inline void* RequestPluginAPI(const InterfaceVersion a_interfaceVersion = InterfaceVersion::V1) {
		auto pluginHandle = GetModuleHandle("DynamicTargetReticle.dll");
		_RequestPluginAPI requestAPIFunction = (_RequestPluginAPI)GetProcAddress(pluginHandle, "RequestPluginAPI");
		if (requestAPIFunction) {
			return requestAPIFunction(a_interfaceVersion);
		}
		return nullptr;
	}
}