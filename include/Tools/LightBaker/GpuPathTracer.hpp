#pragma once

#include <memory>
#include <vector>

#include "Engine/Graphics/Descriptors.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Tools/LightBaker/LightBaker.hpp"

namespace LightBaker {

  class GpuPathTracer
  {
  public:
    struct Config
    {
      int       width        = 512;
      int       height       = 512;
      int       numSamples   = 64;
      int       maxBounces   = 3; // number of path bounces
      int       denoiseIters = 0; // 0 = disabled
      uint32_t  randomSeed   = 0;
      uint32_t  frameIndex   = 0; // optional accumulation / decorrelation across dispatches
      glm::vec3 sunDirection = glm::vec3(0.0f, 0.0f, 1.0f);
      float     sunIntensity = 1.0f;
    };

    explicit GpuPathTracer(engine::Device& device);
    ~GpuPathTracer();

    // Run a minimal GPU bake. Currently implements a compute-pass placeholder that
    // writes ambient + sun intensity into the output image. This will be extended
    // to a proper path-tracer (NEE, cosine sampling, bounces) in follow-ups.
    Result bakeTrianglesGPU(const std::vector<LightmapBaker::Tri>& tris, const Config& cfg);

    // Query whether the GPU compute pipeline is usable
    bool isReady() const;

  private:
    engine::Device&                              device_;
    std::unique_ptr<engine::DescriptorSetLayout> descriptorSetLayout_;
    VkPipelineLayout                             pipelineLayout_  = VK_NULL_HANDLE;
    VkPipeline                                   computePipeline_ = VK_NULL_HANDLE;
    std::unique_ptr<engine::DescriptorPool>      descriptorPool_;

    VkShaderModule createShaderModule(const std::vector<char>& code);

    void createDescriptorSetLayout();
    void createComputePipeline();
    void createDescriptorPool();
  };

} // namespace LightBaker
