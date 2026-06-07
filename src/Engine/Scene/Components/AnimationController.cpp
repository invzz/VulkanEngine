#include "Engine/Scene/Components/AnimationController.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace engine {

// ── AnimationClip ─────────────────────────────────────────────────

// (AnimationClip::step is defined inline in the header as it has no
// cross-file dependencies beyond Model.hpp)

// ── AnimationController ───────────────────────────────────────────

void AnimationController::addClip(int clipIndex, const Model& model,
                                   int priority, bool play) {
    // Check if a clip with this index already exists
    for (auto& clip : clips_) {
        if (clip.clipIndex == clipIndex) {
            clip.reset();
            clip.clipIndex = clipIndex;
            clip.priority  = priority;
            clip.mode      = defaultMode_;
            if (play) {
                clip.active = true;
                clip.loop   = true;
            }
            return;
        }
    }

    // Find the animation to get its name and duration
    AnimationClip newClip;
    newClip.clipIndex = clipIndex;
    if (clipIndex >= 0 && clipIndex < static_cast<int>(model.getAnimations().size())) {
        const auto& anim = model.getAnimations()[static_cast<size_t>(clipIndex)];
        newClip.name      = anim.name;
        newClip.duration  = anim.duration;
        // Events are not part of glTF data; add them separately via UI/API later
    }
    newClip.priority = priority;
    newClip.mode     = defaultMode_;
    if (play) {
        newClip.active = true;
        newClip.loop   = true;
    }
    clips_.push_back(std::move(newClip));
}

void AnimationController::removeClip(int clipIndex) {
    clips_.erase(std::remove_if(clips_.begin(), clips_.end(),
        [clipIndex](const AnimationClip& c) { return c.clipIndex == clipIndex; }),
        clips_.end());
}

void AnimationController::setClipWeight(int clipIndex, float weight) {
    for (auto& clip : clips_) {
        if (clip.clipIndex == clipIndex) {
            clip.weight = std::max(0.0f, std::min(1.0f, weight));
            return;
        }
    }
}

void AnimationController::setClipSpeed(int clipIndex, float speed) {
    for (auto& clip : clips_) {
        if (clip.clipIndex == clipIndex) {
            clip.speed = std::max(0.01f, speed);
            return;
        }
    }
}

void AnimationController::stop(int clipIndex) {
    for (auto& clip : clips_) {
        if (clip.clipIndex == clipIndex) {
            clip.active   = false;
            clip.weight   = 0.0f;
            clip.currentTime = 0.0f;
            clip.nextEventIndex = 0;
            return;
        }
    }
}

void AnimationController::stopAll() {
    for (auto& clip : clips_) {
        clip.active = false;
        clip.weight = 0.0f;
        clip.currentTime = 0.0f;
        clip.nextEventIndex = 0;
    }
}

void AnimationController::setClipTime(int clipIndex, float time) {
    for (auto& clip : clips_) {
        if (clip.clipIndex == clipIndex) {
            float duration = clip.duration > 0.0f ? clip.duration : 1.0f;
            clip.currentTime = std::max(0.0f, std::min(time, duration));
            // Reset event index so events replay from the new position
            clip.nextEventIndex = 0;
            for (size_t i = 0; i < clip.events.size(); ++i) {
                if (clip.events[i].time > clip.currentTime) {
                    clip.nextEventIndex = i;
                    break;
                }
            }
            return;
        }
    }
}

void AnimationController::reset() {
    for (auto& clip : clips_) {
        clip.reset();
    }
    firedEvents_.clear();
}

void AnimationController::applyClipToAccumulators(
    const AnimationClip& clip, const Model& model,
    std::vector<glm::vec3>& outTranslations,
    std::vector<glm::quat>& outRotations,
    std::vector<glm::vec3>& outScales,
    bool isFirst) const {

    if (!clip.active || clip.weight <= 0.0f) return;

    if (clip.clipIndex < 0 || clip.clipIndex >= static_cast<int>(model.getAnimations().size())) return;

    const auto& animation = model.getAnimations()[static_cast<size_t>(clip.clipIndex)];
    const auto& samplers  = animation.samplers;
    const auto& channels  = animation.channels;
    const auto& nodes     = model.getNodes();

    for (const auto& channel : channels) {
        if (channel.targetNode < 0 || channel.targetNode >= static_cast<int>(nodes.size())) continue;
        if (channel.samplerIndex < 0 || channel.samplerIndex >= static_cast<int>(samplers.size())) continue;

        const auto& sampler = samplers[static_cast<size_t>(channel.samplerIndex)];

        switch (channel.path) {
            case Model::AnimationChannel::TRANSLATION: {
                glm::vec3 translated = glm::vec3(0.0f);
                if (sampler.translations.empty()) break;

                if (clip.currentTime <= sampler.times.front())
                    translated = sampler.translations.front();
                else if (clip.currentTime >= sampler.times.back())
                    translated = sampler.translations.back();
                else {
                    size_t nextIdx = 0;
                    for (size_t i = 0; i < sampler.times.size() - 1; ++i) {
                        if (clip.currentTime >= sampler.times[i] && clip.currentTime < sampler.times[i + 1]) {
                            nextIdx = i + 1;
                            break;
                        }
                    }
                    size_t prevIdx = nextIdx - 1;
                    float factor = (clip.currentTime - sampler.times[prevIdx]) /
                                   (sampler.times[nextIdx] - sampler.times[prevIdx]);

                    if (sampler.interpolation == Model::AnimationSampler::STEP)
                        translated = sampler.translations[prevIdx];
                    else
                        translated = glm::mix(sampler.translations[prevIdx],
                                              sampler.translations[nextIdx], factor);
                }

                // Apply weight
                translated *= clip.weight;

                if (isFirst || clip.mode == AnimationClip::OVERRIDE) {
                    // Override mode: replace (but apply weight)
                    if (outTranslations.size() > static_cast<size_t>(channel.targetNode)) {
                        outTranslations[static_cast<size_t>(channel.targetNode)] = translated;
                    }
                } else {
                    // ADDITIVE: accumulate on top of existing
                    if (outTranslations.size() > static_cast<size_t>(channel.targetNode)) {
                        outTranslations[static_cast<size_t>(channel.targetNode)] += translated;
                    }
                }
                break;
            }

            case Model::AnimationChannel::ROTATION: {
                glm::quat rotated{1.0f, 0.0f, 0.0f, 0.0f};
                if (sampler.rotations.empty()) break;

                if (clip.currentTime <= sampler.times.front())
                    rotated = sampler.rotations.front();
                else if (clip.currentTime >= sampler.times.back())
                    rotated = sampler.rotations.back();
                else {
                    size_t nextIdx = 0;
                    for (size_t i = 0; i < sampler.times.size() - 1; ++i) {
                        if (clip.currentTime >= sampler.times[i] && clip.currentTime < sampler.times[i + 1]) {
                            nextIdx = i + 1;
                            break;
                        }
                    }
                    size_t prevIdx = nextIdx - 1;
                    float factor = (clip.currentTime - sampler.times[prevIdx]) /
                                   (sampler.times[nextIdx] - sampler.times[prevIdx]);

                    if (sampler.interpolation == Model::AnimationSampler::STEP)
                        rotated = sampler.rotations[prevIdx];
                    else
                        rotated = glm::normalize(glm::slerp(sampler.rotations[prevIdx],
                                                            sampler.rotations[nextIdx], factor));
                }

                // Scale rotation quaternion by weight: q_weighted = (1-w)*q_identity + w*q
                // For quaternions, we slerp from identity toward the target
                glm::quat identity{1.0f, 0.0f, 0.0f, 0.0f};
                rotated = glm::normalize(glm::slerp(identity, rotated, clip.weight));

                if (isFirst || clip.mode == AnimationClip::OVERRIDE) {
                    if (outRotations.size() > static_cast<size_t>(channel.targetNode)) {
                        outRotations[static_cast<size_t>(channel.targetNode)] = rotated;
                    }
                } else {
                    // ADDITIVE: multiply (compose) on top of existing
                    if (outRotations.size() > static_cast<size_t>(channel.targetNode)) {
                        outRotations[static_cast<size_t>(channel.targetNode)] = rotated *
                            outRotations[static_cast<size_t>(channel.targetNode)];
                    }
                }
                break;
            }

            case Model::AnimationChannel::SCALE: {
                glm::vec3 scaled = glm::vec3(1.0f);
                if (sampler.scales.empty()) break;

                if (clip.currentTime <= sampler.times.front())
                    scaled = sampler.scales.front();
                else if (clip.currentTime >= sampler.times.back())
                    scaled = sampler.scales.back();
                else {
                    size_t nextIdx = 0;
                    for (size_t i = 0; i < sampler.times.size() - 1; ++i) {
                        if (clip.currentTime >= sampler.times[i] && clip.currentTime < sampler.times[i + 1]) {
                            nextIdx = i + 1;
                            break;
                        }
                    }
                    size_t prevIdx = nextIdx - 1;
                    float factor = (clip.currentTime - sampler.times[prevIdx]) /
                                   (sampler.times[nextIdx] - sampler.times[prevIdx]);

                    if (sampler.interpolation == Model::AnimationSampler::STEP)
                        scaled = sampler.scales[prevIdx];
                    else
                        scaled = glm::mix(sampler.scales[prevIdx],
                                          sampler.scales[nextIdx], factor);
                }

                // Scale by weight
                scaled = glm::mix(glm::vec3(1.0f), scaled, clip.weight);

                if (isFirst || clip.mode == AnimationClip::OVERRIDE) {
                    if (outScales.size() > static_cast<size_t>(channel.targetNode)) {
                        outScales[static_cast<size_t>(channel.targetNode)] = scaled;
                    }
                } else {
                    if (outScales.size() > static_cast<size_t>(channel.targetNode)) {
                        outScales[static_cast<size_t>(channel.targetNode)] *= scaled;
                    }
                }
                break;
            }

            case Model::AnimationChannel::WEIGHTS:
                // Morph weights handled separately by MorphTargetSystem
                break;
        }
    }
}

int AnimationController::findDominantClipForBone(int boneIndex) const {
    int bestIdx = -1;
    int bestPriority = std::numeric_limits<int>::min();

    for (int i = 0; i < static_cast<int>(clips_.size()); ++i) {
        const auto& clip = clips_[i];
        if (!clip.active || clip.weight <= 0.0f) continue;

        // Check if this clip affects the target bone
        if (clip.clipIndex < 0 || clip.clipIndex >= static_cast<int>(/* we need the model... */0)) {
            continue;
        }

        if (clip.priority > bestPriority) {
            bestPriority = clip.priority;
            bestIdx = i;
        }
    }

    return bestIdx;
}

void AnimationController::setGraph(std::shared_ptr<AnimationGraph> graph) {
    graph_ = graph;
    if (graph_ && graph_->getEntryNode()) {
        currentGraphNodeId_ = graph_->getEntryNode()->id;
        transitioningToNode_ = -1;
        transitionTimer_ = 0.0f;
    }
}

void AnimationController::triggerTransition(int targetNodeId) {
    if (!graph_) return;
    
    const auto* node = graph_->getNode(targetNodeId);
    if (!node) return;
    
    // Store current node for crossfading
    transitioningToNode_ = targetNodeId;
    transitionTimer_ = 0.0f;
    
    // Find the transition duration
    auto trans = graph_->getTransitions(currentGraphNodeId_);
    for (auto t : trans) {
        if (t->targetNodeId == targetNodeId) {
            transitionTimer_ = t->blendDuration;
            break;
        }
    }
    if (transitionTimer_ <= 0.0f) transitionTimer_ = 0.25f; // Default blend time
}

const AnimationGraphNode* AnimationController::getCurrentGraphNode() const {
    if (!graph_ || currentGraphNodeId_ < 0) return nullptr;
    return graph_->getNode(currentGraphNodeId_);
}

std::string AnimationController::getCurrentGraphNodeName() const {
    const auto* node = getCurrentGraphNode();
    return node ? node->name : "None";
}

std::vector<std::pair<std::string, void*>> AnimationController::update(float deltaTime,
                                                                       const Model& model,
                                                                       std::vector<glm::vec3>& outTranslations,
                                                                       std::vector<glm::quat>& outRotations,
                                                                       std::vector<glm::vec3>& outScales) {
    firedEvents_.clear();

    // Evaluate graph transitions if a graph exists
    if (graph_ && !transitioningToNode_) {
        const auto* trans = graph_->evaluateNextTransition();
        if (trans && graph_->getCurrentNode()) {
            // Check time-based transitions
            if (trans->condition == TransitionCondition::TIME_BASED) {
                // This is handled by checking elapsed time in the graph's step()
                // For now, we'll use a simple threshold
            }
        }
    }

    // Handle crossfading during transition
    if (transitioningToNode_ != -1) {
        transitionTimer_ -= deltaTime;
        if (transitionTimer_ <= 0.0f) {
            // Transition complete
            currentGraphNodeId_ = transitioningToNode_;
            transitioningToNode_ = -1;
            transitionTimer_ = 0.0f;
        }
    }

    // Step each active clip's time
    for (auto& clip : clips_) {
        if (!clip.active || clip.weight <= 0.0f) continue;
        clip.step(deltaTime, model, outTranslations, outRotations, outScales);

        // Fire events
        while (clip.nextEventIndex < clip.events.size() &&
               clip.events[clip.nextEventIndex].time <= clip.currentTime) {
            const auto& evt = clip.events[clip.nextEventIndex];
            firedEvents_.emplace_back(evt.name, evt.userData);

            // Call callback if set
            if (eventCallback_) {
                eventCallback_(evt.name, evt.userData);
            }
            ++clip.nextEventIndex;
        }
    }

    // Blend clips with same priority (OVERRIDE mode: only highest-priority clip writes)
    // For now, OVERRIDE mode means the first active clip for each bone wins.
    // ADDITIVE mode means all clips accumulate.

    return std::move(firedEvents_);
}

std::vector<glm::mat4> AnimationController::computeGlobalTransforms(
    const Model& model,
    const std::vector<glm::mat4>& baseTransforms) const {

    std::vector<glm::mat4> globalTransforms;
    if (baseTransforms.empty()) return globalTransforms;

    globalTransforms.resize(baseTransforms.size(), glm::mat4(1.0f));

    // Apply each active clip's local transforms to the base
    for (const auto& clip : clips_) {
        if (!clip.active || clip.weight <= 0.0f) continue;
        if (clip.clipIndex < 0 || clip.clipIndex >= static_cast<int>(model.getAnimations().size()))
            continue;

        const auto& animation = model.getAnimations()[static_cast<size_t>(clip.clipIndex)];
        const auto& nodes     = model.getNodes();

        for (const auto& channel : animation.channels) {
            if (channel.targetNode < 0 || channel.targetNode >= static_cast<int>(nodes.size()))
                continue;

            auto& node = nodes[static_cast<size_t>(channel.targetNode)];

            if (channel.path == Model::AnimationChannel::TRANSLATION) {
                globalTransforms[static_cast<size_t>(channel.targetNode)] =
                    baseTransforms[static_cast<size_t>(channel.targetNode)] *
                    glm::translate(glm::mat4(1.0f), node.translation);
            } else if (channel.path == Model::AnimationChannel::ROTATION) {
                globalTransforms[static_cast<size_t>(channel.targetNode)] =
                    baseTransforms[static_cast<size_t>(channel.targetNode)] *
                    glm::mat4_cast(node.rotation);
            } else if (channel.path == Model::AnimationChannel::SCALE) {
                globalTransforms[static_cast<size_t>(channel.targetNode)] =
                    baseTransforms[static_cast<size_t>(channel.targetNode)] *
                    glm::scale(glm::mat4(1.0f), node.scale);
            }
        }
    }

    return globalTransforms;
}

}  // namespace engine
