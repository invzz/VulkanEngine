#pragma once

#ifndef MODEL_PATH
#define MODEL_PATH "assets/models/"
#endif

// Ensure GLM uses radians for all angle measurements
#define GLM_FORCE_RADIANS
// Ensure depth range is [0, 1] for Vulkan
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <memory>
#include <vector>

#include "Device.hpp"

namespace engine {

  class Model
  {
  public:
    struct Vertex
    {
      glm::vec3                                             position;
      glm::vec3                                             color;
      glm::vec3                                             normal;
      glm::vec2                                             uv;
      static std::vector<VkVertexInputBindingDescription>   getBindingDescriptions();
      static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
      bool operator==(const Vertex& other) const { return position == other.position && color == other.color && normal == other.normal && uv == other.uv; }
    };

    struct Builder
    {
      std::vector<Vertex>   vertices{};
      std::vector<uint32_t> indices{};

      void loadModelFromFile(const std::string& filepath);
    };

    explicit Model(Device& device, const Builder& builder);
    ~Model();

    // delete copy and move constructors and assignment operators
    Model(const Model&)            = delete;
    Model& operator=(const Model&) = delete;

    static std::unique_ptr<Model> createModelFromFile(Device& device, const std::string& filepath);

    void bind(VkCommandBuffer commandBuffer) const;
    void draw(VkCommandBuffer commandBuffer) const;

  private:
    Device& device;

    VkBuffer       vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    uint32_t       vertexCount;

    bool hasIndexBuffer = false;

    VkBuffer       indexBuffer;
    VkDeviceMemory indexBufferMemory;
    uint32_t       indexCount;

    void createVertexBuffers(const std::vector<Vertex>& vertices);
    void createIndexBuffers(const std::vector<uint32_t>& indices);
    // Additional model data members would go here
  };

} // namespace engine
