#ifndef VULKANENGINE_INCLUDE_ENGINE_SCENE_COMPONENTS_ANIMATIONCONTROLLER_HPP
#define VULKANENGINE_INCLUDE_ENGINE_SCENE_COMPONENTS_ANIMATIONCONTROLLER_HPP

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "AnimationClip.hpp"
#include "AnimationGraph.hpp"
#include "ModelLib/Resources/Model.hpp"

namespace engine {

    /**
 * @brief Manages multiple animation clips with priority-based blending
 *
 * Supports:
 * - Multiple simultaneous clips (layered animation)
 * - OVERRIDE mode: higher priority wins on bone conflicts
 * - ADDITIVE mode: result = base + (clip - bindPose) * weight
 * - Crossfade between clips (weight-based)
 * - Animation events at specific timestamps
 */
    class AnimationController {
       public:
        using EventCallback = std::function<void(const std::string& eventName, void* userData)>;

        /**
     * @brief Add a clip to the controller (plays immediately if play=true)
     * @param clipIndex Index into Model::animations
     * @param model Model containing the animation data
     * @param priority Priority level (0-100, higher wins)
     * @param play Whether to start playing immediately
     */
        void addClip(int clipIndex, const Model& model, int priority = 0, bool play = true);

        /**
     * @brief Remove a clip by its index
     */
        void removeClip(int clipIndex);

        /**
     * @brief Set the weight (opacity) of a specific clip
     * @param clipIndex Clip index
     * @param weight 0.0 (gone) to 1.0 (full)
     */
        void setClipWeight(int clipIndex, float weight);

        /**
     * @brief Set the playback speed of a specific clip
     */
        void setClipSpeed(int clipIndex, float speed);

        /**
     * @brief Set blend mode (OVERRIDE or ADDITIVE)
     */
        void setBlendMode(int clipIndex, AnimationClip::Mode mode);

        /**
     * @brief Set blend mode for all clips
     */
        void setDefaultBlendMode(AnimationClip::Mode mode) {
            defaultMode_ = mode;
        }

        /**
     * @brief Set the absolute playback time of a specific clip
     * @param clipIndex Clip index
     * @param time Time in seconds (clamped to [0, duration])
     */
        void setClipTime(int clipIndex, float time);

        /**
     * @brief Convenience: play a clip (clears existing, starts fresh)
     * @param clipIndex Index into Model::animations
     * @param model Model containing the animation data
     */
        void play(int clipIndex, const Model& model) {
            stopAll();
            addClip(clipIndex, model, 0, true);
        }

        /**
     * @brief Stop a specific clip
     */
        void stop(int clipIndex);

        /**
     * @brief Stop all clips
     */
        void stopAll();

        /**
     * @brief Reset all clips to initial state
     */
        void reset();

        /**
     * @brief Core update: step all clips, fire events, compute bone transforms
     * @param deltaTime Time elapsed since last frame (seconds)
     * @param model Model containing animation data
     * @param[out] outTranslations Output bone translations (size must be >= node count)
     * @param[out] outRotations Output bone rotations (size must be >= node count)
     * @param[out] outScales Output bone scales (size must be >= node count)
     * @return Events that were fired this frame (drained from all clips)
     */
        std::vector<std::pair<std::string, void*>> update(float deltaTime, const Model& model,
            std::vector<glm::vec3>& outTranslations,
            std::vector<glm::quat>& outRotations,
            std::vector<glm::vec3>& outScales);

        /**
     * @brief Compute final global bone transforms from all active clips
     * @param model Model containing animation data
     * @param baseTransforms Per-bone bind pose / base transforms (from lowest-priority clip or identity)
     * @return Vector of global transforms ready for rendering
     */
        std::vector<glm::mat4> computeGlobalTransforms(const Model& model,
            const std::vector<glm::mat4>&                           baseTransforms) const;

        /**
     * @brief Set callback for animation events
     */
        void setEventCallback(EventCallback cb) {
            eventCallback_ = std::move(cb);
        }

        /**
     * @brief Query whether any clips are currently active
     */
        [[nodiscard]] bool hasActiveClips() const {
            for (const auto& clip : clips_) {
                if (clip.active && clip.weight > 0.0f)
                    return true;
            }
            return false;
        }

        /**
     * @brief Get reference to all clips (for UI iteration)
     */
        std::vector<AnimationClip>& getClips() {
            return clips_;
        }
        const std::vector<AnimationClip>& getClips() const {
            return clips_;
        }

        /**
     * @brief Set the animation graph for this controller
     */
        void setGraph(std::shared_ptr<AnimationGraph> graph);

        /**
     * @brief Check if this controller has an associated graph
     */
        bool hasGraph() const {
            return graph_ != nullptr;
        }

        /**
     * @brief Get the graph
     */
        std::shared_ptr<AnimationGraph> getGraph() const {
            return graph_;
        }

        /**
     * @brief Trigger a transition to a target node
     */
        void triggerTransition(int targetNodeId);

        /**
     * @brief Handle an event — check if any EVENT_BASED transitions fire
     * @return true if a transition was triggered
     */
        bool handleEvent(const std::string& eventName);

        /**
     * @brief Get current graph node (if any)
     */
        const AnimationGraphNode* getCurrentGraphNode() const;

        /**
     * @brief Get current graph node name (for display)
     */
        std::string getCurrentGraphNodeName() const;

        /**
     * @brief Check if a transition is in progress (crossfading)
     */
        bool isTransitioning() const {
            return transitioningToNode_ != -1;
        }

        /**
     * @brief Get clip by index, or nullptr if not found
     */
        AnimationClip* getClip(int clipIndex) {
            for (auto& clip : clips_) {
                if (clip.clipIndex == clipIndex)
                    return &clip;
            }
            return nullptr;
        }

        const AnimationClip* getClip(int clipIndex) const {
            return const_cast<AnimationController*>(this)->getClip(clipIndex);
        }

        /**
     * @brief Drain and return all events fired this frame
     */
        std::vector<std::pair<std::string, void*>> takeEvents() {
            std::vector<std::pair<std::string, void*>> result = std::move(firedEvents_);
            return result;
        }

        /**
     * @brief Clear all fired events without returning them
     */
        void clearEvents() {
            firedEvents_.clear();
        }

       private:
        std::vector<AnimationClip> clips_;
        EventCallback              eventCallback_;
        AnimationClip::Mode        defaultMode_{AnimationClip::OVERRIDE};

        std::vector<std::pair<std::string, void*>> firedEvents_;

        std::shared_ptr<AnimationGraph> graph_;
        int                             currentGraphNodeId_{-1};
        int                             transitioningToNode_{-1};
        float                           transitionTimer_{0.0f};

        void applyClipToAccumulators(const AnimationClip& clip, const Model& model,
            std::vector<glm::vec3>& outTranslations,
            std::vector<glm::quat>& outRotations,
            std::vector<glm::vec3>& outScales,
            bool                    isFirst) const;

        int findDominantClipForBone(int boneIndex) const;
    };

}  // namespace engine

#endif
