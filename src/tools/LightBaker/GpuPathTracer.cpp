#include "Tools/LightBaker/GpuPathTracer.hpp"

#include <fstream>
#include <glm/glm.hpp>
#include <iostream>
#include <stdexcept>

#include "Engine/Graphics/Descriptors.hpp"
#include "Engine/Graphics/DeviceMemory.hpp"
#include "ModelLib/Resources/Model.hpp" // for AABB

namespace LightBaker {

  GpuPathTracer::GpuPathTracer(engine::Device& device) : device_(device)
  {
    createDescriptorSetLayout();
    createComputePipeline();
    createDescriptorPool();
    std::cout << "[GpuPathTracer] Compute-based baker initialized (placeholder)" << '\n';
  }

  GpuPathTracer::~GpuPathTracer()
  {
    if (computePipeline_ != VK_NULL_HANDLE) vkDestroyPipeline(device_.device(), computePipeline_, nullptr);
    if (pipelineLayout_ != VK_NULL_HANDLE) vkDestroyPipelineLayout(device_.device(), pipelineLayout_, nullptr);
    // descriptorSetLayout_ and descriptorPool_ will be destroyed by their RAII objects
  }

  bool GpuPathTracer::isReady() const
  {
    return computePipeline_ != VK_NULL_HANDLE;
  }

  void GpuPathTracer::createDescriptorSetLayout()
  {
    // Bindings:
    // 0 -> storage image (out HDR)
    // 1 -> params SSBO
    // 2 -> triangles SSBO
    // 3 -> normal storage image (debug / denoiser input)
    descriptorSetLayout_ = engine::DescriptorSetLayout::Builder(device_)
                                   .addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
                                   .addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                                   .addBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                                   .addBinding(3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
                                   .addBinding(4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT) // BVH nodes
                                   .build();
  }

  void GpuPathTracer::createComputePipeline()
  {
    std::string const shaderPath = std::string(SHADER_PATH) + "/bake_pathtrace.comp.spv";
    std::cout << "[GpuPathTracer] Loading shader from: " << shaderPath << '\n';
    std::ifstream file(shaderPath, std::ios::ate | std::ios::binary);

    if (!file.is_open())
    {
      std::cerr << "[GpuPathTracer] Failed to open shader file: " << shaderPath << " - compute baker disabled" << '\n';
      return;
    }

    auto              fileSize = static_cast<long>(file.tellg());
    std::vector<char> shaderCode(fileSize);
    file.seekg(0);
    file.read(shaderCode.data(), fileSize);
    file.close();

    VkShaderModule computeShaderModule = createShaderModule(shaderCode);

    VkPipelineShaderStageCreateInfo const shaderStageInfo{
            .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage  = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = computeShaderModule,
            .pName  = "main",
    };

    // No push constants for the placeholder; we'll pass sun intensity via a small SSBO or later push constants
    VkPushConstantRange const pushConstantRange{
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .offset     = 0,
            .size       = 0,
    };

    VkDescriptorSetLayout            layout = descriptorSetLayout_->getDescriptorSetLayout();
    VkPipelineLayoutCreateInfo const pipelineLayoutInfo{
            .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount         = 1,
            .pSetLayouts            = &layout,
            .pushConstantRangeCount = 0,
            .pPushConstantRanges    = nullptr,
    };

    if (vkCreatePipelineLayout(device_.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout_) != VK_SUCCESS)
    {
      throw std::runtime_error("Failed to create compute pipeline layout!");
    }

    VkComputePipelineCreateInfo const pipelineInfo{
            .sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage  = shaderStageInfo,
            .layout = pipelineLayout_,
    };

    if (vkCreateComputePipelines(device_.device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &computePipeline_) != VK_SUCCESS)
    {
      vkDestroyShaderModule(device_.device(), computeShaderModule, nullptr);
      throw std::runtime_error("Failed to create compute pipeline!");
    }

    vkDestroyShaderModule(device_.device(), computeShaderModule, nullptr);
  }

  void GpuPathTracer::createDescriptorPool()
  {
    // Use a much larger pool to accommodate scenes with many primitives; free descriptors after use.
    descriptorPool_ = engine::DescriptorPool::Builder(device_)
                              .setMaxSets(1024)
                              .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1024)
                              .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1024)
                              .setPoolFlags(VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT)
                              .build();
  }

  VkShaderModule GpuPathTracer::createShaderModule(const std::vector<char>& code)
  {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();

    std::vector<uint32_t> codeAligned((code.size() + 3) / 4);
    std::memcpy(codeAligned.data(), code.data(), code.size());
    createInfo.pCode = codeAligned.data();

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device_.device(), &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
    {
      throw std::runtime_error("Failed to create compute shader module!");
    }
    return shaderModule;
  }

  // Simple CPU BVH builder (median split) and GPU traversal integration.
  // NOTE: not optimized (no SAH) but gives correct, fast-enough ray culling for medium scenes.
  namespace {
    struct BVHNodeCPU
    {
      glm::vec4 min;
      glm::vec4 max;
      int       left     = -1;
      int       right    = -1;
      int       triStart = 0;
      int       triCount = 0;
      int       pad      = 0;
    };

    // Build a median-split BVH and produce node list + ordered triangle list
    int buildBVHRecursive(const std::vector<LightmapBaker::Tri>& tris,
                          const std::vector<glm::vec3>&          centroids,
                          std::vector<int>&                      indices,
                          int                                    start,
                          int                                    end,
                          std::vector<BVHNodeCPU>&               nodes,
                          std::vector<LightmapBaker::Tri>&       orderedTris,
                          int                                    leafSize)
    {
      int nodeIdx = static_cast<int>(nodes.size());
      nodes.emplace_back();

      // compute bounds for this range
      glm::vec3 bmin(std::numeric_limits<float>::infinity());
      glm::vec3 bmax(-std::numeric_limits<float>::infinity());
      for (int i = start; i < end; ++i)
      {
        const auto& t = tris[indices[i]];
        bmin          = glm::min(bmin, t.p0);
        bmin          = glm::min(bmin, t.p1);
        bmin          = glm::min(bmin, t.p2);
        bmax          = glm::max(bmax, t.p0);
        bmax          = glm::max(bmax, t.p1);
        bmax          = glm::max(bmax, t.p2);
      }
      nodes[nodeIdx].min = glm::vec4(bmin, 0.0f);
      nodes[nodeIdx].max = glm::vec4(bmax, 0.0f);

      int count = end - start;
      if (count <= leafSize)
      {
        // make leaf: append triangles to ordered list
        nodes[nodeIdx].triStart = static_cast<int>(orderedTris.size());
        nodes[nodeIdx].triCount = count;
        nodes[nodeIdx].left     = -1;
        nodes[nodeIdx].right    = -1;
        for (int i = start; i < end; ++i)
        {
          orderedTris.push_back(tris[indices[i]]);
        }
        return nodeIdx;
      }

      // split along longest axis of bounds
      glm::vec3 ext  = bmax - bmin;
      int       axis = 0;
      if (ext.y > ext.x) axis = 1;
      if (ext.z > ext[axis]) axis = 2;

      // If extent along axis is near zero, fallback to median split to avoid div-by-zero
      float axisLen = ext[axis];
      if (axisLen < 1e-5f)
      {
        std::sort(indices.begin() + start, indices.begin() + end, [&](int a, int b) { return centroids[a][axis] < centroids[b][axis]; });
        int mid              = (start + end) / 2;
        int left             = buildBVHRecursive(tris, centroids, indices, start, mid, nodes, orderedTris, leafSize);
        int right            = buildBVHRecursive(tris, centroids, indices, mid, end, nodes, orderedTris, leafSize);
        nodes[nodeIdx].left  = left;
        nodes[nodeIdx].right = right;
        return nodeIdx;
      }

      // Binned SAH
      const int BIN_COUNT = 16;
      struct Bin
      {
        glm::vec3 bmin;
        glm::vec3 bmax;
        int       count;
      };
      Bin bins[BIN_COUNT];
      for (int i = 0; i < BIN_COUNT; ++i)
      {
        bins[i].bmin  = glm::vec3(std::numeric_limits<float>::infinity());
        bins[i].bmax  = glm::vec3(-std::numeric_limits<float>::infinity());
        bins[i].count = 0;
      }

      // compute centroid bounds along chosen axis
      float cmin = centroids[indices[start]][axis];
      float cmax = cmin;
      for (int i = start + 1; i < end; ++i)
      {
        float v = centroids[indices[i]][axis];
        cmin    = std::min(cmin, v);
        cmax    = std::max(cmax, v);
      }
      float cRange = cmax - cmin;
      if (cRange <= 1e-8f)
      {
        // degenerate: fall back to median
        std::sort(indices.begin() + start, indices.begin() + end, [&](int a, int b) { return centroids[a][axis] < centroids[b][axis]; });
        int mid              = (start + end) / 2;
        int left             = buildBVHRecursive(tris, centroids, indices, start, mid, nodes, orderedTris, leafSize);
        int right            = buildBVHRecursive(tris, centroids, indices, mid, end, nodes, orderedTris, leafSize);
        nodes[nodeIdx].left  = left;
        nodes[nodeIdx].right = right;
        return nodeIdx;
      }

      // fill bins
      for (int i = start; i < end; ++i)
      {
        int   idx = indices[i];
        float v   = centroids[idx][axis];
        int   bin = int(((v - cmin) / cRange) * float(BIN_COUNT));
        if (bin < 0) bin = 0;
        if (bin >= BIN_COUNT) bin = BIN_COUNT - 1;
        bins[bin].count++;
        // expand with triangle geometry
        bins[bin].bmin = glm::min(bins[bin].bmin, tris[idx].p0);
        bins[bin].bmin = glm::min(bins[bin].bmin, tris[idx].p1);
        bins[bin].bmin = glm::min(bins[bin].bmin, tris[idx].p2);
        bins[bin].bmax = glm::max(bins[bin].bmax, tris[idx].p0);
        bins[bin].bmax = glm::max(bins[bin].bmax, tris[idx].p1);
        bins[bin].bmax = glm::max(bins[bin].bmax, tris[idx].p2);
      }

      // prefix/suffix accumulators
      int       leftCount[BIN_COUNT];
      glm::vec3 leftMin[BIN_COUNT];
      glm::vec3 leftMax[BIN_COUNT];
      int       rightCount[BIN_COUNT];
      glm::vec3 rightMin[BIN_COUNT];
      glm::vec3 rightMax[BIN_COUNT];

      int       accCount = 0;
      glm::vec3 accMin   = glm::vec3(std::numeric_limits<float>::infinity());
      glm::vec3 accMax   = glm::vec3(-std::numeric_limits<float>::infinity());
      for (int i = 0; i < BIN_COUNT; ++i)
      {
        accCount += bins[i].count;
        if (bins[i].count > 0)
        {
          accMin = glm::min(accMin, bins[i].bmin);
          accMax = glm::max(accMax, bins[i].bmax);
        }
        leftCount[i] = accCount;
        leftMin[i]   = accMin;
        leftMax[i]   = accMax;
      }

      accCount = 0;
      accMin   = glm::vec3(std::numeric_limits<float>::infinity());
      accMax   = glm::vec3(-std::numeric_limits<float>::infinity());
      for (int i = BIN_COUNT - 1; i >= 0; --i)
      {
        accCount += bins[i].count;
        if (bins[i].count > 0)
        {
          accMin = glm::min(accMin, bins[i].bmin);
          accMax = glm::max(accMax, bins[i].bmax);
        }
        rightCount[i] = accCount;
        rightMin[i]   = accMin;
        rightMax[i]   = accMax;
      }

      auto area = [](const glm::vec3& amin, const glm::vec3& amax) {
        glm::vec3 e = amax - amin;
        return e.x * e.y + e.y * e.z + e.z * e.x; // half-surface area proxy
      };

      float bestCost     = std::numeric_limits<float>::infinity();
      int   bestSplitBin = -1;
      float currArea     = area(bmin, bmax);
      float leafCost     = float(count) * currArea; // cost if leaf

      for (int i = 0; i < BIN_COUNT - 1; ++i)
      {
        if (leftCount[i] == 0 || rightCount[i + 1] == 0) continue;
        float leftA  = area(leftMin[i], leftMax[i]);
        float rightA = area(rightMin[i + 1], rightMax[i + 1]);
        float cost   = float(leftCount[i]) * leftA + float(rightCount[i + 1]) * rightA;
        if (cost < bestCost)
        {
          bestCost     = cost;
          bestSplitBin = i + 1;
        }
      }

      if (bestSplitBin < 0 || bestCost >= leafCost)
      {
        // no beneficial split -> make leaf
        nodes[nodeIdx].triStart = static_cast<int>(orderedTris.size());
        nodes[nodeIdx].triCount = count;
        nodes[nodeIdx].left     = -1;
        nodes[nodeIdx].right    = -1;
        for (int i = start; i < end; ++i)
        {
          orderedTris.push_back(tris[indices[i]]);
        }
        return nodeIdx;
      }

      // partition indices by split bin
      int mid = start;
      for (int i = start; i < end; ++i)
      {
        int   idx = indices[i];
        float v   = centroids[idx][axis];
        int   bin = int(((v - cmin) / cRange) * float(BIN_COUNT));
        if (bin < 0) bin = 0;
        if (bin >= BIN_COUNT) bin = BIN_COUNT - 1;
        if (bin < bestSplitBin)
        {
          std::swap(indices[mid++], indices[i]);
        }
      }

      int left             = buildBVHRecursive(tris, centroids, indices, start, mid, nodes, orderedTris, leafSize);
      int right            = buildBVHRecursive(tris, centroids, indices, mid, end, nodes, orderedTris, leafSize);
      nodes[nodeIdx].left  = left;
      nodes[nodeIdx].right = right;
      return nodeIdx;
    }
  } // namespace

  Result GpuPathTracer::bakeTrianglesGPU(const std::vector<LightmapBaker::Tri>& tris, const Config& cfg)
  {
    Result res;
    if (tris.empty())
    {
      std::cerr << "[GpuPathTracer] No triangles supplied\n";
      return res;
    }

    // Choose a valid resolution if none provided (mirror CPU heuristic)
    int w = cfg.width;
    int h = cfg.height;

    std::cerr << "[GpuPathTracer] computePipeline_ is " << (computePipeline_ == VK_NULL_HANDLE ? "NULL" : "OK") << "\n";
    if (w <= 0 || h <= 0)
    {
      engine::AABB bounds;
      for (const auto& t : tris)
      {
        bounds.expand(t.p0);
        bounds.expand(t.p1);
        bounds.expand(t.p2);
      }
      float sceneExtent = glm::length(bounds.max - bounds.min);
      int   choice      = sceneExtent > 5.0f ? 512 : 256;
      w                 = choice;
      h                 = choice;
      std::cerr << "[GpuPathTracer] Warning: no resolution provided; selecting " << choice << " based on scene extent\n";
    }

    res.width  = w;
    res.height = h;
    res.hdrPixels.assign(static_cast<size_t>(res.width) * static_cast<size_t>(res.height) * 3, 0.0f);

    if (computePipeline_ == VK_NULL_HANDLE)
    {
      std::cerr << "[GpuPathTracer] Compute pipeline not available; falling back to CPU bake\n";
      // no fallback here to mantain separation of CPU/GPU bakers
      throw std::runtime_error("Compute pipeline not available");
    }

    // Build BVH (simple median-split) and produce ordered triangle list
    std::vector<int> indices(tris.size());
    std::iota(indices.begin(), indices.end(), 0);
    std::vector<glm::vec3> centroids(tris.size());
    for (size_t i = 0; i < tris.size(); ++i)
    {
      centroids[i] = (tris[i].p0 + tris[i].p1 + tris[i].p2) / 3.0f;
    }
    std::vector<BVHNodeCPU>         nodesCPU;
    std::vector<LightmapBaker::Tri> orderedTris;
    buildBVHRecursive(tris, centroids, indices, 0, static_cast<int>(indices.size()), nodesCPU, orderedTris, 4);
    std::cerr << "[GpuPathTracer] BVH built: nodes=" << nodesCPU.size() << " orderedTris=" << orderedTris.size() << "\n";

    // Create an offscreen image
    VkFormat   format = VK_FORMAT_R32G32B32A32_SFLOAT;
    VkExtent3D extent{static_cast<uint32_t>(res.width), static_cast<uint32_t>(res.height), 1};

    VkImageCreateInfo imageInfo{.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                                .imageType     = VK_IMAGE_TYPE_2D,
                                .format        = format,
                                .extent        = extent,
                                .mipLevels     = 1,
                                .arrayLayers   = 1,
                                .samples       = VK_SAMPLE_COUNT_1_BIT,
                                .tiling        = VK_IMAGE_TILING_OPTIMAL,
                                .usage         = static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT),
                                .sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
                                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};

    VkImage        lmImage;
    VkDeviceMemory lmMem;
    device_.memory().createImageWithInfo(imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, lmImage, lmMem);

    // Create image view
    std::cerr << "[GpuPathTracer] Creating image view for bake target\n";
    VkImageViewCreateInfo viewInfo{.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                   .image            = lmImage,
                                   .viewType         = VK_IMAGE_VIEW_TYPE_2D,
                                   .format           = format,
                                   .components       = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY},
                                   .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1}};
    VkImageView           lmView;
    if (vkCreateImageView(device_.device(), &viewInfo, nullptr, &lmView) != VK_SUCCESS)
    {
      std::cerr << "[GpuPathTracer] Failed to create image view for bake target\n";
      return res;
    }

    // Allocate descriptor set
    VkDescriptorSet ds = VK_NULL_HANDLE;
    if (!descriptorPool_->allocateDescriptor(descriptorSetLayout_->getDescriptorSetLayout(), ds))
    {
      std::cerr << "[GpuPathTracer] Failed to allocate descriptor set\n";
      // Clean up image resources before returning to avoid leaks
      vkDestroyImageView(device_.device(), lmView, nullptr);
      vkDestroyImage(device_.device(), lmImage, nullptr);
      vkFreeMemory(device_.device(), lmMem, nullptr);
      return res;
    }
    std::cerr << "[GpuPathTracer] Descriptor set allocated\n";

    // Update descriptor set with storage image
    VkDescriptorImageInfo    imageInfoDesc{.sampler = VK_NULL_HANDLE, .imageView = lmView, .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
    engine::DescriptorWriter writer(*descriptorSetLayout_, *descriptorPool_);
    writer.writeImage(0, &imageInfoDesc);
    std::cerr << "[GpuPathTracer] Wrote image descriptor (binding 0)\n";

    // Prepare a small SSBO for params (sun intensity, samples, bounces, denoiser iters, seed, frameIndex)
    struct Params
    {
      float    sunIntensity;
      int      numSamples;
      int      maxBounces;
      int      denoiseIters;
      uint32_t randomSeed;
      uint32_t frameIndex;
      float    sunDirX;
      float    sunDirY;
      float    sunDirZ;
      float    pad0; // pad to 16-byte boundary
    } params{cfg.sunIntensity, cfg.numSamples, cfg.maxBounces, cfg.denoiseIters, cfg.randomSeed, cfg.frameIndex, cfg.sunDirection.x, cfg.sunDirection.y, cfg.sunDirection.z, 0.0f};

    VkBuffer       paramsBuf;
    VkDeviceMemory paramsMem;
    device_.getMemory().createBuffer(sizeof(params), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, paramsBuf, paramsMem);
    void* pData = nullptr;
    vkMapMemory(device_.device(), paramsMem, 0, sizeof(params), 0, &pData);
    memcpy(pData, &params, sizeof(params));
    vkUnmapMemory(device_.device(), paramsMem);

    VkDescriptorBufferInfo paramsDesc{.buffer = paramsBuf, .offset = 0, .range = sizeof(params)};
    writer.writeBuffer(1, &paramsDesc);

    // Create triangle SSBO (host visible and coherent for easy upload) using orderedTris
    size_t         trisSize = orderedTris.size() * sizeof(LightmapBaker::Tri);
    VkBuffer       trisBuf;
    VkDeviceMemory trisMem;
    device_.getMemory().createBuffer(trisSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, trisBuf, trisMem);
    void* trisPtr = nullptr;
    vkMapMemory(device_.device(), trisMem, 0, trisSize, 0, &trisPtr);
    memcpy(trisPtr, orderedTris.data(), trisSize);
    vkUnmapMemory(device_.device(), trisMem);

    VkDescriptorBufferInfo trisDesc{.buffer = trisBuf, .offset = 0, .range = trisSize};
    writer.writeBuffer(2, &trisDesc);

    // Upload BVH nodes
    size_t         nodesSize = nodesCPU.size() * sizeof(BVHNodeCPU);
    VkBuffer       nodesBuf;
    VkDeviceMemory nodesMem;
    device_.getMemory().createBuffer(nodesSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, nodesBuf, nodesMem);
    void* nodesPtr = nullptr;
    vkMapMemory(device_.device(), nodesMem, 0, nodesSize, 0, &nodesPtr);
    memcpy(nodesPtr, nodesCPU.data(), nodesSize);
    vkUnmapMemory(device_.device(), nodesMem);

    VkDescriptorBufferInfo nodesDesc{.buffer = nodesBuf, .offset = 0, .range = nodesSize};
    writer.writeBuffer(4, &nodesDesc);

    // Create a normal image for optional denoising / debugging
    VkImage        normalImage;
    VkDeviceMemory normalMem;
    device_.memory().createImageWithInfo(imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, normalImage, normalMem);

    VkImageViewCreateInfo normalViewInfo = viewInfo;
    normalViewInfo.image                 = normalImage;
    VkImageView normalView;
    vkCreateImageView(device_.device(), &normalViewInfo, nullptr, &normalView);

    VkDescriptorImageInfo normalImageDesc{.sampler = VK_NULL_HANDLE, .imageView = normalView, .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
    writer.writeImage(3, &normalImageDesc);
    std::cerr << "[GpuPathTracer] Wrote normal image descriptor (binding 3)\n";

    // Commit descriptor updates
    std::cerr << "[GpuPathTracer] Committing descriptors (overwrite)\n";
    writer.overwrite(ds);
    std::cerr << "[GpuPathTracer] Descriptors committed\n";

    // Record and dispatch compute
    VkCommandBuffer cmd = device_.beginSingleTimeCommands();

    // Transition both images to general for write
    VkImageMemoryBarrier barrier{};
    barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout                       = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.image                           = lmImage;
    barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel   = 0;
    barrier.subresourceRange.levelCount     = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = 1;
    barrier.srcAccessMask                   = 0;
    barrier.dstAccessMask                   = VK_ACCESS_SHADER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    barrier.image = normalImage;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline_);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout_, 0, 1, &ds, 0, nullptr);

    uint32_t groupX = (res.width + 15) / 16;
    uint32_t groupY = (res.height + 15) / 16;
    std::cerr << "[GpuPathTracer] Dispatching compute shader groups: " << groupX << " x " << groupY << "\n";
    vkCmdDispatch(cmd, groupX, groupY, 1);

    device_.endSingleTimeCommands(cmd);
    std::cerr << "[GpuPathTracer] Compute dispatch complete (endSingleTimeCommands returned)\n";

    // Free descriptor set now that compute work is finished
    {
      std::vector<VkDescriptorSet> dsToFree{ds};
      descriptorPool_->freeDescriptors(dsToFree);
    }

    // Copy image to host-visible buffer
    VkBuffer       hostBuf;
    VkDeviceMemory hostMem;
    VkDeviceSize   hostSize = static_cast<VkDeviceSize>(res.width) * res.height * 4u * sizeof(float);
    device_.getMemory().createBuffer(hostSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, hostBuf, hostMem);
    VkBufferImageCopy region{};
    region.bufferOffset                    = 0;
    region.bufferRowLength                 = 0;
    region.bufferImageHeight               = 0;
    region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel       = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount     = 1;
    region.imageOffset                     = {0, 0, 0};
    region.imageExtent                     = {static_cast<uint32_t>(res.width), static_cast<uint32_t>(res.height), 1};

    std::cerr << "[GpuPathTracer] Starting copyImageToBuffer...\n";
    device_.getMemory().copyImageToBuffer(lmImage, hostBuf, {region}, VK_IMAGE_LAYOUT_GENERAL);
    std::cerr << "[GpuPathTracer] copyImageToBuffer completed\n";

    // Map and read floats
    void* mapped = nullptr;
    vkMapMemory(device_.device(), hostMem, 0, VK_WHOLE_SIZE, 0, &mapped);
    std::cerr << "[GpuPathTracer] Mapped host memory; reading pixels\n";
    float* outFloats  = reinterpret_cast<float*>(mapped);
    size_t pixelCount = static_cast<size_t>(res.width) * res.height;
    for (size_t i = 0; i < pixelCount; ++i)
    {
      res.hdrPixels[(i * 3) + 0] = outFloats[(i * 4) + 0];
      res.hdrPixels[(i * 3) + 1] = outFloats[(i * 4) + 1];
      res.hdrPixels[(i * 3) + 2] = outFloats[(i * 4) + 2];
    }
    vkUnmapMemory(device_.device(), hostMem);

    // Cleanup GPU resources we created for the bake
    vkDestroyImageView(device_.device(), normalView, nullptr);
    vkDestroyImage(device_.device(), normalImage, nullptr);
    vkFreeMemory(device_.device(), normalMem, nullptr);

    vkDestroyBuffer(device_.device(), trisBuf, nullptr);
    vkFreeMemory(device_.device(), trisMem, nullptr);

    vkDestroyBuffer(device_.device(), paramsBuf, nullptr);
    vkFreeMemory(device_.device(), paramsMem, nullptr);

    vkDestroyBuffer(device_.device(), hostBuf, nullptr);
    vkFreeMemory(device_.device(), hostMem, nullptr);

    std::cerr << "[GpuPathTracer] Finished readback; cleaning up GPU resources\n";
    // Quick diagnostic: compute HDR min/max to detect uniform/saturated outputs
    {
      float minV = std::numeric_limits<float>::infinity();
      float maxV = -std::numeric_limits<float>::infinity();
      for (size_t i = 0; i < res.hdrPixels.size(); ++i)
      {
        minV = std::min(minV, res.hdrPixels[i]);
        maxV = std::max(maxV, res.hdrPixels[i]);
      }
      std::cerr << "[GpuPathTracer] Bake HDR range: min=" << minV << " max=" << maxV << "\n";
    }
    vkDestroyImage(device_.device(), lmImage, nullptr);
    vkFreeMemory(device_.device(), lmMem, nullptr);

    return res;
  }

} // namespace LightBaker
