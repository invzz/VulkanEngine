#ifndef CUBE_RENDERCONTEXT_HPP
#define CUBE_RENDERCONTEXT_HPP
#include <memory>
#include <vector>

#include "Engine/Graphics/Buffer.hpp"
#include "Engine/Graphics/Descriptors.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/FrameInfo.hpp"

#include "Engine/Graphics/AccelBuilder.hpp"
#include "ModelLib/Resources/MeshManager.hpp"
namespace engine {
    class RenderContext {
       public:
        explicit RenderContext(Device& device, MeshManager& meshManager);
        void updateUBO(int frameIndex, const GlobalUbo& ubo, const GlobalUboCold& uboCold);
        struct LightCounts {
            int point       = 0;
            int directional = 0;
            int spot        = 0;
        };
        LightCounts                   updateLightBuffers(int frameIndex, Scene& scene);
        [[nodiscard]] VkDescriptorSet getGlobalDescriptorSet(int frameIndex) const {
            return globalDescriptorSets_[frameIndex];
        }
        [[nodiscard]] VkDescriptorSetLayout getGlobalSetLayout() const {
            return globalSetLayout_->getDescriptorSetLayout();
        }

        void setAccelBuilder(AccelBuilder* builder) {
            accelBuilder_ = builder;
        }

        VkAccelerationStructureKHR rebuildTlas(
            const std::vector<std::pair<glm::mat4, VkAccelerationStructureKHR>>& instances,
            const std::vector<float>& opacityValues,
            VkCommandBuffer cmd);

        /// Update the mesh buffer descriptor for a specific frame index so newly
        /// loaded models are visible to the shader. Call once per frame per frame index.
        void updateMeshDescriptorSet(int frameIndex);

       private:
        Device&                              device_;
        MeshManager&                         meshManager_;
        AccelBuilder*                        accelBuilder_     = nullptr;
        bool                                 rayTracingEnabled_ = false;
        std::unique_ptr<DescriptorPool>      globalPool_;
        std::unique_ptr<DescriptorSetLayout> globalSetLayout_;
        std::vector<std::unique_ptr<Buffer>> uboBuffers_;
        std::vector<std::unique_ptr<Buffer>> uboColdBuffers_;
        std::vector<std::unique_ptr<Buffer>> pointLightBuffers_;
        std::vector<std::unique_ptr<Buffer>> directionalLightBuffers_;
        std::vector<std::unique_ptr<Buffer>> spotLightBuffers_;
        std::vector<std::unique_ptr<Buffer>> instanceOpacityBuffers_;
        size_t                               pointLightCapacity_       = 0;
        size_t                               directionalLightCapacity_ = 0;
        size_t                               spotLightCapacity_        = 0;
        std::vector<VkDescriptorSet>         globalDescriptorSets_;
        void                                 createDescriptorPool();
        void                                 createGlobalSetLayout();
        void                                 createInstanceOpacityBuffers();
        void                                 createUBOBuffers();
        void                                 createLightBuffers(size_t pointCapacity, size_t directionalCapacity, size_t spotCapacity);
        void                                 createGlobalDescriptorSets();
        void                                 updateLightDescriptorSets(int frameIndex);
        void                                 updateTlasDescriptorSets(int frameIndex);
    };
}  // namespace engine
#endif
