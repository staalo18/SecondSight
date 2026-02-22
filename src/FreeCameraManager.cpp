#include "FreeCameraManager.h"
#include "_ts_SKSEFunctions.h"
#include "APIManager.h"
#include "Offsets.h"
#include "CrosshairTargetManager.h"
#include "CLIBUtil/EditorID.hpp"

namespace SecondSight {
    void FreeCameraManager::Initialize()
    {
        if (!APIs::FCFW) {
            log::error("{}: FCFW API not available, SecondSight will not function properly!", __FUNCTION__);
            RE::DebugMessageBox("SecondSight: FreeCamera Framework (FCFW) not available, SecondSight will not function properly!");
            return;
        }

        if (!APIs::DTR) {
            log::info("{}: DTR API not available.", __FUNCTION__);
        } else {
            APIs::DTR->ShowReticle(true);
        }

        // Reset state before RegisterPlugin (which cleans up orphaned timelines)
        m_isFreeCameraActive = false;
        m_target = nullptr;
        m_transitionToTarget_TimelineID = 0;
        m_atTarget_TimelineID = 0;
        m_transitionToPrevious_TimelineID = 0;

        if (!APIs::FCFW->RegisterPlugin(SKSE::GetPluginHandle())) {
            log::error("{}: Could not register SecondSight plugin with FCFW!", __FUNCTION__);
        }

        // Register listener for FCFW timeline events
        if (!SKSE::GetMessagingInterface()->RegisterListener(FCFW_API::FCFWPluginName, SecondSight::FreeCameraManager::FCFWMessageHandler)) {
            log::warn("{}: Failed to register FCFW message listener", __FUNCTION__);
        }
/*
        if (APIs::DTR)
        {
            // Register listener for DTR target reticle events
            if (!SKSE::GetMessagingInterface()->RegisterListener(DTR_API::DTRPluginName, SecondSight::FreeCameraManager::DTRMessageHandler)) {
                log::warn("{}: Failed to register DTR message listener", __FUNCTION__);
            }
        } */
    }

    void FreeCameraManager::FCFWMessageHandler(SKSE::MessagingInterface::Message* a_msg)
    {
        if (!APIs::FCFW) {
            return;
        }

        if (!a_msg || !a_msg->sender || strcmp(a_msg->sender, FCFW_API::FCFWPluginName) != 0) {
            return;
        }
        
        auto& self = GetSingleton();

        switch (static_cast<FCFW_API::FCFWMessage>(a_msg->type)) {
        case FCFW_API::FCFWMessage::kPlaybackStart:
            if (APIs::DTR) {
                APIs::DTR->ShowReticle(false);
            }
            break;
        case FCFW_API::FCFWMessage::kPlaybackStop:
            if (APIs::DTR) {
                APIs::DTR->ShowReticle(true);
            }
            break;
        case FCFW_API::FCFWMessage::kPlaybackWait:
            auto* eventData = static_cast<FCFW_API::FCFWTimelineEventData*>(a_msg->data);
            if (eventData && eventData->timelineID == self.m_transitionToTarget_TimelineID) {
                // timeline1 playback completed, switch to timeline2 playback

                if (!APIs::FCFW->SwitchPlayback(SKSE::GetPluginHandle(), eventData->timelineID, self.m_atTarget_TimelineID)) {
                    log::warn("{}: Could not switch playback", __FUNCTION__);
                }
            }
            break;
        }
    }

/*
    void FreeCameraManager::DTRMessageHandler(SKSE::MessagingInterface::Message* a_msg)
    {
        if (!APIs::DTR) {
            return;
        }

        if (!a_msg || !a_msg->sender || strcmp(a_msg->sender, DTR_API::DTRPluginName) != 0) {
            return;
        }
        
        switch (static_cast<DTR_API::DTRMessage>(a_msg->type)) {
        case DTR_API::DTRMessage::kLostTarget:
            break;
        case DTR_API::DTRMessage::kFoundTarget:
            auto* eventData = static_cast<DTR_API::DTRTimelineEventData*>(a_msg->data);
            if (eventData && eventData->target) {
                // found a target
            }        
            break;
        }
    }
*/
    
    void FreeCameraManager::Update() {
        if (RE::UI::GetSingleton()->GameIsPaused()) {
            return;
        }

        if (IsPlaybackActive()) {
            ClampFreeRotation();
    
            if (!(m_target && m_target->Get3D2())) {
                // lost target
                StopSecondSightEffect();
            }
        }
    }
  
    bool FreeCameraManager::StartSecondSightEffect() {

        UpdateTarget();
        if (!m_target) {
            log::warn("{}: No target available to start Second Sight Effect on.", __FUNCTION__);
            return false;
        }
        if (IsPlaybackActive()) {
            return false;
        }
        if (m_isFreeCameraActive) {
            log::warn("{}: Free Camera is already active.", __FUNCTION__);
            return false;
        }

        if (APIs::TrueDirectionalMovementV5) {
            auto result = APIs::TrueDirectionalMovementV5->RequestDisableTargetLock(SKSE::GetPluginHandle());
            if (result != TDM_API::APIResult::OK) {
                log::warn("{}: Failed to disable target lock control from True Directional Movement API. Result: {}", __FUNCTION__, static_cast<int>(result));
            }
        }

        m_isFreeCameraActive = true;
        ToggleFreeCamera();

        return true;
    }

    void FreeCameraManager::StopSecondSightEffect() {
        if (!IsPlaybackActive()) {
            return;
        }
        if (!m_isFreeCameraActive) {
            return;
        }

        if (!m_target) {
            log::error("{}: No target available.", __FUNCTION__);
            return;
        }

        if (APIs::TrueDirectionalMovementV5) {
            auto result = APIs::TrueDirectionalMovementV5->ReleaseDisableTargetLock(SKSE::GetPluginHandle());
            if (result != TDM_API::APIResult::OK) {
                log::warn("{}: Failed to release target lock control from True Directional Movement API. Result: {}", __FUNCTION__, static_cast<int>(result));
            }
        }

        m_isFreeCameraActive = false;
        ToggleFreeCamera();
    }

    void FreeCameraManager::UpdateTarget() {
        if (APIs::DTR && APIs::DTR->IsReticleActive()) { // if DTR reticle active, use DTR target (has prio)
            m_target = APIs::DTR->GetCurrentTarget();
        } else if (APIs::TrueDirectionalMovementV1 && APIs::TrueDirectionalMovementV1->GetTargetLockState()) {
            auto targetHandle = APIs::TrueDirectionalMovementV1->GetCurrentTarget();
            if (targetHandle) {
                m_target = targetHandle.get().get();
            } else {
                m_target = nullptr;
            }
        } else { // no other plugin found that would provide target info, use own logic
            // first, check directly under the cross hair (has prio)
            m_target =  CrosshairTargetManager::GetSingleton().GetActorUnderCrosshair();

            // if no actor under crosshair, try to find a target with GetCrosshairTarget() (cone and bounding shape based search)
            if (!m_target) {
                m_target = _ts_SKSEFunctions::GetCrosshairTarget(8000.0f, 0.5f);
log::info("{}: No target under crosshair, using GetCrosshairTarget().", __FUNCTION__);
            } else {
log::info("{}: Found target under crosshair: 0x{:X}", __FUNCTION__, m_target->GetFormID());
            }
        }

        if (m_target && (!GetCameraAnchorPoint() || 
                (m_target->GetDistance(RE::PlayerCharacter::GetSingleton()) > 8000.f) ||
                m_target->IsDead(true))) {
            m_target = nullptr;
        }
    }

    RE::NiPointer<RE::NiAVObject> FreeCameraManager::GetCameraAnchorPoint() {
        RE::NiPointer<RE::NiAVObject> targetPoint = nullptr;

        if (!m_target) {
            return nullptr;
        }

        auto race = m_target->GetRace();
        if (!race) {
            return nullptr;
        }

        RE::BGSBodyPartData* bodyPartData = race->bodyPartData;
        if (!bodyPartData) {
            return nullptr;
        }

        auto actor3D = m_target->Get3D2();
        if (!actor3D) {
            return nullptr;
        }
    
        RE::BGSBodyPart* bodyPart = bodyPartData->parts[RE::BGSBodyPartDefs::LIMB_ENUM::kHead];
        if (!bodyPart) {
            bodyPart = bodyPartData->parts[RE::BGSBodyPartDefs::LIMB_ENUM::kTotal];
        }
        if (bodyPart) {
            targetPoint = RE::NiPointer<RE::NiAVObject>(NiAVObject_LookupBoneNodeByName(actor3D, bodyPart->targetName, true));
        }

        return targetPoint;
    }

    bool FreeCameraManager::IsPlaybackActive() const { 
        if (!APIs::FCFW) {
            log::error("{}: FCFW API not available.", __FUNCTION__);
            return false;
        }

        SKSE::PluginHandle handle = SKSE::GetPluginHandle();

        if (APIs::FCFW->IsPlaybackRunning(handle, m_transitionToTarget_TimelineID) ||
            APIs::FCFW->IsPlaybackRunning(handle, m_atTarget_TimelineID) ||
            APIs::FCFW->IsPlaybackRunning(handle, m_transitionToPrevious_TimelineID)) {
            return true;
        }

        return false;
    }

    bool FreeCameraManager::InitializePlayback() {
        if (!m_target || IsPlaybackActive()) {
            return false;
        }

        auto targetPoint = GetCameraAnchorPoint();
        if (!targetPoint) {
            log::error("{}: Could not obtain target point.", __FUNCTION__);
            return false;
        }

        RE::PlayerCamera* playerCamera = RE::PlayerCamera::GetSingleton();
        if (!playerCamera) {
            log::error("{}: PlayerCamera singleton not found", __FUNCTION__);
            return false;
        }

        auto race = m_target->GetRace();
        if (!race || !race->bodyPartData) {
            return false;
        }
        
        auto bodyPartEDID = clib_util::editorID::get_editorID(race->bodyPartData);

        m_offset.x = 0.0f;    // left (-) / right (+) -  no sideways offset

        m_rotationOffset.x = 0.0f; // pitch
        m_rotationOffset.y = 0.0f; // roll
        m_rotationOffset.z = 0.0f; // yaw
        // define offset to account for head dimensions
        if (bodyPartEDID == "DragonBodyPartData" || bodyPartEDID == "DLC2DragonBodyPartData") {
            m_offset.y = 40.0f;    // forward (+) / backward (-)
            m_offset.z = 40.0f;    // up (+) / down (-)
        } else if (bodyPartEDID == "ChaurusBodyPartData") {
            m_offset.y = 40.0f;   // forward (+) / backward (-)
            m_offset.z = 0.0f;    // up (+) / down (-)
        } else if (bodyPartEDID == "ChickenBodyPartData") {
            m_offset.y = 10.0f;   // forward (+) / backward (-)
            m_offset.z = 0.0f;    // up (+) / down (-)
        } else if (bodyPartEDID == "CowBodyPartData" || bodyPartEDID == "DeerBodyPartData") {
            m_offset.y = 20.0f;    // forward (+) / backward (-)
            m_offset.z = 20.0f;    // up (+) / down (-)
            m_rotationOffset.x = -30.0f * PI / 180.f; // pitch
        } else if (bodyPartEDID == "DLC2MountedRieklingBodyPartData") {
            m_offset.y = 20.0f;    // forward (+) / backward (-)
            m_offset.z = 20.0f;    // up (+) / down (-)
            m_rotationOffset.x = -10.0f * PI / 180.f; // pitch
        } else if (bodyPartEDID == "HareBodyPartData") {
            m_offset.y = 10.0f;    // forward (+) / backward (-)
            m_offset.z = 10.0f;    // up (+) / down (-)
            m_rotationOffset.x = 30.0f * PI / 180.f; // pitch
        } else if (bodyPartEDID == "BearBodyPartData" || bodyPartEDID == "BenthicLurkerBodyPartData" ||
                   bodyPartEDID == "DogBodyPartData") {
            m_offset.y = 30.0f;    // forward (+) / backward (-)
            m_offset.z = 0.0f;    // up (+) / down (-)
        } else if (bodyPartEDID == "HorkerBodyPartData") {
            m_offset.y = 30.0f;    // forward (+) / backward (-)
            m_offset.z = 0.0f;    // up (+) / down (-)
            m_rotationOffset.x = 20.0f * PI / 180.f; // pitch
        } else if (bodyPartEDID == "DLC2HMDaedraPartData") {
            m_offset.y = 60.0f;   // forward (+) / backward (-)
            m_offset.z = 80.0f;    // up (+) / down (-)
        } else if (bodyPartEDID == "MammothBodyPartData") {
            m_offset.y = 0.0f;   // forward (+) / backward (-)
            m_offset.z = 100.0f;    // up (+) / down (-)
            m_rotationOffset.x = -40.0f * PI / 180.f; // pitch
        } else if (bodyPartEDID == "FrostbiteSpiderPartData") {
            m_offset.y = 20.0f;   // forward (+) / backward (-)
            m_offset.z = 40.0f;    // up (+) / down (-)
        } else if (bodyPartEDID == "DLC2ScribBodyPartData") {
            m_offset.y = 20.0f;    // forward (+) / backward (-)
            m_offset.z = 10.0f;    // up (+) / down (-)
            m_rotationOffset.x =-20.0f * PI / 180.f; // pitch
        } else if (bodyPartEDID == "SabreCatBodyPartData" || bodyPartEDID == "TrollBodyPartData" ||
                   bodyPartEDID == "WerewolfBeastBodyPartData") {
            m_offset.y = 40.0f;    // forward (+) / backward (-)
            m_offset.z = 0.0f;    // up (+) / down (-)
            m_rotationOffset.x =-20.0f * PI / 180.f; // pitch
        } else if (bodyPartEDID == "AtronachFlameBodyPartData" || bodyPartEDID == "AtronachFrostBodyPartData" ||
                   bodyPartEDID == "AtronachStormBodyPartData" || bodyPartEDID == "DefaultBodyPartData" ||
                   bodyPartEDID == "DLC2NetchBodyPartData" || bodyPartEDID == "DLC2RieklingBodyPartData" ||
                   bodyPartEDID == "DragonPriestBodyPartData" || bodyPartEDID == "DraugrBodyPartData" ||
                   bodyPartEDID == "DwarvenBallistaCenturionBodyPartData" || bodyPartEDID == "DwarvenSpiderPartData" ||
                   bodyPartEDID == "DwarvenSteamCenturionBodyPartData" || bodyPartEDID == "FalmerBodyPartData" ||
                   bodyPartEDID == "GargoyleBodyPartData" || bodyPartEDID == "GiantBodyPartData" ||
                   bodyPartEDID == "HagravenBodyPartData" || bodyPartEDID == "MudcrabPartData" ||
                   bodyPartEDID == "SprigganBodyPartData" || bodyPartEDID == "WitchlightBodyPartData" ||
                   bodyPartEDID == "SlaughterfishBodyPartData" || bodyPartEDID == "WispBodyPartData" ||
                   bodyPartEDID == "DwarvenSphereCenturionBodyPartData" || bodyPartEDID == "ChaurusFlyerBodyPartData" ||
                   bodyPartEDID == "HorseBodyPartData" || bodyPartEDID == "GoatBodyPartData" ||
                   bodyPartEDID == "SkeeverBodyPartData" || bodyPartEDID == "IceWraithBodyPartData") {
            m_offset.y = 20.0f;   // forward (+) / backward (-)
            m_offset.z = 0.0f;    // up (+) / down (-)
        } else {
            log::warn("{}: Unknown bodyPartEDID: {}, using default offset", __FUNCTION__, bodyPartEDID);
            m_offset.y = 20.0f;   // forward (+) / backward (-)
            m_offset.z = 0.0f;    // up (+) / down (-)
        }

        m_previousCameraPos = _ts_SKSEFunctions::GetCameraPos();

        m_prevRotation.x = 0.0f;
        m_prevRotation.y = 0.0f;
        m_prevRotation.z = 0.0f;

        RE::ThirdPersonState* thirdPersonState = nullptr;
        if (playerCamera->currentState) {
            m_previousCameraState = playerCamera->currentState->id;

            if (m_previousCameraState == RE::CameraState::kThirdPerson ||
                m_previousCameraState == RE::CameraState::kMount ||
                m_previousCameraState == RE::CameraState::kDragon) {
                thirdPersonState = static_cast<RE::ThirdPersonState*>(playerCamera->currentState.get());
            }

            auto rotation = _ts_SKSEFunctions::GetCameraRotation();
            m_prevRotation.x = rotation.x; // pitch
            m_prevRotation.y = rotation.y; // roll
            m_prevRotation.z = rotation.z; // yaw
        } else{
            log::warn("{}: PlayerCamera currentState is null", __FUNCTION__);
        }

        m_prevFreeRotation = thirdPersonState ? thirdPersonState->freeRotation : RE::NiPoint2{ 0.0f, 0.0f };

        return true;
    }

    bool FreeCameraManager::InitializeTimeline(size_t& a_timelineID) {
        if (!APIs::FCFW) {
            return false;
        }

        SKSE::PluginHandle handle = SKSE::GetPluginHandle();

        if (a_timelineID != 0) {
            if (!APIs::FCFW->ClearTimeline(handle, a_timelineID)) {
                log::error("{}: Could not clear timeline.", __FUNCTION__);
                return false;
            }
        } else {
            a_timelineID = APIs::FCFW->RegisterTimeline(handle);
        }
        if (a_timelineID == 0) {
            log::error("{}: Could not register timeline.", __FUNCTION__);
            return false;
        }

        return true;
    }

    bool FreeCameraManager::UpdateTimeline1() { 
        
        if (!APIs::FCFW) {
            log::error("{}: FCFW API not available, cannot update timeline", __FUNCTION__);
            return false;
        }

        if (!m_target) {
            log::error("{}: No target available to update timeline.", __FUNCTION__);
            return false;
        }

        if (!InitializePlayback()) {
            return false;
        }

        if (!InitializeTimeline(m_transitionToTarget_TimelineID)) {
            return false;
        }

        float transitionTime = ComputeTransitionTime(m_target->GetPosition());

        float rotationToMovement_End = 0.2f * transitionTime; // The time the camera finishes rotating towards the movement direction
        float rotationToTarget_Start = 0.5f * transitionTime; // the time the camera starts rotating towards the target

        SKSE::PluginHandle handle = SKSE::GetPluginHandle();
        RE::NiPoint3 rotationOffset = RE::NiPoint3(); // no offset

        int ret;
        ret = APIs::FCFW->AddTranslationPointAtCamera(handle, m_transitionToTarget_TimelineID, 0.0f, true, true);
        ret = APIs::FCFW->AddRotationPointAtCamera(handle, m_transitionToTarget_TimelineID, 0.f, true, true);
        ret = APIs::FCFW->AddRotationPointAtRef(handle, m_transitionToTarget_TimelineID, rotationToMovement_End, m_target, FCFW_API::BodyPart::kHead, rotationOffset, false, true, true);
        ret = APIs::FCFW->AddRotationPointAtRef(handle, m_transitionToTarget_TimelineID, rotationToTarget_Start, m_target, FCFW_API::BodyPart::kHead, rotationOffset, false, true, true);
        ret = APIs::FCFW->AddTranslationPointAtRef(handle, m_transitionToTarget_TimelineID, transitionTime, m_target, FCFW_API::BodyPart::kHead, m_offset, true, true, true);
        ret = APIs::FCFW->AddRotationPointAtRef(handle, m_transitionToTarget_TimelineID, transitionTime, m_target, FCFW_API::BodyPart::kHead, m_rotationOffset, true, true, true);
        ret = APIs::FCFW->SetPlaybackMode(handle, m_transitionToTarget_TimelineID, FCFW_API::PlaybackMode::kWait);

        return true;     
    }

    bool FreeCameraManager::UpdateTimeline2() { 
        if (!APIs::FCFW) {
            return false;
        }
        
        if (!m_target) {
            log::error("{}: No target available to update timeline.", __FUNCTION__);
            return false;
        }

        if (!InitializeTimeline(m_atTarget_TimelineID)) {
            return false;
        }

        SKSE::PluginHandle handle = SKSE::GetPluginHandle();
        RE::NiPoint3 rotationOffset = RE::NiPoint3(); // no offset

        int ret;
        ret = APIs::FCFW->AddTranslationPointAtRef(handle, m_atTarget_TimelineID, 0.f, m_target, FCFW_API::BodyPart::kHead, m_offset, true, true, true);
        ret = APIs::FCFW->AddRotationPointAtRef(handle, m_atTarget_TimelineID, 0.f, m_target, FCFW_API::BodyPart::kHead, m_rotationOffset, true, true, true);
        ret = APIs::FCFW->SetPlaybackMode(handle, m_atTarget_TimelineID, FCFW_API::PlaybackMode::kWait);
        APIs::FCFW->AllowUserRotation(handle, m_atTarget_TimelineID, true);

        return true;     
    }

    bool FreeCameraManager::UpdateTimeline3() {   
        if (!APIs::FCFW) {
            return false;
        }

        if (!m_target) {
            log::error("{}: No target available to update timeline.", __FUNCTION__);
            return false;
        }

        if (!InitializeTimeline(m_transitionToPrevious_TimelineID)) {
            return false;
        }

        float transitionTime = ComputeTransitionTime(m_previousCameraPos);

        SKSE::PluginHandle handle = SKSE::GetPluginHandle();
        RE::NiPoint3 rotationOffset = RE::NiPoint3(); // no offset
        
        int ret;
        ret = APIs::FCFW->AddTranslationPointAtCamera(handle, m_transitionToPrevious_TimelineID, 0.0f, true, true);
        ret = APIs::FCFW->AddRotationPointAtCamera(handle, m_transitionToPrevious_TimelineID, 0.f, true, true);
        ret = APIs::FCFW->AddRotationPointAtRef(handle, m_transitionToPrevious_TimelineID, 0.5f * transitionTime, m_target, FCFW_API::BodyPart::kHead, rotationOffset, true, true, true);
        ret = APIs::FCFW->AddTranslationPoint(handle, m_transitionToPrevious_TimelineID, transitionTime, m_previousCameraPos, true, true);
        ret = APIs::FCFW->AddRotationPoint(handle, m_transitionToPrevious_TimelineID, transitionTime, m_prevRotation, true, true);

        return true;     
    }

    void FreeCameraManager::ClampFreeRotation() {
        auto* playerCamera = RE::PlayerCamera::GetSingleton();
        RE::FreeCameraState* freeCameraState = nullptr;

        if (playerCamera && playerCamera->currentState && (playerCamera->currentState->id == RE::CameraState::kFree)) {
            freeCameraState = static_cast<RE::FreeCameraState*>(playerCamera->currentState.get());
        }
        
        if (!freeCameraState) {
            log::warn("{}: Not in Free Camera State", __FUNCTION__);
			return;
		}

        RE::NiPoint3 headRotation = _ts_SKSEFunctions::GetBodyPartRotation(m_target, RE::BGSBodyPartDefs::LIMB_ENUM::kHead);
        float headYaw = headRotation.z;
        float headPitch = headRotation.x;

        // Clamp pitch
        float relativePitch = _ts_SKSEFunctions::NormalRelativeAngle(freeCameraState->rotation.x - headPitch);
        relativePitch = std::clamp(relativePitch, - 0.45f * PI, 0.4f * PI);
        freeCameraState->rotation.x = _ts_SKSEFunctions::NormalRelativeAngle(headPitch + relativePitch);
        
        // Clamp yaw
        float relativeYaw = _ts_SKSEFunctions::NormalRelativeAngle(freeCameraState->rotation.y - headYaw);
        relativeYaw = std::clamp(relativeYaw, -0.5f * PI, 0.5f * PI);
        freeCameraState->rotation.y = _ts_SKSEFunctions::NormalRelativeAngle(headYaw + relativeYaw);
    }

    void FreeCameraManager::ToggleFreeCamera() {
        if (RE::UI::GetSingleton()->GameIsPaused()) {
            return;
        }

        if (!APIs::FCFW) {
            log::error("{}: FCFW API not available", __FUNCTION__);
            return;
        }

        auto activeTimelineID = APIs::FCFW->GetActiveTimelineID();
        if (activeTimelineID == 0) {
            if (!UpdateTimeline1()) {
                log::warn("{}: Could not update timeline1", __FUNCTION__);
                return;
            }
            if (!UpdateTimeline2()) {
                log::warn("{}: Could not update timeline2", __FUNCTION__);
                return;
            }
            
            if (!APIs::FCFW->SetFollowGround(SKSE::GetPluginHandle(), m_transitionToTarget_TimelineID, true, 10.0f)) {
                log::warn("{}: Could not set follow ground", __FUNCTION__);
            }

            if (!APIs::FCFW->SetMenuVisibility(SKSE::GetPluginHandle(), m_transitionToTarget_TimelineID, true)) {
                log::warn("{}: Could not set menu visibility", __FUNCTION__);
            }

            if (!APIs::FCFW->StartPlayback(SKSE::GetPluginHandle(), m_transitionToTarget_TimelineID,
                1.0f, false, false, false, 0.0f)) {
                log::warn("{}: Could not start playback", __FUNCTION__);
            }
        } else if (activeTimelineID == m_transitionToTarget_TimelineID || activeTimelineID == m_atTarget_TimelineID) {
            if (!UpdateTimeline3()) {
                log::warn("{}: Could not update timeline3", __FUNCTION__);
                return;
            }
            if (!APIs::FCFW->SwitchPlayback(SKSE::GetPluginHandle(), activeTimelineID, m_transitionToPrevious_TimelineID)) {
                log::warn("{}: Could not switch playback", __FUNCTION__);
            }

        } else if (activeTimelineID == m_transitionToPrevious_TimelineID) {
            if (!APIs::FCFW->SwitchPlayback(SKSE::GetPluginHandle(), activeTimelineID, m_transitionToTarget_TimelineID)) {
                log::warn("{}: Could not switch playback", __FUNCTION__);
            }
        } else {
            log::info("{}: FCFW is currently playing another timeline.", __FUNCTION__);
        }
    }

    void FreeCameraManager::ReturnToPrevious() {
        if (RE::UI::GetSingleton()->GameIsPaused()) {
            return;
        }

        if (!APIs::FCFW) {
            log::error("{}: FCFW API not available", __FUNCTION__);
            return;
        }

        auto activeTimelineID = APIs::FCFW->GetActiveTimelineID();
        if (activeTimelineID == m_transitionToTarget_TimelineID || activeTimelineID == m_atTarget_TimelineID) {
            if (!UpdateTimeline3()) {
                log::warn("{}: Could not update timeline3", __FUNCTION__);
                return;
            }
            if (!APIs::FCFW->SwitchPlayback(SKSE::GetPluginHandle(), activeTimelineID, m_transitionToPrevious_TimelineID)) {
                log::warn("{}: Could not switch playback", __FUNCTION__);
            }

        }
    }

    float FreeCameraManager::ComputeTransitionTime(RE::NiPoint3 a_targetPos) {
        if (!m_target) {
            log::warn("{}: No target to compute transition time to", __FUNCTION__);
            return 1.0f;
        }

        float minDistance = 2000.f;
        float maxDistance = 10000.f;
        float minTime = 0.5f;
        float maxTime = 2.0f;

        float distance = _ts_SKSEFunctions::GetCameraPos().GetDistance(a_targetPos);

        float relDistance = (distance - minDistance) / (maxDistance - minDistance);
        relDistance = std::clamp(relDistance, 0.0f, 1.0f);
        
        float transitionTime = minTime + (maxTime - minTime) * relDistance;

        return transitionTime;
    }
} // namespace SecondSight
