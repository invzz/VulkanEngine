#include "Engine/Scene/Components/AnimationClip.hpp"

#include <algorithm>
#include <cmath>
namespace engine {
    void AnimationClip::step(float deltaTime, const Model& model,
        std::vector<glm::vec3>& outTranslations,
        std::vector<glm::quat>& outRotations,
        std::vector<glm::vec3>& outScales) {
        if (!active || weight <= 0.0f || clipIndex < 0)
            return;
        const auto& animations = model.getAnimations();
        if (clipIndex >= static_cast<int>(animations.size()))
            return;
        const auto& animation = animations[static_cast<size_t>(clipIndex)];
        currentTime += deltaTime * speed;
        if (currentTime > animation.duration) {
            if (loop) {
                currentTime = fmod(currentTime, animation.duration);
            } else {
                currentTime = animation.duration;
                active      = false;
                return;
            }
        }
        const auto& samplers = animation.samplers;
        const auto& channels = animation.channels;
        const auto& nodes    = model.getNodes();
        for (const auto& channel : channels) {
            if (channel.targetNode < 0 || channel.targetNode >= static_cast<int>(nodes.size()))
                continue;
            if (channel.samplerIndex < 0 || channel.samplerIndex >= static_cast<int>(samplers.size()))
                continue;
            const auto& sampler = samplers[static_cast<size_t>(channel.samplerIndex)];
            switch (channel.path) {
                case Model::AnimationChannel::TRANSLATION: {
                    if (sampler.translations.empty())
                        continue;
                    glm::vec3 translated = glm::vec3(0.0f);
                    if (currentTime <= sampler.times.front())
                        translated = sampler.translations.front();
                    else if (currentTime >= sampler.times.back())
                        translated = sampler.translations.back();
                    else {
                        size_t nextIdx = 0;
                        for (size_t i = 0; i < sampler.times.size() - 1; ++i) {
                            if (currentTime >= sampler.times[i] && currentTime < sampler.times[i + 1]) {
                                nextIdx = i + 1;
                                break;
                            }
                        }
                        size_t prevIdx = nextIdx - 1;
                        float  t0      = sampler.times[prevIdx];
                        float  t1      = sampler.times[nextIdx];
                        float  factor  = (currentTime - t0) / (t1 - t0);
                        if (sampler.interpolation == Model::AnimationSampler::STEP) {
                            translated = sampler.translations[prevIdx];
                        } else {
                            translated = glm::mix(sampler.translations[prevIdx], sampler.translations[nextIdx], factor);
                        }
                    }
                    if (outTranslations.size() > static_cast<size_t>(channel.targetNode)) {
                        outTranslations[static_cast<size_t>(channel.targetNode)] = translated;
                    }
                    break;
                }
                case Model::AnimationChannel::ROTATION: {
                    if (sampler.rotations.empty())
                        continue;
                    glm::quat rotated{1.0f, 0.0f, 0.0f, 0.0f};
                    if (currentTime <= sampler.times.front())
                        rotated = sampler.rotations.front();
                    else if (currentTime >= sampler.times.back())
                        rotated = sampler.rotations.back();
                    else {
                        size_t nextIdx = 0;
                        for (size_t i = 0; i < sampler.times.size() - 1; ++i) {
                            if (currentTime >= sampler.times[i] && currentTime < sampler.times[i + 1]) {
                                nextIdx = i + 1;
                                break;
                            }
                        }
                        size_t prevIdx = nextIdx - 1;
                        float  t0      = sampler.times[prevIdx];
                        float  t1      = sampler.times[nextIdx];
                        float  factor  = (currentTime - t0) / (t1 - t0);
                        if (sampler.interpolation == Model::AnimationSampler::STEP) {
                            rotated = sampler.rotations[prevIdx];
                        } else {
                            rotated = glm::normalize(glm::slerp(sampler.rotations[prevIdx], sampler.rotations[nextIdx], factor));
                        }
                    }
                    if (outRotations.size() > static_cast<size_t>(channel.targetNode)) {
                        outRotations[static_cast<size_t>(channel.targetNode)] = rotated;
                    }
                    break;
                }
                case Model::AnimationChannel::SCALE: {
                    if (sampler.scales.empty())
                        continue;
                    glm::vec3 scaled = glm::vec3(1.0f);
                    if (currentTime <= sampler.times.front())
                        scaled = sampler.scales.front();
                    else if (currentTime >= sampler.times.back())
                        scaled = sampler.scales.back();
                    else {
                        size_t nextIdx = 0;
                        for (size_t i = 0; i < sampler.times.size() - 1; ++i) {
                            if (currentTime >= sampler.times[i] && currentTime < sampler.times[i + 1]) {
                                nextIdx = i + 1;
                                break;
                            }
                        }
                        size_t prevIdx = nextIdx - 1;
                        float  t0      = sampler.times[prevIdx];
                        float  t1      = sampler.times[nextIdx];
                        float  factor  = (currentTime - t0) / (t1 - t0);
                        if (sampler.interpolation == Model::AnimationSampler::STEP) {
                            scaled = sampler.scales[prevIdx];
                        } else {
                            scaled = glm::mix(sampler.scales[prevIdx], sampler.scales[nextIdx], factor);
                        }
                    }
                    if (outScales.size() > static_cast<size_t>(channel.targetNode)) {
                        outScales[static_cast<size_t>(channel.targetNode)] = scaled;
                    }
                    break;
                }
                case Model::AnimationChannel::WEIGHTS: {
                    break;
                }
            }
        }
    }
}  // namespace engine
