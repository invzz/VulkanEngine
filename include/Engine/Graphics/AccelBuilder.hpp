#ifndef VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_ACCELBUILDER_HPP
#define VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_ACCELBUILDER_HPP

#include <vulkan/vulkan.h>

#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "Engine/Graphics/Device.hpp"

#include "ModelLib/Resources/Model.hpp"

namespace engine {

    /**
     * @brief Manages bottom-level (BLAS) and top-level (TLAS) acceleration structures
     *        for hardware raytracing via ray queries.
     *
     * BLAS: one per Model, built from its vertex + index buffers.
     * TLAS: rebuilt every frame from all visible model instances.
     */
    class AccelBuilder {
       public:
        AccelBuilder(Device& device);
        ~AccelBuilder();

        AccelBuilder(const AccelBuilder&)            = delete;
        AccelBuilder& operator=(const AccelBuilder&) = delete;

        /// Build (or rebuild) a BLAS for the given model. The model must have
        /// valid vertex + index buffers with SHADER_DEVICE_ADDRESS_BIT.
        void buildBlas(Model& model);

        /// Destroy the BLAS associated with a model (e.g. when model is unloaded).
        void destroyBlas(const Model& model);

        /// Rebuild the TLAS from all currently-loaded BLAS instances.
        /// Returns the TLAS handle for binding.
        VkAccelerationStructureKHR rebuildTlas(
            const std::vector<std::pair<glm::mat4, VkAccelerationStructureKHR>>& instances,
            VkCommandBuffer                                                      cmd);

        /// Get the TLAS device address for use in shader descriptors.
        [[nodiscard]] VkDeviceAddress getTlasAddress() const {
            return tlasAddress_;
        }

        /// Get the raw TLAS handle for descriptor binding.
        [[nodiscard]] VkAccelerationStructureKHR getTlas() const {
            std::scoped_lock const lock(mutex_);
            return tlas_;
        }

        /// Get the BLAS handle for a model (returns VK_NULL_HANDLE if not built).
        [[nodiscard]] VkAccelerationStructureKHR getBlas(const Model& model) const {
            std::scoped_lock const lock(mutex_);
            auto                   it = blasMap_.find(&model);
            return (it != blasMap_.end()) ? it->second : VK_NULL_HANDLE;
        }

        /// Load Vulkan RT function pointers.
        void loadFunctions();

       private:
        void   createBlasScratch(VkDeviceSize size);
        void   createTlasScratch(VkDeviceSize size);
        void   destroyScratch();
        Buffer createAccelBuffer(VkAccelerationStructureKHR accel);

        Device&            device_;
        mutable std::mutex mutex_;

        // Per-model BLAS storage
        std::unordered_map<const Model*, VkAccelerationStructureKHR> blasMap_;
        std::unordered_map<const Model*, std::unique_ptr<Buffer>>    blasBuffers_;

        // TLAS (single, rebuilt each frame)
        VkAccelerationStructureKHR tlas_ = VK_NULL_HANDLE;
        std::unique_ptr<Buffer>    tlasBuffer_;
        VkDeviceAddress            tlasAddress_ = 0;

        // Scratch buffers (reallocated when sizes change)
        std::unique_ptr<Buffer> blasScratch_;
        VkDeviceSize            blasScratchSize_ = 0;
        std::unique_ptr<Buffer> tlasScratch_;
        VkDeviceSize            tlasScratchSize_ = 0;
        std::unique_ptr<Buffer> tlasInstanceBuffer_;

        // Function pointers
        PFN_vkCreateAccelerationStructureKHR           vkCreateAccelerationStructureKHR_           = nullptr;
        PFN_vkDestroyAccelerationStructureKHR          vkDestroyAccelerationStructureKHR_          = nullptr;
        PFN_vkGetAccelerationStructureBuildSizesKHR    vkGetAccelerationStructureBuildSizesKHR_    = nullptr;
        PFN_vkCmdBuildAccelerationStructuresKHR        vkCmdBuildAccelerationStructuresKHR_        = nullptr;
        PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR_ = nullptr;
        PFN_vkBuildAccelerationStructuresKHR           vkBuildAccelerationStructuresKHR_           = nullptr;
    };

}  // namespace engine

#endif