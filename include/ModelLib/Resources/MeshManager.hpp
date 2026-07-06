#ifndef VULKANENGINE_INCLUDE_ENGINE_RESOURCES_MESHMANAGER_HPP
#define VULKANENGINE_INCLUDE_ENGINE_RESOURCES_MESHMANAGER_HPP
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "Engine/Graphics/Buffer.hpp"
#include "Engine/Graphics/Device.hpp"

#include "ModelLib/Resources/Model.hpp"
namespace engine {
    class MeshManager {
       public:
        MeshManager(Device& device);
        ~MeshManager() = default;
        uint32_t                             registerModel(const Model* model);
        [[nodiscard]] VkDescriptorBufferInfo getDescriptorInfo() const;
        static VkDescriptorSetLayoutBinding  getDescriptorSetLayoutBinding();

       private:
        Device&                                    device;
        std::unique_ptr<Buffer>                    meshBuffer;
        std::vector<MeshBuffers>                   meshInfos;
        std::unordered_map<const Model*, uint32_t> modelToId;
        mutable std::mutex                         mutex_;
        void                                       updateBuffer();
    };
}  // namespace engine
#endif
