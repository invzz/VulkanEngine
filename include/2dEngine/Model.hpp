#pragma once

// Ensure GLM uses radians for all angle measurements
#define GLM_FORCE_RADIANS
// Ensure depth range is [0, 1] for Vulkan
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <vector>

#include "Device.hpp"

namespace engine {

  class Model
  {
  public:
    struct Vertex
    {
      glm::vec2                                             position;
      glm::vec3                                             color;
      static std::vector<VkVertexInputBindingDescription>   getBindingDescriptions();
      static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
    };

    explicit Model(Device& device, const std::vector<Vertex>& vertices);
    ~Model();

    // delete copy and move constructors and assignment operators
    Model(const Model&)            = delete;
    Model& operator=(const Model&) = delete;

    void bind(VkCommandBuffer commandBuffer) const;
    void draw(VkCommandBuffer commandBuffer) const;

  private:
    Device&        device;
    VkBuffer       vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    uint32_t       vertexCount;

    void createVertexBuffers(const std::vector<Vertex>& vertices);
    // Additional model data members would go here
  };

} // namespace engine
