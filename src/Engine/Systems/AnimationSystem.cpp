#include "Engine/Systems/AnimationSystem.hpp"

#include <algorithm>
#include <exception>
#include <iostream>
#include <memory>
#include <utility>
#include <vector>

#include "Engine/Core/Logger.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Scene/Animation/AnimationController.hpp"
#include "Engine/Scene/components/AnimationComponent.hpp"
#include "Engine/Scene/components/ModelComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"

#include "ModelLib/Resources/MorphTargetManager.hpp"
#include "glm/common.hpp"
#include "glm/gtc/quaternion.hpp"
namespace engine {
    AnimationSystem::AnimationSystem(Device& device) : device_(device) {
        try {
            morphManager_ = std::make_unique<MorphTargetManager>(device);
            engine::Logger::info(engine::LogChannel::Render, "[AnimationSystem] Initialized successfully");
        } catch (const std::exception& e) {
            engine::Logger::error(engine::LogChannel::Render, "[AnimationSystem] ERROR: ", e.what());
            throw;
        }
    }
    AnimationSystem::~AnimationSystem() = default;
    void AnimationSystem::update(FrameInfo& frameInfo) {
        updateAnimations(frameInfo);
        updateMorphTargets(frameInfo);
    }
    void AnimationSystem::updateAnimations(FrameInfo& frameInfo) {
        auto view = frameInfo.scene->getRegistry().view<AnimationComponent, TransformComponent>();
        for (auto entity : view) {
            auto [anim, transform] = view.get<AnimationComponent, TransformComponent>(entity);
            if (!anim.model)
                continue;
            if (anim.graph) {
                anim.graph->step(frameInfo.frameTime);
            }
            if (!anim.controller) {
                anim.controller = std::make_shared<AnimationController>();
            }
            if (!anim.controller->hasActiveClips()) {
                if (anim.isPlaying && anim.currentAnimationIndex >= 0) {
                    anim.controller->play(anim.currentAnimationIndex, *anim.model);
                } else {
                    continue;
                }
            }
            auto&                  nodes = anim.model->getNodes();
            std::vector<glm::vec3> localTrans(nodes.size(), glm::vec3(0.0f));
            std::vector<glm::quat> localRot(nodes.size(), glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
            std::vector<glm::vec3> localScale(nodes.size(), glm::vec3(1.0f));
            anim.controller->update(frameInfo.frameTime, *anim.model,
                localTrans, localRot, localScale);
            anim.isPlaying = anim.controller->hasActiveClips();
            for (size_t i = 0; i < nodes.size(); ++i) {
                nodes[i].translation = localTrans[i];
                nodes[i].rotation    = localRot[i];
                nodes[i].scale       = localScale[i];
            }
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
                if (!morphManager_->isModelInitialized(modelComp.model.get())) {
                    try {
                        morphManager_->initializeModel(modelComp.model);
                    } catch (const std::exception& e) {
                        engine::Logger::error(engine::LogChannel::Render, "[AnimationSystem] ERROR initializing morph for object ", (uint32_t) entity, ": ", e.what());
                        continue;
                    }
                }
                morphManager_->updateAndBlend(frameInfo.commandBuffer, modelComp.model);
            }
        }
    }
    void AnimationSystem::updateNodeTransforms(AnimationComponent& animComp, const Model::Animation& animation) {
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
        float const        t0          = sampler.times[prevIndex];
        float const        t1          = sampler.times[nextIndex];
        float const        factor      = (time - t0) / (t1 - t0);
        const auto&        prevWeights = sampler.morphWeights[prevIndex];
        const auto&        nextWeights = sampler.morphWeights[nextIndex];
        std::vector<float> result(prevWeights.size());
        for (size_t i = 0; i < prevWeights.size(); i++) {
            result[i] = (prevWeights[i] * (1.0f - factor)) + (nextWeights[i] * factor);
        }
        return result;
    }
}  // namespace engine
