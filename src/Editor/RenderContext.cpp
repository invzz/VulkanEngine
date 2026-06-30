#include "Editor/RenderContext.hpp"

#include <memory>
#include <stdexcept>
#include <vector>

#include "Engine/Graphics/Buffer.hpp"
#include "Engine/Graphics/Descriptors.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Graphics/SwapChain.hpp"
#include "Engine/Scene/LightMath.hpp"
#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/components/DirectionalLightComponent.hpp"
#include "Engine/Scene/components/PointLightComponent.hpp"
#include "Engine/Scene/components/SpotLightComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"

#include "ModelLib/Resources/MeshManager.hpp"
#include "glm/geometric.hpp"
#include "glm/trigonometric.hpp"
#include "vulkan/vulkan_core.h"

namespace engine {

    RenderContext::RenderContext(Device& device, MeshManager& meshManager)
        : device_{device},
          meshManager_{meshManager},
          uboBuffers_(SwapChain::maxFramesInFlight()),
          uboColdBuffers_(SwapChain::maxFramesInFlight()),
          globalDescriptorSets_(SwapChain::maxFramesInFlight()) {
        createDescriptorPool();
        createGlobalSetLayout();
        createUBOBuffers();
        // Start with a conservative capacity and grow on demand.
        createLightBuffers(64, 16, 64);
        createGlobalDescriptorSets();

        for (int i = 0; i < SwapChain::maxFramesInFlight(); i++) {
            updateLightDescriptorSets(i);
        }
    }

    void RenderContext::createDescriptorPool() {
        globalPool_ = DescriptorPool::Builder(device_)
                          .setMaxSets(static_cast<uint32_t>(SwapChain::maxFramesInFlight()))
                          // Two UBO bindings (hot + cold) per set.
                          .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, static_cast<uint32_t>(SwapChain::maxFramesInFlight() * 4))
                          // Storage buffers: mesh buffer + 3 light buffers per set.
                          .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, static_cast<uint32_t>(SwapChain::maxFramesInFlight() * 4))
                          .build();
    }

    void RenderContext::createGlobalSetLayout() {
        globalSetLayout_ = DescriptorSetLayout::Builder(device_)
                               .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT)
                               .addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT)
                               // Dynamic lights
                               .addBinding(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
                               .addBinding(4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
                               .addBinding(5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
                               // Cold frame data (rarely changed values)
                               .addBinding(6, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT)
                               .build();
    }

    void RenderContext::createUBOBuffers() {
        for (size_t i = 0; i < uboBuffers_.size(); ++i) {
            uboBuffers_[i] = std::make_unique<Buffer>(device_,
                sizeof(GlobalUbo),
                1,
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                device_.getProperties().limits.minUniformBufferOffsetAlignment);
            uboBuffers_[i]->map();

            uboColdBuffers_[i] = std::make_unique<Buffer>(device_,
                sizeof(GlobalUboCold),
                1,
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                device_.getProperties().limits.minUniformBufferOffsetAlignment);
            uboColdBuffers_[i]->map();
        }
    }

    void RenderContext::createLightBuffers(size_t pointCapacity, size_t directionalCapacity, size_t spotCapacity) {
        pointLightCapacity_       = pointCapacity;
        directionalLightCapacity_ = directionalCapacity;
        spotLightCapacity_        = spotCapacity;

        pointLightBuffers_.resize(SwapChain::maxFramesInFlight());
        directionalLightBuffers_.resize(SwapChain::maxFramesInFlight());
        spotLightBuffers_.resize(SwapChain::maxFramesInFlight());

        for (int i = 0; i < SwapChain::maxFramesInFlight(); i++) {
            pointLightBuffers_[i] = std::make_unique<Buffer>(device_,
                sizeof(PointLight),
                static_cast<uint32_t>(pointLightCapacity_),
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                device_.getProperties().limits.minStorageBufferOffsetAlignment);
            pointLightBuffers_[i]->map();

            directionalLightBuffers_[i] = std::make_unique<Buffer>(device_,
                sizeof(DirectionalLight),
                static_cast<uint32_t>(directionalLightCapacity_),
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                device_.getProperties().limits.minStorageBufferOffsetAlignment);
            directionalLightBuffers_[i]->map();

            spotLightBuffers_[i] = std::make_unique<Buffer>(device_,
                sizeof(SpotLight),
                static_cast<uint32_t>(spotLightCapacity_),
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                device_.getProperties().limits.minStorageBufferOffsetAlignment);
            spotLightBuffers_[i]->map();
        }
    }

    void RenderContext::createGlobalDescriptorSets() {
        for (size_t i = 0; i < globalDescriptorSets_.size(); i++) {
            auto bufferInfo     = uboBuffers_[i]->descriptorInfo();
            auto coldBufferInfo = uboColdBuffers_[i]->descriptorInfo();
            auto meshInfo       = meshManager_.getDescriptorInfo();
            auto pointInfo      = pointLightBuffers_[i]->descriptorInfo();
            auto dirInfo        = directionalLightBuffers_[i]->descriptorInfo();
            auto spotInfo       = spotLightBuffers_[i]->descriptorInfo();

            // Write all bindings required by the layout (including light buffers) so
            // DescriptorWriter's defensive check succeeds. We still update light
            // counts later as needed via updateLightDescriptorSets.
            DescriptorWriter(*globalSetLayout_, *globalPool_)
                .writeBuffer(0, &bufferInfo)
                .writeBuffer(1, &meshInfo)
                .writeBuffer(3, &pointInfo)
                .writeBuffer(4, &dirInfo)
                .writeBuffer(5, &spotInfo)
                .writeBuffer(6, &coldBufferInfo)
                .build(globalDescriptorSets_[i]);
            if (globalDescriptorSets_[i] == VK_NULL_HANDLE) {
                throw std::runtime_error("failed to allocate global descriptor set");
            }
        }
    }

    void RenderContext::updateLightDescriptorSets(int frameIndex) {
        VkDescriptorBufferInfo const pointInfo = pointLightBuffers_[frameIndex]->descriptorInfo();
        VkDescriptorBufferInfo const dirInfo   = directionalLightBuffers_[frameIndex]->descriptorInfo();
        VkDescriptorBufferInfo const spotInfo  = spotLightBuffers_[frameIndex]->descriptorInfo();

        auto writeSet = [&](VkDescriptorSet dstSet, uint32_t binding, VkDescriptorBufferInfo const& info) {
            VkWriteDescriptorSet write{};
            write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet          = dstSet;
            write.dstBinding      = binding;
            write.dstArrayElement = 0;
            write.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            write.descriptorCount = 1;
            write.pBufferInfo     = &info;
            vkUpdateDescriptorSets(device_.device(), 1, &write, 0, nullptr);
        };

        writeSet(globalDescriptorSets_[frameIndex], 3, pointInfo);
        writeSet(globalDescriptorSets_[frameIndex], 4, dirInfo);
        writeSet(globalDescriptorSets_[frameIndex], 5, spotInfo);
    }

    void RenderContext::updateUBO(int frameIndex, const GlobalUbo& ubo, const GlobalUboCold& uboCold) {
        uboBuffers_[frameIndex]->writeToBuffer(&ubo);
        uboBuffers_[frameIndex]->flush();

        uboColdBuffers_[frameIndex]->writeToBuffer(&uboCold);
        uboColdBuffers_[frameIndex]->flush();
    }

    RenderContext::LightCounts RenderContext::updateLightBuffers(int frameIndex, Scene& scene) {
        // Gather lights from the scene.
        std::vector<PointLight>       pointLights;
        std::vector<DirectionalLight> dirLights;
        std::vector<SpotLight>        spotLights;

        auto& registry = scene.getRegistry();

        {
            auto view = registry.view<PointLightComponent, TransformComponent>();
            pointLights.reserve(view.size_hint());
            for (auto entity : view) {
                auto [point, transform] = view.get<PointLightComponent, TransformComponent>(entity);
                PointLight pl{};
                pl.positionRadius2 = glm::vec4(transform.translation, point.radius * point.radius);
                pl.colorIntensity  = glm::vec4(point.color, point.intensity);
                pointLights.push_back(pl);
            }
        }

        {
            // Only a single directional light is supported at runtime — take the first one found.
            auto view = registry.view<DirectionalLightComponent, TransformComponent>();
            for (auto entity : view) {
                auto [dir, transform] = view.get<DirectionalLightComponent, TransformComponent>(entity);
                DirectionalLight dl{};

                glm::vec3 direction = transform.getForwardDir();
                if (!std::isfinite(direction.x) || !std::isfinite(direction.y) || !std::isfinite(direction.z) || glm::dot(direction, direction) < 1e-8f) {
                    // Fallback to a stable default sun direction if transform forward is invalid.
                    direction = glm::vec3(0.0f, -1.0f, 0.0f);
                }
                dl.direction = glm::vec4(glm::normalize(direction), 0.f);
                dl.color     = glm::vec4(dir.color, dir.intensity);
                dirLights.push_back(dl);
                break;  // only keep the first directional light
            }
        }

        {
            auto view = registry.view<SpotLightComponent, TransformComponent>();
            spotLights.reserve(view.size_hint());
            for (auto entity : view) {
                auto [spot, transform] = view.get<SpotLightComponent, TransformComponent>(entity);
                SpotLight sl{};

                glm::vec3 const direction = transform.getForwardDir();

                sl.positionRadius2 = glm::vec4(transform.translation, computeSpotLightRadius2(spot));
                sl.directionInner  = glm::vec4(glm::normalize(direction), glm::cos(glm::radians(spot.innerCutoffAngle)));
                sl.colorIntensity  = glm::vec4(spot.color, spot.intensity);
                sl.attenOuter      = glm::vec4(
                    glm::cos(glm::radians(spot.outerCutoffAngle)),
                    spot.constantAttenuation,
                    spot.linearAttenuation,
                    spot.quadraticAttenuation);

                spotLights.push_back(sl);
            }
        }

        auto nextPow2 = [](size_t v) {
            size_t p = 1;
            while (p < v) {
                p <<= 1;
            }
            return p;
        };

        bool resized = false;
        if (pointLights.size() > pointLightCapacity_) {
            pointLightCapacity_ = nextPow2(pointLights.size());
            resized             = true;
        }
        if (dirLights.size() > directionalLightCapacity_) {
            directionalLightCapacity_ = nextPow2(dirLights.size());
            resized                   = true;
        }
        if (spotLights.size() > spotLightCapacity_) {
            spotLightCapacity_ = nextPow2(spotLights.size());
            resized            = true;
        }

        if (resized) {
            // Recreate all per-frame buffers so every in-flight frame has matching capacity.
            createLightBuffers(pointLightCapacity_, directionalLightCapacity_, spotLightCapacity_);
            for (int i = 0; i < SwapChain::maxFramesInFlight(); i++) {
                updateLightDescriptorSets(i);
            }
        } else {
            // Always refresh descriptors for this frame (cheap), in case buffers were re-created earlier.
            updateLightDescriptorSets(frameIndex);
        }

        if (!pointLights.empty()) {
            pointLightBuffers_[frameIndex]->writeToBuffer(pointLights.data(), pointLights.size() * sizeof(PointLight));
            pointLightBuffers_[frameIndex]->flush();
        }
        if (!dirLights.empty()) {
            directionalLightBuffers_[frameIndex]->writeToBuffer(dirLights.data(), dirLights.size() * sizeof(DirectionalLight));
            directionalLightBuffers_[frameIndex]->flush();
        }
        if (!spotLights.empty()) {
            spotLightBuffers_[frameIndex]->writeToBuffer(spotLights.data(), spotLights.size() * sizeof(SpotLight));
            spotLightBuffers_[frameIndex]->flush();
        }

        LightCounts counts;
        counts.point       = static_cast<int>(pointLights.size());
        counts.directional = static_cast<int>(dirLights.size());
        counts.spot        = static_cast<int>(spotLights.size());
        return counts;
    }

    // Shadow descriptors removed - to be reimplemented later

}  // namespace engine
