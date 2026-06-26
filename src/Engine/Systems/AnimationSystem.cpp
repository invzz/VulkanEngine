#include "Engine/Systems/AnimationSystem.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>
#include <utility>
#include <vector>

#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Scene/Components/AnimationController.hpp"
#include "Engine/Scene/components/AnimationComponent.hpp"
#include "Engine/Scene/components/ModelComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"

#include "ModelLib/Resources/MorphTargetManager.hpp"
#include "glm/common.hpp"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/gtc/quaternion.hpp"

namespace engine {

    AnimationSystem::AnimationSystem(Device& device) : device_(device) {
        try {
            morphManager_ = std::make_unique<MorphTargetManager>(device);
            std::cout << "[AnimationSystem] Initialized successfully" << '\n';
        } catch (const std::exception& e) {
            std::cerr << "[AnimationSystem] ERROR: " << e.what() << '\n';
            throw;
        }
    }

    AnimationSystem::~AnimationSystem() = default;

    void AnimationSystem::update(FrameInfo& frameInfo) {
        // Step 1: Update animation components (CPU-side: interpolate
        // weights/transforms)
        updateAnimations(frameInfo);

        // Step 2: Dispatch morph target compute shaders (GPU-side: blend vertices)
        updateMorphTargets(frameInfo);
    }

    void AnimationSystem::updateAnimations(FrameInfo& frameInfo) {
        auto view = frameInfo.scene->getRegistry().view<AnimationComponent, TransformComponent>();

        for (auto entity : view) {
            auto [anim, transform] = view.get<AnimationComponent, TransformComponent>(entity);

            if (!anim.model)
                continue;

            // Step animation graph if present
            if (anim.graph) {
                anim.graph->step(frameInfo.frameTime);
            }

            // Lazy-create controller if needed
            if (!anim.controller) {
                anim.controller = std::make_shared<AnimationController>();
            }

            // If no active clips, check legacy single-clip mode
            if (!anim.controller->hasActiveClips()) {
                if (anim.isPlaying && anim.currentAnimationIndex >= 0) {
                    // Legacy: create controller and play the current animation
                    anim.controller->play(anim.currentAnimationIndex, *anim.model);
                } else {
                    continue;
                }
            }

            // Prepare per-bone accumulators
            auto&                  nodes = anim.model->getNodes();
            std::vector<glm::vec3> localTrans(nodes.size(), glm::vec3(0.0f));
            std::vector<glm::quat> localRot(nodes.size(), glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
            std::vector<glm::vec3> localScale(nodes.size(), glm::vec3(1.0f));

            // Update controller — steps time, fires events, accumulates bone transforms
            anim.controller->update(frameInfo.frameTime, *anim.model,
                localTrans, localRot, localScale);

            // Sync isPlaying state from controller
            anim.isPlaying = anim.controller->hasActiveClips();

            // Apply local transforms to model nodes
            for (size_t i = 0; i < nodes.size(); ++i) {
                nodes[i].translation = localTrans[i];
                nodes[i].rotation    = localRot[i];
                nodes[i].scale       = localScale[i];
            }

            // Compute global transforms (reusing existing helper)
            anim.nodeTransforms.resize(nodes.size(), glm::mat4(1.0f));
            for (size_t i = 0; i < nodes.size(); ++i) {
                bool isRoot = true;
                for (const auto& n : nodes) {
                    if (std::ranges::find(n.children, static_cast<int>(i)) != n.children.end()) {
                        isRoot = false;
                        break;
                    }
                }
                if (isRoot) {
                    computeGlobalTransforms(anim, static_cast<int>(i), glm::mat4(1.0f));
                }
            }

            // Apply root node transform to entity's TransformComponent
            int rootNodeIndex = -1;
            for (size_t i = 0; i < nodes.size(); ++i) {
                bool isRoot = true;
                for (const auto& n : nodes) {
                    if (std::ranges::find(n.children, static_cast<int>(i)) != n.children.end()) {
                        isRoot = false;
                        break;
                    }
                }
                if (isRoot) {
                    rootNodeIndex = static_cast<int>(i);
                    break;
                }
            }

            if (rootNodeIndex >= 0 && rootNodeIndex < static_cast<int>(nodes.size())) {
                transform.translation = nodes[static_cast<size_t>(rootNodeIndex)].translation;
                transform.rotation    = glm::eulerAngles(nodes[static_cast<size_t>(rootNodeIndex)].rotation);
                transform.scale       = nodes[static_cast<size_t>(rootNodeIndex)].scale * transform.baseScale;
            }
        }
    }

    void AnimationSystem::updateMorphTargets(FrameInfo& frameInfo) {
        if (!morphManager_) {
            return;
        }

        auto view = frameInfo.scene->getRegistry().view<ModelComponent>();
        for (auto entity : view) {
            auto& modelComp = view.get<ModelComponent>(entity);

            if (modelComp.model && modelComp.model->hasMorphTargets()) {
                // Initialize GPU buffers for new models
                if (!morphManager_->isModelInitialized(modelComp.model.get())) {
                    try {
                        morphManager_->initializeModel(modelComp.model);
                    } catch (const std::exception& e) {
                        std::cerr << "[AnimationSystem] ERROR initializing morph for object " << (uint32_t) entity << ": " << e.what() << '\n';
                        continue;
                    }
                }

                // Dispatch compute shader
                morphManager_->updateAndBlend(frameInfo.commandBuffer, modelComp.model);
            }
        }
    }

    void AnimationSystem::updateNodeTransforms(AnimationComponent& animComp, const Model::Animation& animation) {
        // Apply animation to nodes
        auto& nodes = animComp.model->getNodes();

        for (const auto& channel : animation.channels) {
            if (channel.targetNode < 0 || std::cmp_greater_equal(channel.targetNode, nodes.size())) {
                continue;
            }

            const auto& sampler = animation.samplers[channel.samplerIndex];
            auto&       node    = nodes[channel.targetNode];

            switch (channel.path) {
                case Model::AnimationChannel::TRANSLATION:
                    node.translation = interpolateVec3(sampler, animComp.currentTime);
                    break;
                case Model::AnimationChannel::ROTATION:
                    node.rotation = interpolateQuat(sampler, animComp.currentTime);
                    break;
                case Model::AnimationChannel::SCALE:
                    node.scale = interpolateVec3(sampler, animComp.currentTime);
                    break;
                case Model::AnimationChannel::WEIGHTS:
                    node.morphWeights = interpolateMorphWeights(sampler, animComp.currentTime);
                    break;
            }
        }

        // Recompute global transforms
        for (size_t i = 0; i < nodes.size(); i++) {
            bool isRoot = true;
            for (auto& node : nodes) {
                const auto& children = node.children;
                if (std::ranges::find(children, static_cast<int>(i)) != children.end()) {
                    isRoot = false;
                    break;
                }
            }

            if (isRoot) {
                computeGlobalTransforms(animComp, static_cast<int>(i), glm::mat4(1.0f));
            }
        }
    }

    void AnimationSystem::computeGlobalTransforms(AnimationComponent& animComp, int nodeIndex, const glm::mat4& parentTransform) {
        if (nodeIndex < 0 || std::cmp_greater_equal(nodeIndex, animComp.model->getNodes().size())) {
            return;
        }

        const auto&     node           = animComp.model->getNodes()[nodeIndex];
        glm::mat4 const localTransform = node.getLocalTransform();

        if (std::cmp_less(nodeIndex, animComp.nodeTransforms.size())) {
            animComp.nodeTransforms[nodeIndex] = parentTransform * localTransform;
        }

        for (int const childIdx : node.children) {
            computeGlobalTransforms(animComp, childIdx, animComp.nodeTransforms[nodeIndex]);
        }
    }

    glm::vec3 AnimationSystem::interpolateVec3(const Model::AnimationSampler& sampler, float time) {
        if (sampler.times.empty() || sampler.translations.empty()) {
            return glm::vec3(0.0f);
        }

        if (time <= sampler.times.front())
            return sampler.translations.front();
        if (time >= sampler.times.back())
            return sampler.translations.back();

        size_t nextIndex = 0;
        for (size_t i = 0; i < sampler.times.size() - 1; i++) {
            if (time >= sampler.times[i] && time < sampler.times[i + 1]) {
                nextIndex = i + 1;
                break;
            }
        }

        size_t const prevIndex = nextIndex - 1;

        if (sampler.interpolation == Model::AnimationSampler::STEP) {
            return sampler.translations[prevIndex];
        }

        float const t0     = sampler.times[prevIndex];
        float const t1     = sampler.times[nextIndex];
        float const factor = (time - t0) / (t1 - t0);

        return glm::mix(sampler.translations[prevIndex], sampler.translations[nextIndex], factor);
    }

    glm::quat AnimationSystem::interpolateQuat(const Model::AnimationSampler& sampler, float time) {
        if (sampler.times.empty() || sampler.rotations.empty()) {
            return {1.0f, 0.0f, 0.0f, 0.0f};
        }

        if (time <= sampler.times.front())
            return sampler.rotations.front();
        if (time >= sampler.times.back())
            return sampler.rotations.back();

        size_t nextIndex = 0;
        for (size_t i = 0; i < sampler.times.size() - 1; i++) {
            if (time >= sampler.times[i] && time < sampler.times[i + 1]) {
                nextIndex = i + 1;
                break;
            }
        }

        size_t const prevIndex = nextIndex - 1;

        if (sampler.interpolation == Model::AnimationSampler::STEP) {
            return sampler.rotations[prevIndex];
        }

        float const t0     = sampler.times[prevIndex];
        float const t1     = sampler.times[nextIndex];
        float const factor = (time - t0) / (t1 - t0);

        return glm::slerp(sampler.rotations[prevIndex], sampler.rotations[nextIndex], factor);
    }

    std::vector<float> AnimationSystem::interpolateMorphWeights(const Model::AnimationSampler& sampler, float time) {
        if (sampler.times.empty() || sampler.morphWeights.empty()) {
            return {};
        }

        if (time <= sampler.times.front())
            return sampler.morphWeights.front();
        if (time >= sampler.times.back())
            return sampler.morphWeights.back();

        size_t nextIndex = 0;
        for (size_t i = 0; i < sampler.times.size() - 1; i++) {
            if (time >= sampler.times[i] && time < sampler.times[i + 1]) {
                nextIndex = i + 1;
                break;
            }
        }

        size_t const prevIndex = nextIndex - 1;

        if (sampler.interpolation == Model::AnimationSampler::STEP) {
            return sampler.morphWeights[prevIndex];
        }

        float const t0     = sampler.times[prevIndex];
        float const t1     = sampler.times[nextIndex];
        float const factor = (time - t0) / (t1 - t0);

        const auto&        prevWeights = sampler.morphWeights[prevIndex];
        const auto&        nextWeights = sampler.morphWeights[nextIndex];
        std::vector<float> result(prevWeights.size());

        for (size_t i = 0; i < prevWeights.size(); i++) {
            result[i] = (prevWeights[i] * (1.0f - factor)) + (nextWeights[i] * factor);
        }

        return result;
    }

}  // namespace engine
