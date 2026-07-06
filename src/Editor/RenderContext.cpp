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
          globalDescriptorSets_(SwapChain::maxFramesInFlight()),
          rayTracingEnabled_(device.rayQuerySupported()) {
        createDescriptorPool();
        createGlobalSetLayout();
        createUBOBuffers();
        createLightBuffers(64, 16, 64);
        if (rayTracingEnabled_) {
            createInstanceOpacityBuffers();
        }
        createGlobalDescriptorSets();
        for (int i = 0; i < SwapChain::maxFramesInFlight(); i++) {
            updateLightDescriptorSets(i);
        }
    }
    void RenderContext::createDescriptorPool() {
        auto pool = DescriptorPool::Builder(device_)
                        .setMaxSets(static_cast<uint32_t>(SwapChain::maxFramesInFlight()))
                        .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, static_cast<uint32_t>(SwapChain::maxFramesInFlight() * 4))
                        .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, static_cast<uint32_t>(SwapChain::maxFramesInFlight() * 4));
        if (rayTracingEnabled_) {
            pool.addPoolSize(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, static_cast<uint32_t>(SwapChain::maxFramesInFlight()));
            pool.addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, static_cast<uint32_t>(SwapChain::maxFramesInFlight()));
        }
        globalPool_ = pool.build();
    }
    void RenderContext::createGlobalSetLayout() {
        auto builder = DescriptorSetLayout::Builder(device_)
                           .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT)
                           .addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
        if (rayTracingEnabled_) {
            builder.addBinding(2, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_FRAGMENT_BIT);
            builder.addBinding(7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
        }
        globalSetLayout_ = builder.addBinding(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
                               .addBinding(4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
                               .addBinding(5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
                               .addBinding(6, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT)
                               .build();
    }
    void RenderContext::createInstanceOpacityBuffers() {
        instanceOpacityBuffers_.resize(SwapChain::maxFramesInFlight());
        for (size_t i = 0; i < SwapChain::maxFramesInFlight(); i++) {
            instanceOpacityBuffers_[i] = std::make_unique<Buffer>(device_,
                sizeof(float),
                1024,  // Max 1024 instances
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                device_.getProperties().limits.minStorageBufferOffsetAlignment);
            instanceOpacityBuffers_[i]->map();
        }
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

            // Start the writer chain
            DescriptorWriter writer(*globalSetLayout_, *globalPool_);
            writer.writeBuffer(0, &bufferInfo)
                .writeBuffer(1, &meshInfo)
                .writeBuffer(3, &pointInfo)
                .writeBuffer(4, &dirInfo)
                .writeBuffer(5, &spotInfo)
                .writeBuffer(6, &coldBufferInfo);

            // Write TLAS at binding 2 if ray tracing is enabled
            // (passes validation in DescriptorWriter::build which checks all bindings)
            if (rayTracingEnabled_) {
                VkAccelerationStructureKHR tlasHandle =
                    (accelBuilder_ != nullptr) ? accelBuilder_->getTlas() : VK_NULL_HANDLE;
                writer.writeAccelerationStructure(2, tlasHandle);
                if (i < instanceOpacityBuffers_.size() && instanceOpacityBuffers_[i]) {
                    auto opacityInfo = instanceOpacityBuffers_[i]->descriptorInfo();
                    writer.writeBuffer(7, &opacityInfo);
                }
            }

            writer.build(globalDescriptorSets_[i]);
            if (globalDescriptorSets_[i] == VK_NULL_HANDLE) {
                throw std::runtime_error("failed to allocate global descriptor set");
            }
        }
    }
    void RenderContext::updateLightDescriptorSets(int frameIndex) {
        VkDescriptorBufferInfo const pointInfo = pointLightBuffers_[frameIndex]->descriptorInfo();
        VkDescriptorBufferInfo const dirInfo   = directionalLightBuffers_[frameIndex]->descriptorInfo();
        VkDescriptorBufferInfo const spotInfo  = spotLightBuffers_[frameIndex]->descriptorInfo();
        auto                         writeSet  = [&](VkDescriptorSet dstSet, uint32_t binding, VkDescriptorBufferInfo const& info) {
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
    void RenderContext::updateTlasDescriptorSets(int frameIndex) {
        if (!accelBuilder_)
            return;

        VkAccelerationStructureKHR tlasHandle = accelBuilder_->getTlas();
        if (tlasHandle == VK_NULL_HANDLE)
            return;

        VkWriteDescriptorSetAccelerationStructureKHR accelWriteInfo{};
        accelWriteInfo.sType                      = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
        accelWriteInfo.accelerationStructureCount = 1;
        accelWriteInfo.pAccelerationStructures    = &tlasHandle;

        VkWriteDescriptorSet writeAccel{};
        writeAccel.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeAccel.pNext           = &accelWriteInfo;
        writeAccel.dstSet          = globalDescriptorSets_[frameIndex];
        writeAccel.dstBinding      = 2;
        writeAccel.dstArrayElement = 0;
        writeAccel.descriptorType  = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        writeAccel.descriptorCount = 1;
        vkUpdateDescriptorSets(device_.device(), 1, &writeAccel, 0, nullptr);
    }

    void RenderContext::updateMeshDescriptorSet(int frameIndex) {
        VkDescriptorBufferInfo const meshInfo = meshManager_.getDescriptorInfo();
        VkWriteDescriptorSet write{};
        write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet          = globalDescriptorSets_[frameIndex];
        write.dstBinding      = 1;
        write.dstArrayElement = 0;
        write.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo     = &meshInfo;
        vkUpdateDescriptorSets(device_.device(), 1, &write, 0, nullptr);
    }
    void RenderContext::updateUBO(int frameIndex, const GlobalUbo& ubo, const GlobalUboCold& uboCold) {
        uboBuffers_[frameIndex]->writeToBuffer(&ubo);
        uboBuffers_[frameIndex]->flush();
        uboColdBuffers_[frameIndex]->writeToBuffer(&uboCold);
        uboColdBuffers_[frameIndex]->flush();
    }
    RenderContext::LightCounts RenderContext::updateLightBuffers(int frameIndex, Scene& scene) {
        std::vector<PointLight>       pointLights;
        std::vector<DirectionalLight> dirLights;
        std::vector<SpotLight>        spotLights;
        auto&                         registry = scene.getRegistry();
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
            auto view = registry.view<DirectionalLightComponent, TransformComponent>();
            for (auto entity : view) {
                auto [dir, transform] = view.get<DirectionalLightComponent, TransformComponent>(entity);
                DirectionalLight dl{};
                glm::vec3        direction = transform.getForwardDir();
                if (!std::isfinite(direction.x) || !std::isfinite(direction.y) || !std::isfinite(direction.z) || glm::dot(direction, direction) < 1e-8f) {
                    direction = glm::vec3(0.0f, -1.0f, 0.0f);
                }
                dl.direction = glm::vec4(glm::normalize(direction), 0.f);
                dl.color     = glm::vec4(dir.color, dir.intensity);
                dirLights.push_back(dl);
                break;
            }
        }
        {
            auto view = registry.view<SpotLightComponent, TransformComponent>();
            spotLights.reserve(view.size_hint());
            for (auto entity : view) {
                auto [spot, transform] = view.get<SpotLightComponent, TransformComponent>(entity);
                SpotLight       sl{};
                glm::vec3 const direction = transform.getForwardDir();
                sl.positionRadius2        = glm::vec4(transform.translation, computeSpotLightRadius2(spot));
                sl.directionInner         = glm::vec4(glm::normalize(direction), glm::cos(glm::radians(spot.innerCutoffAngle)));
                sl.colorIntensity         = glm::vec4(spot.color, spot.intensity);
                sl.attenOuter             = glm::vec4(
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
            createLightBuffers(pointLightCapacity_, directionalLightCapacity_, spotLightCapacity_);
            for (int i = 0; i < SwapChain::maxFramesInFlight(); i++) {
                updateLightDescriptorSets(i);
            }
        } else {
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
    VkAccelerationStructureKHR RenderContext::rebuildTlas(
        const std::vector<std::pair<glm::mat4, VkAccelerationStructureKHR>>& instances,
        const std::vector<float>& opacityValues,
        VkCommandBuffer cmd) {
        if (!accelBuilder_)
            return VK_NULL_HANDLE;

        VkAccelerationStructureKHR tlas = accelBuilder_->rebuildTlas(instances, cmd);

        // Upload per-instance opacity data
        if (!instanceOpacityBuffers_.empty() && instanceOpacityBuffers_[0]) {
            size_t count = std::min(opacityValues.size(), size_t{1024});
            for (size_t fi = 0; fi < instanceOpacityBuffers_.size(); ++fi) {
                instanceOpacityBuffers_[fi]->writeToBuffer(opacityValues.data(), count * sizeof(float));
                instanceOpacityBuffers_[fi]->flush();
            }
        }

        // Update all descriptor sets with the new TLAS handle
        for (int i = 0; i < static_cast<int>(globalDescriptorSets_.size()); ++i) {
            updateTlasDescriptorSets(i);
        }

        return tlas;
    }
}  // namespace engine
