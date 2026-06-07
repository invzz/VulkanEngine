#ifndef EDITOR_ANIMATIONPANEL_HPP
#define EDITOR_ANIMATIONPANEL_HPP

#include <vector>
#include <string>
#include <glm/glm.hpp>

#include "Engine/Scene/Scene.hpp"
#include "Editor/ui/UIPanel.hpp"

namespace engine {

/**
 * @brief State for the timeline widget
 */
struct TimelineState {
    float totalDuration{0.0f};        // Total timeline duration
    float currentTime{0.0f};          // Playhead position
    float zoom{1.0f};                 // Pixels per second
    bool isScrubbing{false};          // Mouse is dragging the playhead
    float clipWidth{200.0f};          // Width of clip tracks
    int selectedClip{-1};             // Currently selected clip index (-1 = none)
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

    // Timeline
    TimelineState timeline_;
    std::string timelineSearchFilter_;

    // Per-track animation state for timeline rendering
    struct EntityTrackData {
        std::string name;
        float duration{0.0f};
        float startOffset{0.0f};       // When the clip starts relative to timeline start
        float weight{1.0f};
        float crossfadeDuration{0.0f}; // Crossfade duration in seconds
        float speed{1.0f};
        bool selected{false};
        // Keyframes for visualization
        std::vector<float> keyframeTimes;
        // Per-bone visibility toggle state
        std::vector<bool> boneVisible;
    };

    // Per-entity animation data for display
    struct EntityAnimData {
        uint32_t entityId;
        bool expanded{true};
        std::vector<std::string> clipNames;       // Names of active clips on this model
        std::vector<float> clipDuration;          // Duration of each clip
        std::vector<float> clipCurrentTime;       // Current playback time of each clip
        int selectedClipIndex{-1};
        float selectedClipWeight{1.0f};
        float selectedClipSpeed{1.0f};
        float selectedCrossfadeDuration{0.0f};   // Crossfade duration for selected clip
        std::vector<EntityTrackData> tracks;
    };

    std::vector<EntityAnimData> entityAnimData_;

    // Timeline helpers
    void updateTimelineState();
    void renderTimeline();
    void renderEntityAnimations();
    void renderClipControls(EntityAnimData& data, uint32_t entityId, int clipIndex);
};

}  // namespace engine

#endif  // EDITOR_ANIMATIONPANEL_HPP
