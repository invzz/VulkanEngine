#include "Editor/RenderContext.hpp"

#include <cstring>
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
            createSubmeshBuffers();
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
            pool.addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, static_cast<uint32_t>(SwapChain::maxFramesInFlight() * 2));
        }
        globalPool_ = pool.build();
    }
    void RenderContext::createGlobalSetLayout() {
        auto builder = DescriptorSetLayout::Builder(device_)
                           .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT)
                           .addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
        if (rayTracingEnabled_) {
            builder.addBinding(2, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_FRAGMENT_BIT);
            builder.addBinding(7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);  // per-instance submesh headers
            builder.addBinding(8, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);  // per-submesh entries
        }
        globalSetLayout_ = builder.addBinding(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
                               .addBinding(4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
                               .addBinding(5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
                               .addBinding(6, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT)
                               .build();
    }
    void RenderContext::createSubmeshBuffers() {
        instanceSubmeshHeaderBuffers_.resize(SwapChain::maxFramesInFlight());
        instanceSubmeshDataBuffers_.resize(SwapChain::maxFramesInFlight());
        for (size_t i = 0; i < SwapChain::maxFramesInFlight(); i++) {
            // Binding 7: per-instance (offset, count) pairs, up to 1024 instances
            instanceSubmeshHeaderBuffers_[i] = std::make_unique<Buffer>(device_,
                sizeof(uint32_t) * 2,  // 2 uints per instance: offset, count
                1024,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                device_.getProperties().limits.minStorageBufferOffsetAlignment);
            instanceSubmeshHeaderBuffers_[i]->map();
            // Binding 8: per-submesh (startTri, endTri, opacity) triples, up to 8192 submeshes total
            instanceSubmeshDataBuffers_[i] = std::make_unique<Buffer>(device_,
                sizeof(uint32_t) * 3,  // 3 uints per submesh
                8192,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                device_.getProperties().limits.minStorageBufferOffsetAlignment);
            instanceSubmeshDataBuffers_[i]->map();
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

            if (rayTracingEnabled_) {
                // Write TLAS at binding 2
                VkAccelerationStructureKHR tlasHandle =
                    (accelBuilder_ != nullptr) ? accelBuilder_->getTlas() : VK_NULL_HANDLE;
                writer.writeAccelerationStructure(2, tlasHandle);

                // Write submesh header buffer at binding 7
                if (i < instanceSubmeshHeaderBuffers_.size() && instanceSubmeshHeaderBuffers_[i]) {
                    auto hdrInfo = instanceSubmeshHeaderBuffers_[i]->descriptorInfo();
                    writer.writeBuffer(7, &hdrInfo);
                }
                // Write submesh data buffer at binding 8
                if (i < instanceSubmeshDataBuffers_.size() && instanceSubmeshDataBuffers_[i]) {
                    auto dataInfo = instanceSubmeshDataBuffers_[i]->descriptorInfo();
                    writer.writeBuffer(8, &dataInfo);
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

        // Write TLAS at binding 2
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

        // Write submesh header buffer at binding 7
        if (frameIndex < static_cast<int>(instanceSubmeshHeaderBuffers_.size()) && instanceSubmeshHeaderBuffers_[frameIndex]) {
            auto hdrInfo = instanceSubmeshHeaderBuffers_[frameIndex]->descriptorInfo();
            VkWriteDescriptorSet writeHdr{};
            writeHdr.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeHdr.dstSet          = globalDescriptorSets_[frameIndex];
            writeHdr.dstBinding      = 7;
            writeHdr.dstArrayElement = 0;
            writeHdr.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            writeHdr.descriptorCount = 1;
            writeHdr.pBufferInfo     = &hdrInfo;
            vkUpdateDescriptorSets(device_.device(), 1, &writeHdr, 0, nullptr);
        }

        // Write submesh data buffer at binding 8
        if (frameIndex < static_cast<int>(instanceSubmeshDataBuffers_.size()) && instanceSubmeshDataBuffers_[frameIndex]) {
            auto dataInfo = instanceSubmeshDataBuffers_[frameIndex]->descriptorInfo();
            VkWriteDescriptorSet writeData{};
            writeData.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeData.dstSet          = globalDescriptorSets_[frameIndex];
            writeData.dstBinding      = 8;
            writeData.dstArrayElement = 0;
            writeData.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            writeData.descriptorCount = 1;
            writeData.pBufferInfo     = &dataInfo;
            vkUpdateDescriptorSets(device_.device(), 1, &writeData, 0, nullptr);
        }
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
        const std::vector<uint32_t>& instanceSubmeshHeaders,
        const std::vector<uint32_t>& instanceSubmeshData,
        VkCommandBuffer cmd) {
        if (!accelBuilder_)
            return VK_NULL_HANDLE;

        VkAccelerationStructureKHR tlas = accelBuilder_->rebuildTlas(instances, cmd);

        // Upload per-instance submesh header data (binding 7)
        if (!instanceSubmeshHeaderBuffers_.empty() && instanceSubmeshHeaderBuffers_[0]) {
            size_t const headerBytes = instanceSubmeshHeaders.size() * sizeof(uint32_t);
            size_t const maxBytes    = instanceSubmeshHeaderBuffers_[0]->getBufferSize();
            size_t const toWrite     = std::min(headerBytes, maxBytes);
            for (size_t fi = 0; fi < instanceSubmeshHeaderBuffers_.size(); ++fi) {
                if (fi >= static_cast<size_t>(globalDescriptorSets_.size()))
                    break;
                instanceSubmeshHeaderBuffers_[fi]->writeToBuffer(instanceSubmeshHeaders.data(), toWrite);
                instanceSubmeshHeaderBuffers_[fi]->flush();
            }
        }

        // Upload per-submesh opacity data (binding 8)
        if (!instanceSubmeshDataBuffers_.empty() && instanceSubmeshDataBuffers_[0]) {
            size_t const dataBytes = instanceSubmeshData.size() * sizeof(uint32_t);
            size_t const maxBytes  = instanceSubmeshDataBuffers_[0]->getBufferSize();
            size_t const toWrite   = std::min(dataBytes, maxBytes);
            for (size_t fi = 0; fi < instanceSubmeshDataBuffers_.size(); ++fi) {
                if (fi >= static_cast<size_t>(globalDescriptorSets_.size()))
                    break;
                instanceSubmeshDataBuffers_[fi]->writeToBuffer(instanceSubmeshData.data(), toWrite);
                instanceSubmeshDataBuffers_[fi]->flush();
            }
        }

        // Update all descriptor sets with the new TLAS handle
        for (int i = 0; i < static_cast<int>(globalDescriptorSets_.size()); ++i) {
            updateTlasDescriptorSets(i);
        }

        return tlas;
    }
}  // namespace engine