#include "FreeCameraManager.h"
#include "_ts_SKSEFunctions.h"
#include "APIManager.h"
#include "Offsets.h"

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

        if (!APIs::FCFW->RegisterPlugin(SKSE::GetPluginHandle())) {
            log::error("{}: Could not register SecondSight plugin with FCFW!", __FUNCTION__);
        }
    
        m_transitionToTarget_TimelineID = 0;
        m_atTarget_TimelineID = 0;
        m_transitionToPrevious_TimelineID = 0;

        // Register listener for FCFW timeline events
        if (!SKSE::GetMessagingInterface()->RegisterListener(FCFW_API::FCFWPluginName, SecondSight::FreeCameraManager::FCFWMessageHandler)) {
            log::warn("{}: Failed to register FCFW message listener", __FUNCTION__);
        }

        if (APIs::DTR)
        {
            // Register listener for DTR target reticle events
            if (!SKSE::GetMessagingInterface()->RegisterListener(DTR_API::DTRPluginName, SecondSight::FreeCameraManager::DTRMessageHandler)) {
                log::warn("{}: Failed to register DTR message listener", __FUNCTION__);
            }
        }
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
        // Check if coming from first person and player 3D is hidden
        auto* player = RE::PlayerCharacter::GetSingleton();
//        m_wasPlayerHiddenBeforeFreeCam = false;
        
auto* playerCamera = RE::PlayerCamera::GetSingleton();
        if (!playerCamera) {
            log::error("{}: PlayerCamera not available", __FUNCTION__);
            return false;
        }

        if (player && playerCamera->currentState && playerCamera->currentState->id == RE::CameraState::kFirstPerson) {
            auto player3D = player->Get3D();
            if (player3D) {
                // Check if player 3D is currently hidden (typical for first person)
  //              m_wasPlayerHiddenBeforeFreeCam = player3D->flags.any(RE::NiAVObject::Flag::kHidden);
                
 //               if (m_wasPlayerHiddenBeforeFreeCam) {
                    // Make player visible for free camera
                    auto flags = player3D->GetFlags();
//                    player3D->OnVisible();
//                    player3D->flags.reset(RE::NiAVObject::Flag::kHidden);
                    log::info("{}: Enabled player 3D visibility (was hidden in first person)", __FUNCTION__);
//                }
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

        m_isFreeCameraActive = false;
        ToggleFreeCamera();
    }

    void FreeCameraManager::UpdateTarget() {
        if (APIs::DTR && APIs::DTR->IsReticleActive()) {
            m_target = APIs::DTR->GetCurrentTarget();
        } else if (APIs::TrueDirectionalMovementV1 && APIs::TrueDirectionalMovementV1->GetTargetLockState()) {
            auto targetHandle = APIs::TrueDirectionalMovementV1->GetCurrentTarget();
            if (targetHandle) {
                m_target = targetHandle.get().get();
            } else {
                m_target = nullptr;
            }
        } else {
            m_target = _ts_SKSEFunctions::GetCrosshairTarget();
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
            log::error("{}: FCFW API not available, cannot update timeline", __FUNCTION__);
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
        if (IsPlaybackActive()) {
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

        m_offset = targetPoint->world.translate - m_target->GetPosition();
        m_offset.y += 20.f; // move 20 units into 'forward' direction to account for head dimensions

        m_previousCameraPos = _ts_SKSEFunctions::GetCameraPos();

        m_prevRotation.x = 0.0f;
        m_prevRotation.y = 0.0f;

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
            m_prevRotation.y = rotation.z; // yaw
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
        RE::BSTPoint2<float> rotationOffset = RE::BSTPoint2<float>(); // no offset

        int ret;
        ret = APIs::FCFW->AddTranslationPointAtCamera(handle, m_transitionToTarget_TimelineID, 0.0f, true, true);
        ret = APIs::FCFW->AddRotationPointAtCamera(handle, m_transitionToTarget_TimelineID, 0.f, true, true);
        ret = APIs::FCFW->AddRotationPointAtRef(handle, m_transitionToTarget_TimelineID, rotationToMovement_End, m_target, rotationOffset, false, true, true);
        ret = APIs::FCFW->AddRotationPointAtRef(handle, m_transitionToTarget_TimelineID, rotationToTarget_Start, m_target, rotationOffset, false, true, true);
        ret = APIs::FCFW->AddTranslationPointAtRef(handle, m_transitionToTarget_TimelineID, transitionTime, m_target, m_offset, true, true, true);
        ret = APIs::FCFW->AddRotationPointAtRef(handle, m_transitionToTarget_TimelineID, transitionTime, m_target, rotationOffset, true, true, true);
        ret = APIs::FCFW->SetPlaybackMode(handle, m_transitionToTarget_TimelineID, 2);

        return true;     
    }

    bool FreeCameraManager::UpdateTimeline2() { 
        if (!APIs::FCFW) {
            return false;
        }
        
        if (!InitializeTimeline(m_atTarget_TimelineID)) {
            return false;
        }

        SKSE::PluginHandle handle = SKSE::GetPluginHandle();
        RE::BSTPoint2<float> rotationOffset = RE::BSTPoint2<float>(); // no offset

        int ret;
        ret = APIs::FCFW->AddTranslationPointAtRef(handle, m_atTarget_TimelineID, 0.f, m_target, m_offset, true, true, true);
        ret = APIs::FCFW->AddRotationPointAtRef(handle, m_atTarget_TimelineID, 0.f, m_target, rotationOffset, true, true, true);
        ret = APIs::FCFW->SetPlaybackMode(handle, m_atTarget_TimelineID, 2);
        APIs::FCFW->AllowUserRotation(handle, m_atTarget_TimelineID, true);

        return true;     
    }

    bool FreeCameraManager::UpdateTimeline3() {   
        if (!APIs::FCFW) {
            return false;
        }

        if (!InitializeTimeline(m_transitionToPrevious_TimelineID)) {
            return false;
        }

        float transitionTime = ComputeTransitionTime(m_previousCameraPos);

        SKSE::PluginHandle handle = SKSE::GetPluginHandle();
        RE::BSTPoint2<float> rotationOffset = RE::BSTPoint2<float>(); // no offset
        
        int ret;
        ret = APIs::FCFW->AddTranslationPointAtCamera(handle, m_transitionToPrevious_TimelineID, 0.0f, true, true);
        ret = APIs::FCFW->AddRotationPointAtCamera(handle, m_transitionToPrevious_TimelineID, 0.f, true, true);
        ret = APIs::FCFW->AddRotationPointAtRef(handle, m_transitionToPrevious_TimelineID, 0.5f * transitionTime, m_target, rotationOffset, false, true, true);
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

        float heading = m_target->GetHeading(false);
        
        // Clamp pitch
        freeCameraState->rotation.x = _ts_SKSEFunctions::NormalRelativeAngle(freeCameraState->rotation.x);
        freeCameraState->rotation.x = std::clamp(freeCameraState->rotation.x, - 0.45f * PI, 0.4f * PI);
        
        // Clamp yaw
        float relativeYaw = _ts_SKSEFunctions::NormalRelativeAngle(freeCameraState->rotation.y - heading);
        relativeYaw = std::clamp(relativeYaw, -0.5f * PI, 0.5f * PI);
        freeCameraState->rotation.y = _ts_SKSEFunctions::NormalRelativeAngle(heading + relativeYaw);
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

            if (!APIs::FCFW->StartPlayback(SKSE::GetPluginHandle(), m_transitionToTarget_TimelineID,
                1.0f, false, false, false, 0.0f, true, 100.0f /*a_minHeightAboveGround*/, true /*a_showMenusDuringPlayback*/)) {
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
