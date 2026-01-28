namespace SecondSight {
    
    class FreeCameraManager {
        public:
            static FreeCameraManager& GetSingleton() {
                static FreeCameraManager instance;
                return instance;
            }
            FreeCameraManager(const FreeCameraManager&) = delete;
            FreeCameraManager& operator=(const FreeCameraManager&) = delete;

            static void FCFWMessageHandler(SKSE::MessagingInterface::Message* a_msg);

            void Initialize();

            void Update();

            void StartSecondSightEffect(RE::Actor* a_actor);

            void StopSecondSightEffect();

        private:
            FreeCameraManager() = default;
            ~FreeCameraManager() = default;

            void ToggleFreeCamera();

            float ComputeTransitionTime(RE::NiPoint3 a_targetPos);

            void ReturnToPrevious();

            bool UpdateTimeline1();
            bool UpdateTimeline2();
            bool UpdateTimeline3();

            bool InitializePlayback();
            bool InitializeTimeline(size_t& a_timelineID);

            bool IsPlaybackActive() const;

            void ClampFreeRotation();

            RE::NiPointer<RE::NiAVObject> GetCameraAnchorPoint();

            // members
            RE::CameraState m_previousCameraState;
            RE::NiPoint3 m_previousCameraPos;
            RE::NiPoint2 m_prevFreeRotation;
            RE::Actor* m_target = nullptr;
            RE::BSTPoint2<float> m_prevRotation;
            RE::NiPoint3 m_offset;
            bool m_useReticleTarget = false;
            bool m_isFreeCameraActive = false;

            size_t m_transitionToTarget_TimelineID = 0;
            size_t m_atTarget_TimelineID = 0;
            size_t m_transitionToPrevious_TimelineID = 0;
    }; // class FreeCameraManager
} // namespace SecondSight
