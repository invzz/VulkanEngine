#include "ModelLightBaker.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <thread>

#include "BVH.hpp"
#include "Engine/Graphics/Descriptors.hpp"
#include "Engine/Resources/Texture.hpp" // for EXR -> VTEX helper (CPU)

// TinyEXR for EXR writing
#include <tinyexr.h>

// stb image write for preview PNG
#include <stb_image_write.h>

namespace ModelLightBaker {

  ModelLightBaker::ModelLightBaker(engine::Device& device, engine::Model& model, const Options& opts, const std::string& outDir) : device_(device), model_(model), opts_(opts), outDir_(outDir)
  {
    // Ensure output directory exists
    std::filesystem::create_directories(outDir_);

    // Configure preview settings from options
    previewEnabled_ = opts_.preview;
    previewMaxSize_ = opts_.previewMaxSize;
  }

  void ModelLightBaker::bake()
  {
    // Build CPU geometry from source file (use Builder so we have CPU-side verts/indices)
    engine::Model::Builder builder;
    try
    {
      // Choose loader based on extension
      std::string ext;
      {
        std::filesystem::path p(model_.getFilePath());
        ext = p.extension().string();
        for (auto& c : ext)
          c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
      }
      if (ext == ".gltf" || ext == ".glb")
      {
        builder.loadModelFromGLTF(model_.getFilePath());
      }
      else
      {
        builder.loadModelFromFile(model_.getFilePath());
      }
    }
    catch (const std::exception& e)
    {
      std::cerr << "[ModelLightBaker] Failed to load model geometry for baking: " << e.what() << "\n";
      return;
    }

    // Collect triangles
    std::vector<Tri> tris;
    tris.reserve(builder.indices.size() / 3);

    for (size_t i = 0; i + 2 < builder.indices.size(); i += 3)
    {
      uint32_t ia = builder.indices[i + 0];
      uint32_t ib = builder.indices[i + 1];
      uint32_t ic = builder.indices[i + 2];

      const auto& va = builder.vertices[ia];
      const auto& vb = builder.vertices[ib];
      const auto& vc = builder.vertices[ic];

      Tri t;
      t.p0  = va.position;
      t.p1  = vb.position;
      t.p2  = vc.position;
      t.n0  = va.normal;
      t.n1  = vb.normal;
      t.n2  = vc.normal;
      t.uv0 = va.uv;
      t.uv1 = vb.uv;
      t.uv2 = vc.uv;
      tris.push_back(t);
    }

    if (tris.empty())
    {
      std::cerr << "[ModelLightBaker] No triangles found in model (cannot bake)\n";
      return;
    }

    // Compute scene scale and padding
    AABB sceneBounds;
    for (const auto& t : tris)
    {
      sceneBounds.expand(t.p0);
      sceneBounds.expand(t.p1);
      sceneBounds.expand(t.p2);
    }
    float sceneExtent = glm::length(sceneBounds.max - sceneBounds.min);
    float bvhPadding  = opts_.bvhPaddingFraction * sceneExtent; // absolute

    // Build BVH
    BVH bvh;
    bvh.build(tris, bvhPadding);

    const float ambient = 0.05f;
    glm::vec3   sunDir  = glm::normalize(opts_.sunDir);

    // Helper: choose resolution (pow2) based on chunk size (meters) and texels/m rules
    auto chooseResolutionForChunk = [&](float chunkSize) -> int {
      struct Bucket
      {
        float minS, maxS;
        float texMin, texMax;
        int   resMin, resMax;
      };
      std::vector<Bucket> buckets = {
              {0.0f, 1.0f, 128.0f, 256.0f, 128, 256}, // small props
              {1.0f, 5.0f, 64.0f, 128.0f, 128, 512},  // medium
              {5.0f, 10.0f, 32.0f, 64.0f, 256, 1024}, // large walls/floors
              {10.0f, 20.0f, 16.0f, 32.0f, 512, 2048} // very large
      };
      float s = std::clamp(chunkSize, 0.0f, 20.0f);
      for (auto& b : buckets)
      {
        if (s >= b.minS && s <= b.maxS)
        {
          float f = (b.maxS - b.minS) > 0.0f ? (s - b.minS) / (b.maxS - b.minS) : 0.0f;
          // smaller size -> higher texel density, so lerp from texMax to texMin as f goes 0->1
          float texelsPerMeter = (1.0f - f) * b.texMax + f * b.texMin;
          float desired        = std::round(texelsPerMeter * s);
          int   desiredI       = static_cast<int>(std::clamp(desired, static_cast<float>(b.resMin), static_cast<float>(b.resMax)));
          // nearest pow2 in range
          int p = 128;
          while (p < desiredI)
            p <<= 1;
          if (p > b.resMax) p = b.resMax;
          if (p < b.resMin) p = b.resMin;
          return p;
        }
      }
      return 2048; // fallback
    };

    if (opts_.mode == Options::Mode::MESH)
    {
      // Per-mesh baking: iterate sub-meshes
      const auto& subMeshes = model_.getSubMeshes();
      for (size_t sm = 0; sm < subMeshes.size(); ++sm)
      {
        const auto& sub = subMeshes[sm];
        if (sub.indexCount == 0) continue;

        // collect triangles for this sub-mesh
        std::vector<Tri> meshTris;
        meshTris.reserve(sub.indexCount / 3);
        for (uint32_t i = sub.indexOffset; i + 2 < sub.indexOffset + sub.indexCount; i += 3)
        {
          uint32_t ia = builder.indices[i + 0];
          uint32_t ib = builder.indices[i + 1];
          uint32_t ic = builder.indices[i + 2];

          const auto& va = builder.vertices[ia];
          const auto& vb = builder.vertices[ib];
          const auto& vc = builder.vertices[ic];

          Tri t;
          t.p0  = va.position;
          t.p1  = vb.position;
          t.p2  = vc.position;
          t.n0  = va.normal;
          t.n1  = vb.normal;
          t.n2  = vc.normal;
          t.uv0 = va.uv;
          t.uv1 = vb.uv;
          t.uv2 = vc.uv;
          meshTris.push_back(t);
        }

        // compute mesh bounds
        AABB mBounds;
        for (const auto& t : meshTris)
        {
          mBounds.expand(t.p0);
          mBounds.expand(t.p1);
          mBounds.expand(t.p2);
        }
        float chunkSize  = glm::length(mBounds.max - mBounds.min);
        int   resolution = chooseResolutionForChunk(chunkSize);
        std::cout << "[ModelLightBaker] Mesh " << sm << " chunkSize=" << chunkSize << " -> resolution=" << resolution << "\n";

        // Automatic chunking: if X or Z extent is larger than chunk size, split in XZ grid
        float meshExtentX = mBounds.max.x - mBounds.min.x;
        float meshExtentZ = mBounds.max.z - mBounds.min.z;
        float maxExtentXZ = std::max(meshExtentX, meshExtentZ);

        std::function<void(const std::vector<Tri>&, int, int, int, int)> bakeChunk;
        bakeChunk = [&](const std::vector<Tri>& chunkTris, int imageW, int imageH, int chunkX, int chunkZ) {
          if (chunkTris.empty()) return;

          // Build triangle->texel lists for this chunk
          std::vector<std::vector<int>> meshTexelTris(static_cast<size_t>(imageW) * static_cast<size_t>(imageH));
          for (size_t ti = 0; ti < chunkTris.size(); ++ti)
          {
            const Tri& t   = chunkTris[ti];
            glm::vec2  uv0 = t.uv0 * glm::vec2(imageW - 1, imageH - 1);
            glm::vec2  uv1 = t.uv1 * glm::vec2(imageW - 1, imageH - 1);
            glm::vec2  uv2 = t.uv2 * glm::vec2(imageW - 1, imageH - 1);

            int minx = static_cast<int>(std::floor(std::min({uv0.x, uv1.x, uv2.x})));
            int miny = static_cast<int>(std::floor(std::min({uv0.y, uv1.y, uv2.y})));
            int maxx = static_cast<int>(std::ceil(std::max({uv0.x, uv1.x, uv2.x})));
            int maxy = static_cast<int>(std::ceil(std::max({uv0.y, uv1.y, uv2.y})));

            minx = std::clamp(minx, 0, imageW - 1);
            miny = std::clamp(miny, 0, imageH - 1);
            maxx = std::clamp(maxx, 0, imageW - 1);
            maxy = std::clamp(maxy, 0, imageH - 1);

            for (int y = miny; y <= maxy; ++y)
              for (int x = minx; x <= maxx; ++x)
              {
                glm::vec2 v0  = uv1 - uv0;
                glm::vec2 v1  = uv2 - uv0;
                glm::vec2 v2  = glm::vec2(static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f) - uv0;
                float     den = v0.x * v1.y - v0.y * v1.x;
                if (fabs(den) < 1e-12f) continue;
                float invDen = 1.0f / den;
                float a      = (v2.x * v1.y - v2.y * v1.x) * invDen;
                float b      = (v0.x * v2.y - v0.y * v2.x) * invDen;
                float c      = 1.0f - a - b;
                if (a >= -1e-6f && b >= -1e-6f && c >= -1e-6f) meshTexelTris[static_cast<size_t>(y) * imageW + static_cast<size_t>(x)].push_back(static_cast<int>(ti));
              }
          }

          // Prepare HDR buffer for chunk
          std::vector<float> meshHdr(imageW * imageH * 3, 0.0f);

          // Build chunk BVH
          BVH meshBvh;
          meshBvh.build(chunkTris, opts_.bvhPaddingFraction * std::max(1.0f, std::max(imageW, imageH) / 100.0f));

          // Flatten per-chunk triangle list into triIndexPerTexel and ranges
          std::vector<int> triIndexPerTexel;
          triIndexPerTexel.reserve(imageW * imageH);
          std::vector<glm::ivec2> texelRanges;
          texelRanges.resize(static_cast<size_t>(imageW) * static_cast<size_t>(imageH));
          for (size_t i = 0; i < meshTexelTris.size(); ++i)
          {
            texelRanges[i].x = static_cast<int>(triIndexPerTexel.size());
            texelRanges[i].y = static_cast<int>(meshTexelTris[i].size());
            for (auto tid : meshTexelTris[i])
              triIndexPerTexel.push_back(tid);
          }

          // Prepare triangle GPU structs
          struct GPUTri
          {
            glm::vec4 p0, p1, p2;
            glm::vec4 n0, n1, n2;
            glm::vec4 uv0_uv1;
            glm::vec4 uv2_pad;
          };
          std::vector<GPUTri> gpuTris;
          gpuTris.reserve(chunkTris.size());
          for (const auto& t : chunkTris)
          {
            GPUTri gt{};
            gt.p0      = glm::vec4(t.p0, 0.0f);
            gt.p1      = glm::vec4(t.p1, 0.0f);
            gt.p2      = glm::vec4(t.p2, 0.0f);
            gt.n0      = glm::vec4(t.n0, 0.0f);
            gt.n1      = glm::vec4(t.n1, 0.0f);
            gt.n2      = glm::vec4(t.n2, 0.0f);
            gt.uv0_uv1 = glm::vec4(t.uv0, t.uv1);
            gt.uv2_pad = glm::vec4(t.uv2.x, t.uv2.y, 0.0f, 0.0f);
            gpuTris.push_back(gt);
          }

          // Flatten BVH nodes
          const auto& mNodes = meshBvh.getNodes();
          struct GPUNode
          {
            glm::vec4  minv;
            glm::vec4  maxv;
            glm::ivec4 meta;
          };
          std::vector<GPUNode> gpuNodes;
          gpuNodes.reserve(mNodes.size());
          for (const auto& n : mNodes)
          {
            GPUNode gn;
            gn.minv = glm::vec4(n.bounds.min, 0.0f);
            gn.maxv = glm::vec4(n.bounds.max, 0.0f);
            gn.meta = glm::ivec4(n.left, n.right, n.start, n.count);
            gpuNodes.push_back(gn);
          }

          const auto& triIndicesGlobal = meshBvh.getTriIndices();

          // GPU path: flatten buffers and dispatch compute
          if (opts_.gpu)
          {
            uint32_t       texelCount = static_cast<uint32_t>(imageW) * static_cast<uint32_t>(imageH);
            engine::Buffer triBuffer{device_,
                                     sizeof(GPUTri),
                                     static_cast<uint32_t>(std::max<size_t>(gpuTris.size(), 1)),
                                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
            triBuffer.map();
            if (!gpuTris.empty())
              triBuffer.writeToBuffer(gpuTris.data(), sizeof(GPUTri) * gpuTris.size());
            else
            {
              GPUTri z{};
              triBuffer.writeToBuffer(&z, sizeof(GPUTri));
            }
            triBuffer.flush();
            triBuffer.unmap();

            engine::Buffer texelRangeBuffer{device_,
                                            sizeof(glm::ivec2),
                                            static_cast<uint32_t>(std::max<size_t>(texelRanges.size(), 1)),
                                            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
            texelRangeBuffer.map();
            if (!texelRanges.empty())
              texelRangeBuffer.writeToBuffer(texelRanges.data(), sizeof(glm::ivec2) * texelRanges.size());
            else
            {
              glm::ivec2 zr{0, 0};
              texelRangeBuffer.writeToBuffer(&zr, sizeof(glm::ivec2));
            }
            texelRangeBuffer.flush();
            texelRangeBuffer.unmap();

            engine::Buffer triPerTexelBuffer{device_,
                                             sizeof(int),
                                             static_cast<uint32_t>(std::max<size_t>(triIndexPerTexel.size(), 1)),
                                             VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
            triPerTexelBuffer.map();
            if (!triIndexPerTexel.empty())
              triPerTexelBuffer.writeToBuffer(triIndexPerTexel.data(), sizeof(int) * triIndexPerTexel.size());
            else
            {
              int z = -1;
              triPerTexelBuffer.writeToBuffer(&z, sizeof(int));
            }
            triPerTexelBuffer.flush();
            triPerTexelBuffer.unmap();

            engine::Buffer nodeBuffer{device_,
                                      sizeof(GPUNode),
                                      static_cast<uint32_t>(std::max<size_t>(gpuNodes.size(), 1)),
                                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
            nodeBuffer.map();
            if (!gpuNodes.empty())
              nodeBuffer.writeToBuffer(gpuNodes.data(), sizeof(GPUNode) * gpuNodes.size());
            else
            {
              GPUNode zn{};
              nodeBuffer.writeToBuffer(&zn, sizeof(GPUNode));
            }
            nodeBuffer.flush();
            nodeBuffer.unmap();

            engine::Buffer triGlobalBuffer{device_,
                                           sizeof(int),
                                           static_cast<uint32_t>(std::max<size_t>(triIndicesGlobal.size(), 1)),
                                           VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
            triGlobalBuffer.map();
            if (!triIndicesGlobal.empty())
              triGlobalBuffer.writeToBuffer(triIndicesGlobal.data(), sizeof(int) * triIndicesGlobal.size());
            else
            {
              int z = -1;
              triGlobalBuffer.writeToBuffer(&z, sizeof(int));
            }
            triGlobalBuffer.flush();
            triGlobalBuffer.unmap();

            engine::Buffer countsBuffer{device_, sizeof(uint32_t), texelCount, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
            countsBuffer.map();
            std::vector<uint32_t> zeros(texelCount, 0u);
            countsBuffer.writeToBuffer(zeros.data(), texelCount * sizeof(uint32_t));
            countsBuffer.flush();
            countsBuffer.unmap();

            // Create descriptor set layout and compute pipeline
            auto descriptorSetLayout = engine::DescriptorSetLayout::Builder(device_)
                                               .addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                                               .addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                                               .addBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                                               .addBinding(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                                               .addBinding(4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                                               .addBinding(5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                                               .build();

            std::string const shaderPath = std::string(SHADER_PATH) + "/baker_raycast.comp.spv";
            std::ifstream     file(shaderPath, std::ios::ate | std::ios::binary);
            if (!file.is_open())
            {
              std::cerr << "[ModelLightBaker] Failed to open compute shader: " << shaderPath << "\n";
              return;
            }
            auto              fileSize = static_cast<size_t>(file.tellg());
            std::vector<char> shaderCode(fileSize);
            file.seekg(0);
            file.read(shaderCode.data(), fileSize);

            // Create and use raw Vulkan compute pipeline (no engine wrappers here)
            VkShaderModule computeShaderModule;
            {
              VkShaderModuleCreateInfo createInfo{};
              createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
              createInfo.codeSize = shaderCode.size();
              std::vector<uint32_t> codeAligned((shaderCode.size() + 3) / 4);
              std::memcpy(codeAligned.data(), shaderCode.data(), shaderCode.size());
              createInfo.pCode = codeAligned.data();
              if (vkCreateShaderModule(device_.device(), &createInfo, nullptr, &computeShaderModule) != VK_SUCCESS)
              {
                std::cerr << "[ModelLightBaker] Failed to create shader module\n";
                return;
              }
            }

            VkPipelineShaderStageCreateInfo const shaderStageInfo{
                    .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                    .stage  = VK_SHADER_STAGE_COMPUTE_BIT,
                    .module = computeShaderModule,
                    .pName  = "main",
            };

            struct Push
            {
              int   imageWidth;
              int   imageHeight;
              int   samples;
              float eps;
              float sunDir[3];
              float sunIntensity;
            } pushConst{};
            pushConst.imageWidth   = imageW;
            pushConst.imageHeight  = imageH;
            pushConst.samples      = opts_.samples;
            pushConst.eps          = std::pow(10.0f, static_cast<float>(opts_.sampleEpsilonExponent));
            pushConst.sunDir[0]    = opts_.sunDir.x;
            pushConst.sunDir[1]    = opts_.sunDir.y;
            pushConst.sunDir[2]    = opts_.sunDir.z;
            pushConst.sunIntensity = opts_.sunIntensity;

            VkPushConstantRange const pushConstantRange{.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT, .offset = 0, .size = sizeof(pushConst)};

            VkDescriptorSetLayout            layout = descriptorSetLayout->getDescriptorSetLayout();
            VkPipelineLayoutCreateInfo const pipelineLayoutInfo{
                    .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                    .setLayoutCount         = 1,
                    .pSetLayouts            = &layout,
                    .pushConstantRangeCount = 1,
                    .pPushConstantRanges    = &pushConstantRange,
            };

            VkPipelineLayout pipelineLayout;
            if (vkCreatePipelineLayout(device_.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
            {
              std::cerr << "[ModelLightBaker] Failed to create pipeline layout\n";
              vkDestroyShaderModule(device_.device(), computeShaderModule, nullptr);
              return;
            }

            VkComputePipelineCreateInfo const pipelineInfo{
                    .sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                    .stage  = shaderStageInfo,
                    .layout = pipelineLayout,
            };
            VkPipeline pipeline;
            if (vkCreateComputePipelines(device_.device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS)
            {
              std::cerr << "[ModelLightBaker] Failed to create compute pipeline\n";
              vkDestroyPipelineLayout(device_.device(), pipelineLayout, nullptr);
              vkDestroyShaderModule(device_.device(), computeShaderModule, nullptr);
              return;
            }

            auto descriptorPool =
                    engine::DescriptorPool::Builder(device_).setMaxSets(5).addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 10).setPoolFlags(VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT).build();

            VkDescriptorSet descriptorSet;
            if (!descriptorPool->allocateDescriptor(layout, descriptorSet))
            {
              std::cerr << "[ModelLightBaker] Failed to allocate descriptor set\n";
              vkDestroyPipeline(device_.device(), pipeline, nullptr);
              vkDestroyPipelineLayout(device_.device(), pipelineLayout, nullptr);
              vkDestroyShaderModule(device_.device(), computeShaderModule, nullptr);
              return;
            }

            VkDescriptorBufferInfo triInfo{.buffer = triBuffer.getBuffer(), .offset = 0, .range = VK_WHOLE_SIZE};
            VkDescriptorBufferInfo triPerTexInfo{.buffer = triPerTexelBuffer.getBuffer(), .offset = 0, .range = VK_WHOLE_SIZE};
            VkDescriptorBufferInfo texelRangeInfo{.buffer = texelRangeBuffer.getBuffer(), .offset = 0, .range = VK_WHOLE_SIZE};
            VkDescriptorBufferInfo nodeInfo{.buffer = nodeBuffer.getBuffer(), .offset = 0, .range = VK_WHOLE_SIZE};
            VkDescriptorBufferInfo countsInfo{.buffer = countsBuffer.getBuffer(), .offset = 0, .range = VK_WHOLE_SIZE};
            VkDescriptorBufferInfo triGlobalInfo{.buffer = triGlobalBuffer.getBuffer(), .offset = 0, .range = VK_WHOLE_SIZE};

            engine::DescriptorWriter(*descriptorSetLayout, *descriptorPool)
                    .writeBuffer(0, &triInfo)
                    .writeBuffer(1, &triPerTexInfo)
                    .writeBuffer(2, &texelRangeInfo)
                    .writeBuffer(3, &nodeInfo)
                    .writeBuffer(4, &countsInfo)
                    .writeBuffer(5, &triGlobalInfo)
                    .overwrite(descriptorSet);

            // Dispatch compute shader in grid covering image
            VkCommandBuffer cmd = device_.beginSingleTimeCommands();
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
            vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConst), &pushConst);

            uint32_t groupX        = (imageW + 15) / 16;
            uint32_t groupY        = (imageH + 15) / 16;
            auto     dispatchStart = std::chrono::steady_clock::now();
            vkCmdDispatch(cmd, groupX, groupY, 1);
            device_.endSingleTimeCommands(cmd);

            vkDeviceWaitIdle(device_.device());
            auto                          dispatchEnd      = std::chrono::steady_clock::now();
            std::chrono::duration<double> dispatchDuration = dispatchEnd - dispatchStart;
            std::cout << "[ModelLightBaker] GPU dispatch time (mesh " << sm << " chunk " << chunkX << "," << chunkZ << "): " << dispatchDuration.count() << " s\n";

            // Read back counts via mapped memory and convert to HDR values
            countsBuffer.map();
            uint32_t* data = static_cast<uint32_t*>(countsBuffer.getMappedMemory());
            for (int y = 0; y < imageH; ++y)
            {
              for (int x = 0; x < imageW; ++x)
              {
                size_t   idx      = static_cast<size_t>(y) * static_cast<size_t>(imageW) + static_cast<size_t>(x);
                uint32_t c        = data[idx];
                float    vis      = (c > 0) ? (static_cast<float>(c) / static_cast<float>(opts_.samples)) : 0.0f;
                float    finalVal = ambient + opts_.sunIntensity * vis;
                size_t   pIdx     = (static_cast<size_t>(y) * imageW + static_cast<size_t>(x)) * 3;
                meshHdr[pIdx + 0] = finalVal;
                meshHdr[pIdx + 1] = finalVal;
                meshHdr[pIdx + 2] = finalVal;
              }
            }
            countsBuffer.unmap();

            float eps  = std::pow(10.0f, static_cast<float>(opts_.sampleEpsilonExponent));
            float tmax = (sqrtf(meshExtentX * meshExtentX + meshExtentZ * meshExtentZ) + 1.0f) * 10.0f; // over-approx

            // Save chunk EXR
            std::filesystem::path modelPath{model_.getFilePath()};
            std::string           baseName = modelPath.stem().string();
            std::string           meshName = baseName + "_mesh" + std::to_string(sm) + "_chunk" + std::to_string(chunkX) + "_" + std::to_string(chunkZ) + "_lightmap.exr";
            std::filesystem::path exrPath  = std::filesystem::path(outDir_) / meshName;
            const char*           err      = nullptr;
            int                   ret      = SaveEXR(meshHdr.data(), imageW, imageH, 3, 0, exrPath.string().c_str(), &err);
            if (ret != TINYEXR_SUCCESS)
            {
              std::cerr << "[ModelLightBaker] Failed to write chunk EXR: " << exrPath.string() << "\n";
            }
            else
            {
              std::cout << "[ModelLightBaker] Wrote chunk EXR: " << exrPath.string() << "\n";
              meshLightmaps_.push_back({static_cast<int>(sm), chunkX, chunkZ, exrPath.filename().string(), imageW, imageH});

              if (opts_.preview)
              {
                auto prevHdr = hdrPixels_;
                auto prevW   = imageWidth_;
                auto prevH   = imageHeight_;
                hdrPixels_.swap(meshHdr);
                imageWidth_  = imageW;
                imageHeight_ = imageH;
                savePreviewPNG(outDir_, baseName + "_mesh" + std::to_string(sm) + "_chunk" + std::to_string(chunkX) + "_" + std::to_string(chunkZ));
                hdrPixels_.swap(meshHdr);
                imageWidth_  = prevW;
                imageHeight_ = prevH;
              }
            }
          }
          // CPU fallback removed — GPU-only path (chunk EXRs are written in the GPU branch above)
        };

        // If mesh needs chunking, split and bake each chunk
        float tileSize = opts_.meshChunkSize;
        if (maxExtentXZ > tileSize)
        {
          uint32_t nx    = std::max<uint32_t>(1u, static_cast<uint32_t>(std::ceil(meshExtentX / tileSize)));
          uint32_t nz    = std::max<uint32_t>(1u, static_cast<uint32_t>(std::ceil(meshExtentZ / tileSize)));
          float    tileW = meshExtentX / static_cast<float>(nx);
          float    tileH = meshExtentZ / static_cast<float>(nz);

          for (uint32_t iz = 0; iz < nz; ++iz)
          {
            for (uint32_t ix = 0; ix < nx; ++ix)
            {
              glm::vec3 tileMin = glm::vec3(mBounds.min.x + ix * tileW, mBounds.min.y, mBounds.min.z + iz * tileH);
              glm::vec3 tileMax = glm::vec3(mBounds.min.x + (ix + 1) * tileW, mBounds.max.y, mBounds.min.z + (iz + 1) * tileH);

              // collect triangles whose AABB intersects this tile
              std::vector<Tri> chunkTris;
              chunkTris.reserve(256);
              for (size_t ti = 0; ti < meshTris.size(); ++ti)
              {
                const Tri& t    = meshTris[ti];
                glm::vec3  tmin = glm::min(glm::min(t.p0, t.p1), t.p2);
                glm::vec3  tmax = glm::max(glm::max(t.p0, t.p1), t.p2);
                if (tmax.x < tileMin.x || tmin.x > tileMax.x) continue;
                if (tmax.z < tileMin.z || tmin.z > tileMax.z) continue;
                chunkTris.push_back(t);
              }

              if (chunkTris.empty()) continue;

              // choose chunk resolution
              float chunkMaxSide = std::max(tileW, tileH);
              int   chunkRes     = chooseResolutionForChunk(chunkMaxSide);

              // bake this chunk
              bakeChunk(chunkTris, chunkRes, chunkRes, static_cast<int>(ix), static_cast<int>(iz));
            }
          }

          // done with mesh (chunked)
        }

        // Not chunked: reuse the chunk-baker for the whole mesh (pass chunk coords -1)
        bakeChunk(meshTris, resolution, resolution, -1, -1);

        // done with mesh (handled by bakeChunk)
      };

      return;
    }

    // Prepare per-texel triangle lists by rasterizing triangle UVs into texture space
    imageWidth_  = opts_.resolution;
    imageHeight_ = opts_.resolution;
    hdrPixels_.assign(imageWidth_ * imageHeight_ * 3, 0.0f);

    std::vector<std::vector<int>> texelTris(static_cast<size_t>(imageWidth_) * static_cast<size_t>(imageHeight_));

    for (size_t ti = 0; ti < tris.size(); ++ti)
    {
      const auto& t = tris[ti];
      // convert UVs to texel coordinates
      glm::vec2 uv0 = t.uv0 * glm::vec2(imageWidth_ - 1, imageHeight_ - 1);
      glm::vec2 uv1 = t.uv1 * glm::vec2(imageWidth_ - 1, imageHeight_ - 1);
      glm::vec2 uv2 = t.uv2 * glm::vec2(imageWidth_ - 1, imageHeight_ - 1);

      float minx = std::floor(std::min(std::min(uv0.x, uv1.x), uv2.x));
      float miny = std::floor(std::min(std::min(uv0.y, uv1.y), uv2.y));
      float maxx = std::ceil(std::max(std::max(uv0.x, uv1.x), uv2.x));
      float maxy = std::ceil(std::max(std::max(uv0.y, uv1.y), uv2.y));

      int ix0 = static_cast<int>(glm::clamp(minx, 0.0f, static_cast<float>(imageWidth_ - 1)));
      int iy0 = static_cast<int>(glm::clamp(miny, 0.0f, static_cast<float>(imageHeight_ - 1)));
      int ix1 = static_cast<int>(glm::clamp(maxx, 0.0f, static_cast<float>(imageWidth_ - 1)));
      int iy1 = static_cast<int>(glm::clamp(maxy, 0.0f, static_cast<float>(imageHeight_ - 1)));

      for (int y = iy0; y <= iy1; ++y)
      {
        for (int x = ix0; x <= ix1; ++x)
        {
          // sample point at pixel center
          glm::vec2 p(static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f);

          // barycentric in UV space
          glm::vec2 v0 = uv1 - uv0;
          glm::vec2 v1 = uv2 - uv0;
          glm::vec2 v2 = p - uv0;

          float den = (v0.x * v1.y) - (v0.y * v1.x);
          if (std::fabs(den) < 1e-10f) continue; // degenerate
          float invDen = 1.0f / den;
          float a      = (v2.x * v1.y - v2.y * v1.x) * invDen;
          float b      = (v0.x * v2.y - v0.y * v2.x) * invDen;
          float c      = 1.0f - a - b;
          if (a >= -1e-6f && b >= -1e-6f && c >= -1e-6f)
          {
            size_t idx = (static_cast<size_t>(y) * imageWidth_) + static_cast<size_t>(x);
            texelTris[idx].push_back(static_cast<int>(ti));
          }
        }
      }
    }

    // CPU per-texel sampling removed; using GPU compute only
    {
      // GPU compute path (dispatch compute shader to raycast per-texel samples)
      std::cout << "[ModelLightBaker] Running GPU bake (dispatch compute shader)...\n";

      // Flatten per-texel triangle lists into a single index buffer and ranges
      std::vector<int> triIndexPerTexel;
      triIndexPerTexel.reserve(imageWidth_ * imageHeight_);
      std::vector<glm::ivec2> texelRanges;
      texelRanges.resize(static_cast<size_t>(imageWidth_) * static_cast<size_t>(imageHeight_));
      for (size_t i = 0; i < texelTris.size(); ++i)
      {
        texelRanges[i].x = static_cast<int>(triIndexPerTexel.size());
        texelRanges[i].y = static_cast<int>(texelTris[i].size());
        for (auto tid : texelTris[i])
        {
          triIndexPerTexel.push_back(tid);
        }
      }

      // Prepare triangle GPU structs
      struct GPUTri
      {
        glm::vec4 p0, p1, p2;
        glm::vec4 n0, n1, n2;
        glm::vec4 uv0_uv1;
        glm::vec4 uv2_pad;
      };
      std::vector<GPUTri> gpuTris;
      gpuTris.reserve(tris.size());
      for (const auto& t : tris)
      {
        GPUTri gt{};
        gt.p0      = glm::vec4(t.p0, 0.0f);
        gt.p1      = glm::vec4(t.p1, 0.0f);
        gt.p2      = glm::vec4(t.p2, 0.0f);
        gt.n0      = glm::vec4(t.n0, 0.0f);
        gt.n1      = glm::vec4(t.n1, 0.0f);
        gt.n2      = glm::vec4(t.n2, 0.0f);
        gt.uv0_uv1 = glm::vec4(t.uv0, t.uv1);
        // glm::vec4 doesn't accept (vec2, float) on this GLM version — expand explicitly
        gt.uv2_pad = glm::vec4(t.uv2.x, t.uv2.y, 0.0f, 0.0f);
        gpuTris.push_back(gt);
      }

      // Flatten BVH nodes
      const auto& nodes = bvh.getNodes();
      struct GPUNode
      {
        glm::vec4  minv;
        glm::vec4  maxv;
        glm::ivec4 meta;
      };
      std::vector<GPUNode> gpuNodes;
      gpuNodes.reserve(nodes.size());
      for (const auto& n : nodes)
      {
        GPUNode gn;
        gn.minv = glm::vec4(n.bounds.min, 0.0f);
        gn.maxv = glm::vec4(n.bounds.max, 0.0f);
        gn.meta = glm::ivec4(n.left, n.right, n.start, n.count);
        gpuNodes.push_back(gn);
      }

      // global tri index list from BVH
      const auto& triIndicesGlobal = bvh.getTriIndices();

      // Create GPU buffers (host-visible for simplicity)
      // Debug: print GPU buffer counts to ensure none are zero (helps find validation issues)
      uint32_t localTexelCount = static_cast<uint32_t>(imageWidth_) * static_cast<uint32_t>(imageHeight_);
      std::cout << "[ModelLightBaker] GPU tri count=" << gpuTris.size() << " texelRanges=" << texelRanges.size() << " triIndexPerTexel=" << triIndexPerTexel.size() << " gpuNodes=" << gpuNodes.size()
                << " triIndicesGlobal=" << triIndicesGlobal.size() << " texelCount=" << localTexelCount << "\n";

      // Create triangle buffer (create a 1-element dummy buffer if empty to avoid zero-sized Vulkan buffers)
      engine::Buffer triBuffer{device_,
                               sizeof(GPUTri),
                               static_cast<uint32_t>(std::max<size_t>(gpuTris.size(), 1)),
                               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
      triBuffer.map();
      if (!gpuTris.empty())
      {
        triBuffer.writeToBuffer(gpuTris.data(), sizeof(GPUTri) * gpuTris.size());
      }
      else
      {
        GPUTri zeroTri{};
        triBuffer.writeToBuffer(&zeroTri, sizeof(GPUTri));
      }
      triBuffer.flush();
      triBuffer.unmap();

      // texel ranges (should always be >0)
      engine::Buffer texelRangeBuffer{device_,
                                      sizeof(glm::ivec2),
                                      static_cast<uint32_t>(std::max<size_t>(texelRanges.size(), 1)),
                                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
      texelRangeBuffer.map();
      if (!texelRanges.empty())
      {
        texelRangeBuffer.writeToBuffer(texelRanges.data(), sizeof(glm::ivec2) * texelRanges.size());
      }
      else
      {
        glm::ivec2 zr{0, 0};
        texelRangeBuffer.writeToBuffer(&zr, sizeof(glm::ivec2));
      }
      texelRangeBuffer.flush();
      texelRangeBuffer.unmap();

      // tri index per texel (may be empty)
      engine::Buffer triPerTexelBuffer{device_,
                                       sizeof(int),
                                       static_cast<uint32_t>(std::max<size_t>(triIndexPerTexel.size(), 1)),
                                       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
      triPerTexelBuffer.map();
      if (!triIndexPerTexel.empty())
      {
        triPerTexelBuffer.writeToBuffer(triIndexPerTexel.data(), sizeof(int) * triIndexPerTexel.size());
      }
      else
      {
        int z = -1;
        triPerTexelBuffer.writeToBuffer(&z, sizeof(int));
      }
      triPerTexelBuffer.flush();
      triPerTexelBuffer.unmap();

      // BVH node buffer
      engine::Buffer nodeBuffer{device_,
                                sizeof(GPUNode),
                                static_cast<uint32_t>(std::max<size_t>(gpuNodes.size(), 1)),
                                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
      nodeBuffer.map();
      if (!gpuNodes.empty())
      {
        nodeBuffer.writeToBuffer(gpuNodes.data(), sizeof(GPUNode) * gpuNodes.size());
      }
      else
      {
        GPUNode zn{};
        nodeBuffer.writeToBuffer(&zn, sizeof(GPUNode));
      }
      nodeBuffer.flush();
      nodeBuffer.unmap();

      // global tri index list from BVH
      engine::Buffer triGlobalBuffer{device_,
                                     sizeof(int),
                                     static_cast<uint32_t>(std::max<size_t>(triIndicesGlobal.size(), 1)),
                                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
      triGlobalBuffer.map();
      if (!triIndicesGlobal.empty())
      {
        triGlobalBuffer.writeToBuffer(triIndicesGlobal.data(), sizeof(int) * triIndicesGlobal.size());
      }
      else
      {
        int z = -1;
        triGlobalBuffer.writeToBuffer(&z, sizeof(int));
      }
      triGlobalBuffer.flush();
      triGlobalBuffer.unmap();

      uint32_t       texelCount = static_cast<uint32_t>(imageWidth_) * static_cast<uint32_t>(imageHeight_);
      engine::Buffer countsBuffer{device_, sizeof(uint32_t), texelCount, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
      // zero it
      countsBuffer.map();
      std::vector<uint32_t> zeros(texelCount, 0u);
      countsBuffer.writeToBuffer(zeros.data(), texelCount * sizeof(uint32_t));
      countsBuffer.flush();
      countsBuffer.unmap();

      // Create descriptor set layout
      auto descriptorSetLayout = engine::DescriptorSetLayout::Builder(device_)
                                         .addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                                         .addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                                         .addBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                                         .addBinding(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                                         .addBinding(4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                                         .addBinding(5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                                         .build();

      // Load compiled compute shader (SPV)
      std::string const shaderPath = std::string(SHADER_PATH) + "/baker_raycast.comp.spv";
      std::ifstream     file(shaderPath, std::ios::ate | std::ios::binary);
      if (!file.is_open())
      {
        std::cerr << "[ModelLightBaker] Failed to open compute shader: " << shaderPath << "\n";
        return;
      }
      auto              fileSize = static_cast<size_t>(file.tellg());
      std::vector<char> shaderCode(fileSize);
      file.seekg(0);
      file.read(shaderCode.data(), fileSize);
      file.close();

      // Create shader module
      VkShaderModule computeShaderModule;
      {
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = shaderCode.size();
        std::vector<uint32_t> codeAligned((shaderCode.size() + 3) / 4);
        std::memcpy(codeAligned.data(), shaderCode.data(), shaderCode.size());
        createInfo.pCode = codeAligned.data();
        if (vkCreateShaderModule(device_.device(), &createInfo, nullptr, &computeShaderModule) != VK_SUCCESS)
        {
          std::cerr << "[ModelLightBaker] Failed to create shader module\n";
          return;
        }
      }

      VkPipelineShaderStageCreateInfo const shaderStageInfo{
              .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
              .stage  = VK_SHADER_STAGE_COMPUTE_BIT,
              .module = computeShaderModule,
              .pName  = "main",
      };

      // pipeline layout with push constants
      struct Push
      {
        int   imageWidth;
        int   imageHeight;
        int   samples;
        float eps;
        float sunDir[3];
        float sunIntensity;
      } pushConst{};
      pushConst.imageWidth   = imageWidth_;
      pushConst.imageHeight  = imageHeight_;
      pushConst.samples      = opts_.samples;
      pushConst.eps          = std::pow(10.0f, static_cast<float>(opts_.sampleEpsilonExponent));
      pushConst.sunDir[0]    = opts_.sunDir.x;
      pushConst.sunDir[1]    = opts_.sunDir.y;
      pushConst.sunDir[2]    = opts_.sunDir.z;
      pushConst.sunIntensity = opts_.sunIntensity;

      VkPushConstantRange const pushConstantRange{.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT, .offset = 0, .size = sizeof(pushConst)};

      VkDescriptorSetLayout            layout = descriptorSetLayout->getDescriptorSetLayout();
      VkPipelineLayoutCreateInfo const pipelineLayoutInfo{
              .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
              .setLayoutCount         = 1,
              .pSetLayouts            = &layout,
              .pushConstantRangeCount = 1,
              .pPushConstantRanges    = &pushConstantRange,
      };

      VkPipelineLayout pipelineLayout;
      if (vkCreatePipelineLayout(device_.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
      {
        std::cerr << "[ModelLightBaker] Failed to create pipeline layout\n";
        vkDestroyShaderModule(device_.device(), computeShaderModule, nullptr);
        return;
      }

      VkComputePipelineCreateInfo const pipelineInfo{
              .sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
              .stage  = shaderStageInfo,
              .layout = pipelineLayout,
      };
      VkPipeline pipeline;
      if (vkCreateComputePipelines(device_.device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS)
      {
        std::cerr << "[ModelLightBaker] Failed to create compute pipeline\n";
        vkDestroyPipelineLayout(device_.device(), pipelineLayout, nullptr);
        vkDestroyShaderModule(device_.device(), computeShaderModule, nullptr);
        return;
      }

      // descriptor pool
      auto descriptorPool =
              engine::DescriptorPool::Builder(device_).setMaxSets(5).addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 10).setPoolFlags(VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT).build();

      VkDescriptorSet descriptorSet;
      if (!descriptorPool->allocateDescriptor(layout, descriptorSet))
      {
        std::cerr << "[ModelLightBaker] Failed to allocate descriptor set\n";
        vkDestroyPipeline(device_.device(), pipeline, nullptr);
        vkDestroyPipelineLayout(device_.device(), pipelineLayout, nullptr);
        vkDestroyShaderModule(device_.device(), computeShaderModule, nullptr);
        return;
      }

      VkDescriptorBufferInfo triInfo{.buffer = triBuffer.getBuffer(), .offset = 0, .range = VK_WHOLE_SIZE};
      VkDescriptorBufferInfo triPerTexInfo{.buffer = triPerTexelBuffer.getBuffer(), .offset = 0, .range = VK_WHOLE_SIZE};
      VkDescriptorBufferInfo texelRangeInfo{.buffer = texelRangeBuffer.getBuffer(), .offset = 0, .range = VK_WHOLE_SIZE};
      VkDescriptorBufferInfo nodeInfo{.buffer = nodeBuffer.getBuffer(), .offset = 0, .range = VK_WHOLE_SIZE};
      VkDescriptorBufferInfo countsInfo{.buffer = countsBuffer.getBuffer(), .offset = 0, .range = VK_WHOLE_SIZE};
      VkDescriptorBufferInfo triGlobalInfo{.buffer = triGlobalBuffer.getBuffer(), .offset = 0, .range = VK_WHOLE_SIZE};

      engine::DescriptorWriter(*descriptorSetLayout, *descriptorPool)
              .writeBuffer(0, &triInfo)
              .writeBuffer(1, &triPerTexInfo)
              .writeBuffer(2, &texelRangeInfo)
              .writeBuffer(3, &nodeInfo)
              .writeBuffer(4, &countsInfo)
              .writeBuffer(5, &triGlobalInfo)
              .overwrite(descriptorSet);

      // Dispatch compute shader in grid covering image
      VkCommandBuffer cmd = device_.beginSingleTimeCommands();
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
      vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConst), &pushConst);

      uint32_t groupX = (imageWidth_ + 15) / 16;
      uint32_t groupY = (imageHeight_ + 15) / 16;
      // timing start for GPU dispatch
      auto dispatchStart = std::chrono::steady_clock::now();
      vkCmdDispatch(cmd, groupX, groupY, 1);
      device_.endSingleTimeCommands(cmd);

      vkDeviceWaitIdle(device_.device());
      auto                          dispatchEnd      = std::chrono::steady_clock::now();
      std::chrono::duration<double> dispatchDuration = dispatchEnd - dispatchStart;
      std::cout << "[ModelLightBaker] GPU dispatch time: " << dispatchDuration.count() << " s\n";

      // Read back counts
      countsBuffer.map();
      uint32_t* data = static_cast<uint32_t*>(countsBuffer.getMappedMemory());
      for (uint32_t y = 0; y < static_cast<uint32_t>(imageHeight_); ++y)
      {
        for (uint32_t x = 0; x < static_cast<uint32_t>(imageWidth_); ++x)
        {
          size_t   idx         = (static_cast<size_t>(y) * imageWidth_) + x;
          uint32_t hits        = data[idx];
          float    visibility  = static_cast<float>(hits) / static_cast<float>(opts_.samples);
          float    sunTerm     = opts_.sunIntensity * visibility;
          float    finalVal    = ambient + sunTerm;
          size_t   pIdx        = idx * 3;
          hdrPixels_[pIdx + 0] = finalVal;
          hdrPixels_[pIdx + 1] = finalVal;
          hdrPixels_[pIdx + 2] = finalVal;
        }
      }
      countsBuffer.unmap();

      // cleanup
      vkDestroyPipeline(device_.device(), pipeline, nullptr);
      vkDestroyPipelineLayout(device_.device(), pipelineLayout, nullptr);
      vkDestroyShaderModule(device_.device(), computeShaderModule, nullptr);

      std::cout << "[ModelLightBaker] Completed GPU bake (" << imageWidth_ << "x" << imageHeight_ << ")\n";
    }
  }

  bool ModelLightBaker::saveToDisk()
  {
    // If running in MESH mode, write a manifest that lists per-mesh lightmaps
    if (opts_.mode == Options::Mode::MESH)
    {
      if (meshLightmaps_.empty())
      {
        std::cerr << "[ModelLightBaker] No per-mesh lightmaps produced\n";
        return false;
      }

      std::filesystem::path modelPath{model_.getFilePath()};
      std::string           baseName     = modelPath.stem().string();
      std::string           manifestName = baseName + std::string("_mesh_lightmaps.json");
      std::filesystem::path manifest     = std::filesystem::path(outDir_) / manifestName;
      std::ofstream         o(manifest);
      if (!o)
      {
        std::cerr << "[ModelLightBaker] Unable to write manifest: " << manifest.string() << "\n";
        return false;
      }

      o << "{\n";
      o << R"(  "model": ")" << model_.getFilePath() << "\",\n";
      o << "  \"format\": \"exr\",\n";
      o << "  \"meshes\": [\n";
      for (size_t i = 0; i < meshLightmaps_.size(); ++i)
      {
        const auto& m = meshLightmaps_[i];
        o << "    { \"mesh\": " << m.meshIndex << R"(, "file": ")" << m.file << R"(", "resolution": [)" << m.width << ", " << m.height << "] }";
        if (i + 1 < meshLightmaps_.size())
        {
          o << ",\n";
        }
        else
        {
          o << "\n";
        }
      }
      o << "  ]\n";
      o << "}\n";
      o.close();

      std::cout << "[ModelLightBaker] Wrote per-mesh manifest: " << manifest.string() << "\n";
      return true;
    }

    if (hdrPixels_.empty() || imageWidth_ == 0 || imageHeight_ == 0)
    {
      std::cerr << "[ModelLightBaker] No bake image available to save\n";
      return false;
    }

    // Build output filename based on model filename
    std::filesystem::path modelPath{model_.getFilePath()};
    std::string           baseName = modelPath.stem().string();
    std::filesystem::path exrPath  = std::filesystem::path(outDir_) / (baseName + "_lightmap.exr");

    const char* err = nullptr;
    // SaveEXR expects data as float RGB(A) in row-major order.
    int ret = SaveEXR(hdrPixels_.data(), imageWidth_, imageHeight_, 3, 0, exrPath.string().c_str(), &err);
    if (ret != TINYEXR_SUCCESS)
    {
      std::cerr << "[ModelLightBaker] Failed to write EXR: ";
      if (err != nullptr)
      {
        std::cerr << err << '\n';
        FreeEXRErrorMessage(err);
      }
      else
      {
        std::cerr << "unknown error\n";
      }
      return false;
    }

    std::cout << "[ModelLightBaker] Wrote EXR: " << exrPath.string() << "\n";

    // If preview is enabled also write a preview PNG
    if (opts_.preview)
    {
      std::cout << "[ModelLightBaker] Preview enabled (max size=" << opts_.previewMaxSize << ")\n";
    }

    // Optionally pack the EXR to a VTEX container (either CLI flag or sentinel file can trigger)
    std::string outFileName = exrPath.filename().string();

    // Diagnostic: print whether sentinel file exists and current opts flag (helps tests confirm why packing did/didn't run)
    std::filesystem::path sentinelPath   = std::filesystem::path(outDir_) / "MODEL_LIGHT_BAKER_PACK_TO_VTEX";
    bool                  sentinelExists = std::filesystem::exists(sentinelPath);
    std::cerr << "[ModelLightBaker] Sentinel check: '" << sentinelPath.generic_string() << "' exists=" << (sentinelExists ? "yes" : "no") << " opts.packToVTEX=" << (opts_.packToVTEX ? "yes" : "no")
              << "\n";

    bool shouldPack = opts_.packToVTEX || sentinelExists;
    if (shouldPack)
    {
      std::string           vtexName = baseName + std::string("_lightmap.vtex");
      std::filesystem::path vtexPath = std::filesystem::path(outDir_) / vtexName;
      std::cout << "[ModelLightBaker] Packing EXR -> VTEX: " << vtexPath.string() << "\n";

      try
      {
        // Use the CPU EXR->VTEX helper (does not load into GPU). This helper returns
        // nullptr when caller requested only file output (loadIntoGpu == false). Treat
        // a non-exceptional return as success and validate the written file on disk.
        auto texPtr = engine::Texture::createFromEXR_CPUOnly(device_, exrPath.string(), vtexPath.string(), false);

        // Write an attempt sentinel to help test harnesses detect the pack path was entered
        std::filesystem::path attemptSentinel = vtexPath;
        attemptSentinel += ".pack_attempt";
        std::ofstream(attemptSentinel).put('1');

        // Confirm VTEX file exists and is non-empty
        bool vtexExists = std::filesystem::exists(vtexPath);
        uintmax_t vtexSize = 0;
        if (vtexExists)
          vtexSize = std::filesystem::file_size(vtexPath);

        if (!vtexExists || vtexSize == 0)
        {
          std::cerr << "[ModelLightBaker] VTEX write check failed (exists=" << (vtexExists ? "yes" : "no") << ", size=" << vtexSize << ") for: " << vtexPath.string() << "\n";
          return false;
        }

        // Success: if texPtr is non-null it means we also loaded into GPU; otherwise
        // the CPU-only write succeeded and we honor the VTEX output.
        outFileName = vtexName;
        std::cout << "[ModelLightBaker] Wrote VTEX: " << vtexPath.string() << " (size=" << vtexSize << ")\n";
      }
      catch (const std::exception& e)
      {
        std::cerr << "[ModelLightBaker] Failed to pack to VTEX: " << e.what() << "\n";
        return false;
      }
    }

    // Write a per-model manifest JSON next to the EXR/V TEX file
    std::string           manifestName = baseName + std::string("_lightmap.json");
    std::filesystem::path manifest     = std::filesystem::path(outDir_) / manifestName;
    std::ofstream         o(manifest);
    if (!o)
    {
      std::cerr << "[ModelLightBaker] Unable to write manifest: " << manifest.string() << "\n";
      return false;
    }

    // Minimal manifest describing this single bake (easier for the app to consume as one file per model)
    o << "{\n";
    o << "  \"model\": \"" << model_.getFilePath() << "\",\n";
    o << "  \"file\": \"" << outFileName << "\",\n";
    o << "  \"format\": \"" << (opts_.packToVTEX ? "vtex" : "exr") << "\",\n";
    o << "  \"resolution\": " << imageWidth_ << ",\n";
    o << "  \"samples\": " << opts_.samples << ",\n";
    o << "  \"sun_dir\": [" << opts_.sunDir.x << ", " << opts_.sunDir.y << ", " << opts_.sunDir.z << "],\n";
    o << "  \"sun_intensity\": " << opts_.sunIntensity << "\n";
    o << "}\n";
    o.close();

    std::cout << "[ModelLightBaker] Wrote manifest: " << manifest.string() << "\n";

    // Optionally write a small LDR preview for quick validation
    if (opts_.preview)
    {
      if (savePreviewPNG(outDir_, baseName))
      {
        std::cout << "[ModelLightBaker] Wrote preview PNG: " << (baseName + "_preview.png") << "\n";
      }
      else
      {
        std::cerr << "[ModelLightBaker] Failed to write preview PNG\n";
      }
    }

    return true;
  }

} // namespace ModelLightBaker
