#include "RenderContext.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "Engine/Graphics/SwapChain.hpp"
#include "Engine/Scene/components/DirectionalLightComponent.hpp"
#include "Engine/Scene/components/PointLightComponent.hpp"
#include "Engine/Scene/components/SpotLightComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"

namespace engine {

  namespace {
    float computeSpotLightRadius2(const SpotLightComponent& spotLight)
    {
      constexpr float kMinContribution = 0.01f;

      const float target = std::max(spotLight.intensity / kMinContribution, 0.0f);

      const float c = std::max(spotLight.constantAttenuation, 0.0f);
      const float l = std::max(spotLight.linearAttenuation, 0.0f);
      const float q = std::max(spotLight.quadraticAttenuation, 0.0f);

      if (l == 0.0f && q == 0.0f)
      {
        return std::numeric_limits<float>::infinity();
      }

      const float A = q;
      const float B = l;
      const float C = c - target;

      if (A == 0.0f)
      {
        if (B == 0.0f) return std::numeric_limits<float>::infinity();
        const float d = (target - c) / B;
        return d > 0.0f ? d * d : 0.0f;
      }

      const float discriminant = B * B - 4.0f * A * C;
      if (discriminant <= 0.0f)
      {
        return 0.0f;
      }

      const float sqrtD = std::sqrt(discriminant);
      const float d0    = (-B + sqrtD) / (2.0f * A);
      const float d1    = (-B - sqrtD) / (2.0f * A);
      const float d     = std::max(d0, d1);

      return d > 0.0f ? d * d : 0.0f;
    }
  } // namespace

  RenderContext::RenderContext(Device& device, MeshManager& meshManager, VkDescriptorImageInfo hzbImageInfo)
      : device_{device}, meshManager_{meshManager}, uboBuffers_(SwapChain::maxFramesInFlight()), globalDescriptorSets_(SwapChain::maxFramesInFlight())
  {
    createDescriptorPool();
    createGlobalSetLayout();
    createUBOBuffers();
    // Start with a conservative capacity and grow on demand.
    createLightBuffers(64, 16, 64);
    createGlobalDescriptorSets();

    for (int i = 0; i < SwapChain::maxFramesInFlight(); i++)
    {
      updateLightDescriptorSets(i);
    }

    // Initialize with dummy or provided HZB info
    for (int i = 0; i < SwapChain::maxFramesInFlight(); i++)
    {
      updateHZBDescriptorPrev(i, hzbImageInfo);
      updateHZBDescriptorCurrent(i, hzbImageInfo);
    }
  }

  void RenderContext::createDescriptorPool()
  {
    globalPool_ = DescriptorPool::Builder(device_)
                          // We allocate two global descriptor sets per frame: prev-HZB + current-HZB.
                          .setMaxSets(static_cast<uint32_t>(SwapChain::maxFramesInFlight() * 2))
                          .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, static_cast<uint32_t>(SwapChain::maxFramesInFlight() * 2))
                          // Storage buffers: mesh buffer + 3 light buffers per set.
                          .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, static_cast<uint32_t>(SwapChain::maxFramesInFlight() * 2 * 4))
                          .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<uint32_t>(SwapChain::maxFramesInFlight() * 2))
                          .build();
  }

  void RenderContext::createGlobalSetLayout()
  {
    globalSetLayout_ = DescriptorSetLayout::Builder(device_)
                               .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT)
                               .addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT)
                               .addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_TASK_BIT_EXT)
                               // Dynamic lights
                               .addBinding(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
                               .addBinding(4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
                               .addBinding(5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
                               .build();
  }

  void RenderContext::createUBOBuffers()
  {
    for (auto& buffer : uboBuffers_)
    {
      buffer = std::make_unique<Buffer>(device_,
                                        sizeof(GlobalUbo),
                                        1,
                                        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                                        device_.getProperties().limits.minUniformBufferOffsetAlignment);
      buffer->map();
    }
  }

  void RenderContext::createLightBuffers(size_t pointCapacity, size_t directionalCapacity, size_t spotCapacity)
  {
    pointLightCapacity_       = pointCapacity;
    directionalLightCapacity_ = directionalCapacity;
    spotLightCapacity_        = spotCapacity;

    pointLightBuffers_.resize(SwapChain::maxFramesInFlight());
    directionalLightBuffers_.resize(SwapChain::maxFramesInFlight());
    spotLightBuffers_.resize(SwapChain::maxFramesInFlight());

    for (int i = 0; i < SwapChain::maxFramesInFlight(); i++)
    {
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

  void RenderContext::createGlobalDescriptorSets()
  {
    globalDescriptorSetsCurrentHzb_.resize(globalDescriptorSets_.size());

    for (size_t i = 0; i < globalDescriptorSets_.size(); i++)
    {
      auto bufferInfo = uboBuffers_[i]->descriptorInfo();
      auto meshInfo   = meshManager_.getDescriptorInfo();

      // Binding 2 (HZB) will be updated later, but we need to write something or use updateHZBDescriptor
      // DescriptorWriter requires all bindings? No, it builds what is added.
      // But if we don't write binding 2, validation might complain if we use it.
      // We will update it immediately in constructor.

      DescriptorWriter(*globalSetLayout_, *globalPool_)
              .writeBuffer(0, &bufferInfo)
              .writeBuffer(1, &meshInfo)
              //.writeImage(2, ...) // We don't have image info here yet
              .build(globalDescriptorSets_[i]);
      if (globalDescriptorSets_[i] == VK_NULL_HANDLE)
      {
        throw std::runtime_error("failed to allocate global descriptor set (prev HZB)");
      }

      DescriptorWriter(*globalSetLayout_, *globalPool_).writeBuffer(0, &bufferInfo).writeBuffer(1, &meshInfo).build(globalDescriptorSetsCurrentHzb_[i]);
      if (globalDescriptorSetsCurrentHzb_[i] == VK_NULL_HANDLE)
      {
        throw std::runtime_error("failed to allocate global descriptor set (current HZB)");
      }
    }
  }

  void RenderContext::updateLightDescriptorSets(int frameIndex)
  {
    VkDescriptorBufferInfo pointInfo = pointLightBuffers_[frameIndex]->descriptorInfo();
    VkDescriptorBufferInfo dirInfo   = directionalLightBuffers_[frameIndex]->descriptorInfo();
    VkDescriptorBufferInfo spotInfo  = spotLightBuffers_[frameIndex]->descriptorInfo();

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

    // Prev-HZB set
    writeSet(globalDescriptorSets_[frameIndex], 3, pointInfo);
    writeSet(globalDescriptorSets_[frameIndex], 4, dirInfo);
    writeSet(globalDescriptorSets_[frameIndex], 5, spotInfo);

    // Current-HZB set
    writeSet(globalDescriptorSetsCurrentHzb_[frameIndex], 3, pointInfo);
    writeSet(globalDescriptorSetsCurrentHzb_[frameIndex], 4, dirInfo);
    writeSet(globalDescriptorSetsCurrentHzb_[frameIndex], 5, spotInfo);
  }

  static void updateHzbDescriptorSet(Device& device, VkDescriptorSet dstSet, VkDescriptorImageInfo const& hzbImageInfo)
  {
    VkWriteDescriptorSet write{};
    write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet          = dstSet;
    write.dstBinding      = 2;
    write.dstArrayElement = 0;
    write.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.descriptorCount = 1;
    write.pImageInfo      = &hzbImageInfo;

    vkUpdateDescriptorSets(device.device(), 1, &write, 0, nullptr);
  }

  void RenderContext::updateHZBDescriptorPrev(int frameIndex, VkDescriptorImageInfo hzbImageInfo)
  {
    updateHzbDescriptorSet(device_, globalDescriptorSets_[frameIndex], hzbImageInfo);
  }

  void RenderContext::updateHZBDescriptorCurrent(int frameIndex, VkDescriptorImageInfo hzbImageInfo)
  {
    updateHzbDescriptorSet(device_, globalDescriptorSetsCurrentHzb_[frameIndex], hzbImageInfo);
  }

  void RenderContext::updateUBO(int frameIndex, const GlobalUbo& ubo)
  {
    uboBuffers_[frameIndex]->writeToBuffer(&ubo);
    uboBuffers_[frameIndex]->flush();
  }

  RenderContext::LightCounts RenderContext::updateLightBuffers(int frameIndex, Scene& scene)
  {
    // Gather lights from the scene.
    std::vector<PointLight>       pointLights;
    std::vector<DirectionalLight> dirLights;
    std::vector<SpotLight>        spotLights;

    auto& registry = scene.getRegistry();

    {
      auto view = registry.view<PointLightComponent, TransformComponent>();
      pointLights.reserve(view.size_hint());
      for (auto entity : view)
      {
        auto [point, transform] = view.get<PointLightComponent, TransformComponent>(entity);
        PointLight pl{};
        pl.position = glm::vec4(transform.translation, 1.f);
        pl.color    = glm::vec4(point.color, point.intensity);
        pl.radius2  = point.radius * point.radius;
        pointLights.push_back(pl);
      }
    }

    {
      auto view = registry.view<DirectionalLightComponent, TransformComponent>();
      dirLights.reserve(view.size_hint());
      for (auto entity : view)
      {
        auto [dir, transform] = view.get<DirectionalLightComponent, TransformComponent>(entity);
        DirectionalLight dl{};

        if (dir.useTargetPoint)
        {
          transform.lookAt(dir.targetPoint);
        }

        glm::vec3 direction = transform.getForwardDir();
        dl.direction        = glm::vec4(glm::normalize(direction), 0.f);
        dl.color            = glm::vec4(dir.color, dir.intensity);
        dirLights.push_back(dl);
      }
    }

    {
      auto view = registry.view<SpotLightComponent, TransformComponent>();
      spotLights.reserve(view.size_hint());
      for (auto entity : view)
      {
        auto [spot, transform] = view.get<SpotLightComponent, TransformComponent>(entity);
        SpotLight sl{};

        if (spot.useTargetPoint)
        {
          transform.lookAt(spot.targetPoint);
        }

        glm::vec3 direction = transform.getForwardDir();

        sl.position       = glm::vec4(transform.translation, 1.f);
        sl.direction      = glm::vec4(glm::normalize(direction), glm::cos(glm::radians(spot.innerCutoffAngle)));
        sl.color          = glm::vec4(spot.color, spot.intensity);
        sl.outerCutoff    = glm::cos(glm::radians(spot.outerCutoffAngle));
        sl.constantAtten  = spot.constantAttenuation;
        sl.linearAtten    = spot.linearAttenuation;
        sl.quadraticAtten = spot.quadraticAttenuation;
        sl.radius2        = computeSpotLightRadius2(spot);

        spotLights.push_back(sl);
      }
    }

    auto nextPow2 = [](size_t v) {
      size_t p = 1;
      while (p < v)
        p <<= 1;
      return p;
    };

    bool resized = false;
    if (pointLights.size() > pointLightCapacity_)
    {
      pointLightCapacity_ = nextPow2(pointLights.size());
      resized             = true;
    }
    if (dirLights.size() > directionalLightCapacity_)
    {
      directionalLightCapacity_ = nextPow2(dirLights.size());
      resized                   = true;
    }
    if (spotLights.size() > spotLightCapacity_)
    {
      spotLightCapacity_ = nextPow2(spotLights.size());
      resized            = true;
    }

    if (resized)
    {
      // Recreate all per-frame buffers so every in-flight frame has matching capacity.
      createLightBuffers(pointLightCapacity_, directionalLightCapacity_, spotLightCapacity_);
      for (int i = 0; i < SwapChain::maxFramesInFlight(); i++)
      {
        updateLightDescriptorSets(i);
      }
    }
    else
    {
      // Always refresh descriptors for this frame (cheap), in case buffers were re-created earlier.
      updateLightDescriptorSets(frameIndex);
    }

    if (!pointLights.empty())
    {
      pointLightBuffers_[frameIndex]->writeToBuffer(pointLights.data(), pointLights.size() * sizeof(PointLight));
      pointLightBuffers_[frameIndex]->flush();
    }
    if (!dirLights.empty())
    {
      directionalLightBuffers_[frameIndex]->writeToBuffer(dirLights.data(), dirLights.size() * sizeof(DirectionalLight));
      directionalLightBuffers_[frameIndex]->flush();
    }
    if (!spotLights.empty())
    {
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

} // namespace engine
