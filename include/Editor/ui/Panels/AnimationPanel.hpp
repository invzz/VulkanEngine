#ifndef EDITOR_ANIMATIONPANEL_HPP
#define EDITOR_ANIMATIONPANEL_HPP

#include <string>
#include <vector>

#include "Engine/Scene/Scene.hpp"

#include "Editor/ui/AnimationGraphEditor.hpp"
#include "Editor/ui/UIPanel.hpp"

namespace engine {

    /**
 * @brief State for the timeline widget
 */
    struct TimelineState {
        float totalDuration{0.0f};
        float currentTime{0.0f};
        float zoom{1.0f};
        bool  isScrubbing{false};
        float clipWidth{200.0f};
        int   selectedClip{-1};
    };

    /**
 * @brief Panel for animation controls
 */
    class AnimationPanel : public UIPanel {
       public:
        explicit AnimationPanel(Scene& scene);

        void render(FrameInfo& frameInfo) override;

       private:
        Scene& scene_;

        TimelineState timeline_;
        std::string   timelineSearchFilter_;

        struct EntityTrackData {
            std::string name;
            float       duration{0.0f};
            float       startOffset{0.0f};
            float       weight{1.0f};
            float       crossfadeDuration{0.0f};
            float       speed{1.0f};
            bool        selected{false};

            std::vector<float> keyframeTimes;

            std::vector<bool> boneVisible;
        };

        struct EntityAnimData {
            uint32_t                     entityId;
            bool                         expanded{true};
            std::vector<std::string>     clipNames;
            std::vector<float>           clipDuration;
            std::vector<float>           clipCurrentTime;
            int                          selectedClipIndex{-1};
            float                        selectedClipWeight{1.0f};
            float                        selectedClipSpeed{1.0f};
            float                        selectedCrossfadeDuration{0.0f};
            std::vector<EntityTrackData> tracks;
        };

        std::vector<EntityAnimData> entityAnimData_;

        ui::AnimationGraphEditor graphEditor_;
        bool                     showGraphEditor_{false};

        void updateTimelineState();
        void renderTimeline();
        void renderEntityAnimations();
        void renderClipControls(EntityAnimData& data, uint32_t entityId, int clipIndex);
        void renderGraphEditor();
    };

}  // namespace engine

#endif
