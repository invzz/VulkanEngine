#include "Engine/Graphics/AccelBuilder.hpp"

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#include "Engine/Core/Logger.hpp"
#include "Engine/Graphics/Buffer.hpp"

#include "vulkan/vulkan_core.h"

namespace engine {

    AccelBuilder::AccelBuilder(Device& device) : device_(device) {
        loadFunctions();
    }

    AccelBuilder::~AccelBuilder() {
        // Destroy TLAS
        if (tlas_ != VK_NULL_HANDLE) {
            vkDestroyAccelerationStructureKHR_(device_.device(), tlas_, nullptr);
            tlas_ = VK_NULL_HANDLE;
        }
        // Destroy all BLAS
        for (auto& [model, blas] : blasMap_) {
            if (blas != VK_NULL_HANDLE) {
                vkDestroyAccelerationStructureKHR_(device_.device(), blas, nullptr);
            }
        }
        blasMap_.clear();
        blasBuffers_.clear();
        destroyScratch();
    }

    void AccelBuilder::loadFunctions() {
        auto dev = device_.device();
        vkCreateAccelerationStructureKHR_ =
            reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(
                vkGetDeviceProcAddr(dev, "vkCreateAccelerationStructureKHR"));
        vkDestroyAccelerationStructureKHR_ =
            reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(
                vkGetDeviceProcAddr(dev, "vkDestroyAccelerationStructureKHR"));
        vkGetAccelerationStructureBuildSizesKHR_ =
            reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(
                vkGetDeviceProcAddr(dev, "vkGetAccelerationStructureBuildSizesKHR"));
        vkCmdBuildAccelerationStructuresKHR_ =
            reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(
                vkGetDeviceProcAddr(dev, "vkCmdBuildAccelerationStructuresKHR"));
        vkGetAccelerationStructureDeviceAddressKHR_ =
            reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(
                vkGetDeviceProcAddr(dev, "vkGetAccelerationStructureDeviceAddressKHR"));
        vkBuildAccelerationStructuresKHR_ =
            reinterpret_cast<PFN_vkBuildAccelerationStructuresKHR>(
                vkGetDeviceProcAddr(dev, "vkBuildAccelerationStructuresKHR"));

        if (!vkCreateAccelerationStructureKHR_ || !vkDestroyAccelerationStructureKHR_ ||
            !vkGetAccelerationStructureBuildSizesKHR_ || !vkCmdBuildAccelerationStructuresKHR_ ||
            !vkGetAccelerationStructureDeviceAddressKHR_) {
            throw std::runtime_error("Failed to load raytracing function pointers");
        }
        Logger::info(LogChannel::General, "[AccelBuilder] Raytracing function pointers loaded");
    }

    Buffer AccelBuilder::createAccelBuffer(VkAccelerationStructureKHR accel) {
        VkAccelerationStructureDeviceAddressInfoKHR addressInfo{};
        addressInfo.sType                 = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        addressInfo.accelerationStructure = accel;
        VkDeviceAddress address = vkGetAccelerationStructureDeviceAddressKHR_(device_.device(), &addressInfo);
        (void)address;
        return Buffer(device_, 1, 1, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    }

    void AccelBuilder::destroyScratch() {
        blasScratch_.reset();
        tlasScratch_.reset();
        tlasInstanceBuffer_.reset();
        blasScratchSize_ = 0;
        tlasScratchSize_ = 0;
    }

    void AccelBuilder::createBlasScratch(VkDeviceSize size) {
        if (blasScratch_ && blasScratchSize_ >= size)
            return;
        blasScratch_.reset();
        blasScratch_ = std::make_unique<Buffer>(device_,
            1, static_cast<uint32_t>(size),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        blasScratchSize_ = size;
    }

    void AccelBuilder::createTlasScratch(VkDeviceSize size) {
        if (tlasScratch_ && tlasScratchSize_ >= size)
            return;
        tlasScratch_.reset();
        tlasScratch_ = std::make_unique<Buffer>(device_,
            1, static_cast<uint32_t>(size),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        tlasScratchSize_ = size;
    }

    void AccelBuilder::buildBlas(Model& model) {
        // Destroy previous BLAS if rebuilding
        auto it = blasMap_.find(&model);
        if (it != blasMap_.end()) {
            if (it->second != VK_NULL_HANDLE) {
                vkDestroyAccelerationStructureKHR_(device_.device(), it->second, nullptr);
            }
            blasMap_.erase(it);
            blasBuffers_.erase(&model);
        }

        VkBuffer vertexBuf = model.getVertexBuffer();
        VkBuffer indexBuf  = model.getIndexBuffer();
        if (!vertexBuf) {
            Logger::warn(LogChannel::Scene, "[AccelBuilder] Model has no vertex buffer, skipping BLAS");
            return;
        }

        VkDeviceAddress vertexAddress = model.getVertexBufferAddress();
        bool hasIndices = (indexBuf != VK_NULL_HANDLE);
        VkDeviceAddress indexAddress = 0;
        if (hasIndices) {
            indexAddress = model.getIndexBufferAddress();
        }

        uint32_t vertexCount = model.getVertexCount();
        uint32_t indexCount  = model.getIndexCount();
        VkAccelerationStructureGeometryKHR geometry{};
        geometry.sType        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        geometry.flags        = VK_GEOMETRY_OPAQUE_BIT_KHR;
        geometry.geometry.triangles.sType =
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
        geometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
        geometry.geometry.triangles.vertexData.deviceAddress = vertexAddress +
            offsetof(Model::Vertex, position);
        geometry.geometry.triangles.vertexStride = sizeof(Model::Vertex);
        geometry.geometry.triangles.maxVertex    = vertexCount;
        if (hasIndices) {
            geometry.geometry.triangles.indexType      = VK_INDEX_TYPE_UINT32;
            geometry.geometry.triangles.indexData.deviceAddress = indexAddress;
        } else {
            geometry.geometry.triangles.indexType      = VK_INDEX_TYPE_NONE_KHR;
        }

        // Build size info
        VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
        buildInfo.sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        buildInfo.type          = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        buildInfo.flags         = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        buildInfo.geometryCount = 1;
        buildInfo.pGeometries   = &geometry;

        VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
        sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
        vkGetAccelerationStructureBuildSizesKHR_(device_.device(),
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &buildInfo, &buildInfo.geometryCount, &sizeInfo);

        // Scratch buffer
        createBlasScratch(sizeInfo.buildScratchSize);

        // BLAS buffer
        auto blasBuffer = std::make_unique<Buffer>(device_,
            1, static_cast<uint32_t>(sizeInfo.accelerationStructureSize),
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        // Create BLAS
        VkAccelerationStructureCreateInfoKHR createInfo{};
        createInfo.sType  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        createInfo.buffer = blasBuffer->getBuffer();
        createInfo.size   = sizeInfo.accelerationStructureSize;
        createInfo.type   = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

        VkAccelerationStructureKHR blas = VK_NULL_HANDLE;
        VkResult result = vkCreateAccelerationStructureKHR_(device_.device(), &createInfo, nullptr, &blas);
        if (result != VK_SUCCESS || blas == VK_NULL_HANDLE) {
            Logger::error(LogChannel::Scene, "[AccelBuilder] Failed to create BLAS");
            return;
        }

        // Build on device via one-time command
        buildInfo.dstAccelerationStructure = blas;
        buildInfo.scratchData.deviceAddress = blasScratch_->getDeviceAddress();

        VkAccelerationStructureBuildRangeInfoKHR rangeInfo{};
        rangeInfo.primitiveCount  = hasIndices ? (indexCount / 3) : (vertexCount / 3);
        rangeInfo.primitiveOffset = 0;
        rangeInfo.firstVertex     = 0;
        rangeInfo.transformOffset = 0;
        const VkAccelerationStructureBuildRangeInfoKHR* pRangeInfo = &rangeInfo;

        VkCommandBuffer cmd = device_.beginSingleTimeCommands();
        vkCmdBuildAccelerationStructuresKHR_(cmd, 1, &buildInfo, &pRangeInfo);
        device_.endSingleTimeCommands(cmd);

        // Store
        blasMap_[&model]    = blas;
        blasBuffers_[&model] = std::move(blasBuffer);

        Logger::info(LogChannel::Scene, "[AccelBuilder] Built BLAS for model \"",
            model.getFilePath(), "\" (", sizeInfo.accelerationStructureSize, " bytes)");
    }

    void AccelBuilder::destroyBlas(const Model& model) {
        auto it = blasMap_.find(&model);
        if (it != blasMap_.end()) {
            if (it->second != VK_NULL_HANDLE) {
                vkDestroyAccelerationStructureKHR_(device_.device(), it->second, nullptr);
            }
            blasMap_.erase(it);
            blasBuffers_.erase(&model);
        }
    }

    VkAccelerationStructureKHR AccelBuilder::rebuildTlas(
        const std::vector<std::pair<glm::mat4, VkAccelerationStructureKHR>>& instances,
        VkCommandBuffer cmd) {

        uint32_t instanceCount = static_cast<uint32_t>(instances.size());

        // Build instance buffer on host
        std::vector<VkAccelerationStructureInstanceKHR> instanceData(instanceCount);
        for (uint32_t i = 0; i < instanceCount; ++i) {
            const auto& [transform, blas] = instances[i];

            // Convert glm mat4 to VkTransformMatrixKHR (row-major 3x4)
            VkTransformMatrixKHR vkTransform;
            for (int row = 0; row < 3; ++row) {
                for (int col = 0; col < 4; ++col) {
                    vkTransform.matrix[row][col] = transform[col][row];
                }
            }

            VkAccelerationStructureDeviceAddressInfoKHR addrInfo{};
            addrInfo.sType                 = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
            addrInfo.accelerationStructure = blas;
            VkDeviceAddress blasAddr = vkGetAccelerationStructureDeviceAddressKHR_(
                device_.device(), &addrInfo);

            VkAccelerationStructureInstanceKHR inst{};
            std::memcpy(&inst.transform, &vkTransform, sizeof(VkTransformMatrixKHR));
            inst.instanceCustomIndex                    = i;
            inst.mask                                   = 0xFF;
            inst.instanceShaderBindingTableRecordOffset = 0;
            inst.flags                                  = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
            inst.accelerationStructureReference          = blasAddr;
            instanceData[i] = inst;
        }

        // Upload instance data
        VkDeviceSize instanceBufferSize = sizeof(VkAccelerationStructureInstanceKHR) * instanceCount;
        tlasInstanceBuffer_ = std::make_unique<Buffer>(device_,
            sizeof(VkAccelerationStructureInstanceKHR), instanceCount,
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        tlasInstanceBuffer_->map();
        tlasInstanceBuffer_->writeToBuffer(instanceData.data(), instanceBufferSize);

        // Geometry info for TLAS
        VkAccelerationStructureGeometryKHR geometry{};
        geometry.sType        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
        geometry.flags        = VK_GEOMETRY_OPAQUE_BIT_KHR;
        geometry.geometry.instances.sType =
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
        geometry.geometry.instances.arrayOfPointers = VK_FALSE;
        geometry.geometry.instances.data.deviceAddress =
            tlasInstanceBuffer_->getDeviceAddress();

        // Build size info
        VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
        buildInfo.sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        buildInfo.type          = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        buildInfo.flags         = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        buildInfo.geometryCount = 1;
        buildInfo.pGeometries   = &geometry;

        VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
        sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
        vkGetAccelerationStructureBuildSizesKHR_(device_.device(),
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &buildInfo, &buildInfo.geometryCount, &sizeInfo);

        createTlasScratch(sizeInfo.buildScratchSize);

        // Destroy previous TLAS
        if (tlas_ != VK_NULL_HANDLE) {
            vkDestroyAccelerationStructureKHR_(device_.device(), tlas_, nullptr);
            tlas_ = VK_NULL_HANDLE;
            tlasBuffer_.reset();
        }

        // Create new TLAS buffer
        tlasBuffer_ = std::make_unique<Buffer>(device_,
            1, static_cast<uint32_t>(sizeInfo.accelerationStructureSize),
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VkAccelerationStructureCreateInfoKHR createInfo{};
        createInfo.sType  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        createInfo.buffer = tlasBuffer_->getBuffer();
        createInfo.size   = sizeInfo.accelerationStructureSize;
        createInfo.type   = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;

        VkResult result = vkCreateAccelerationStructureKHR_(device_.device(), &createInfo, nullptr, &tlas_);
        if (result != VK_SUCCESS || tlas_ == VK_NULL_HANDLE) {
            Logger::error(LogChannel::Render, "[AccelBuilder] Failed to create TLAS");
            return VK_NULL_HANDLE;
        }

        // Build TLAS
        buildInfo.dstAccelerationStructure = tlas_;
        buildInfo.scratchData.deviceAddress = tlasScratch_->getDeviceAddress();

        VkAccelerationStructureBuildRangeInfoKHR rangeInfo{};
        rangeInfo.primitiveCount  = instanceCount;
        rangeInfo.primitiveOffset = 0;
        rangeInfo.firstVertex     = 0;
        rangeInfo.transformOffset = 0;
        const VkAccelerationStructureBuildRangeInfoKHR* pRangeInfo = &rangeInfo;

        vkCmdBuildAccelerationStructuresKHR_(cmd, 1, &buildInfo, &pRangeInfo);

        // Get device address
        VkAccelerationStructureDeviceAddressInfoKHR addrInfo{};
        addrInfo.sType                 = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        addrInfo.accelerationStructure = tlas_;
        tlasAddress_ = vkGetAccelerationStructureDeviceAddressKHR_(device_.device(), &addrInfo);

        return tlas_;
    }

}  // namespace engine