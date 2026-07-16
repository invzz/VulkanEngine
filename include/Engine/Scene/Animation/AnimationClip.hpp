#ifndef VULKANENGINE_INCLUDE_ENGINE_SCENE_COMPONENTS_ANIMATIONCLIP_HPP
#define VULKANENGINE_INCLUDE_ENGINE_SCENE_COMPONENTS_ANIMATIONCLIP_HPP
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <string>
#include <vector>

#include "ModelLib/Resources/Model.hpp"
namespace engine {
    /**
 * @brief Lightweight handle for a single animation clip playback
 *
 * Tracks time, speed, weight, and events for one clip.
 * Does NOT own the Model — it references a clip by index.
 */
    struct AnimationClip {
        int         clipIndex{-1};
        std::string name;
        float       duration{0.0f};
        float       currentTime{0.0f};
        float       speed{1.0f};
        float       weight{1.0f};
        bool        loop{true};
        bool        active{false};
        enum Mode : uint8_t {
            OVERRIDE,
            ADDITIVE,
        };
        Mode mode{OVERRIDE};
        int  priority{0};
        struct Event {
            float       time{0.0f};
            std::string name;
            void*       userData{nullptr};
        };
        std::vector<Event> events;
        size_t             nextEventIndex{0};
        /** Reset clip to initial state */
        void reset() {
            currentTime    = 0.0f;
            active         = false;
            weight         = 1.0f;
            nextEventIndex = 0;
        }
        /**
     * @brief Step this clip forward by deltaTime and update bone transforms
     * @param deltaTime Time elapsed since last frame (seconds)
     * @param model Model containing the animation data
     * @param[out] outTranslations Accumulated per-bone translations
     * @param[out] outRotations Accumulated per-bone rotations
     * @param[out] outScales Accumulated per-bone scales
     */
        void step(float deltaTime, const Model& model,
            std::vector<glm::vec3>& outTranslations,
            std::vector<glm::quat>& outRotations,
            std::vector<glm::vec3>& outScales);
        /**
     * @brief Check if this clip has finished playing (non-looping)
     */
        [[nodiscard]] bool isFinished() const {
            return active && !loop && currentTime >= duration;
        }
        /**
     * @brief Return true if this clip has any events queued
     */
        [[nodiscard]] bool hasEvents() const {
            return !events.empty() && nextEventIndex < events.size();
        }
        /**
     * @brief Drain all events that have been fired (time <= currentTime)
     * @param firedEvents Output vector to append (name, userData) pairs
     */
        void drainFiredEvents(std::vector<std::pair<std::string, void*>>& firedEvents) {
            while (nextEventIndex < events.size() && events[nextEventIndex].time <= currentTime) {
                firedEvents.emplace_back(events[nextEventIndex].name, events[nextEventIndex].userData);
                ++nextEventIndex;
            }
        }
    };
}  // namespace engine
#endif
