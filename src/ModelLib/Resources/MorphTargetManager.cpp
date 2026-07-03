#include "ModelLib/Resources/MorphTargetManager.hpp"

#include <algorithm>
#include <iostream>
#include <memory>
#include <utility>
#include <vector>

#include "Engine/Core/ansi_colors.hpp"
#include "Engine/Graphics/Buffer.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/MorphTargetCompute.hpp"

#include "ModelLib/Resources/Model.hpp"
#include "vulkan/vulkan_core.h"

namespace engine {

    struct MorphDelta {
        glm::vec4 positionDelta;
        glm::vec4 normalDelta;
    };

    MorphTargetManager::MorphTargetManager(Device& device) : device_(device) {
        compute_ = std::make_unique<MorphTargetCompute>(device_);
    }

    void MorphTargetManager::initializeModel(const std::shared_ptr<Model>& model) {
        if (!model || !model->hasMorphTargets()) {
            return;
        }

        const Model* modelPtr = model.get();

        if (modelData_.contains(modelPtr)) {
            return;
        }

        ModelMorphData data{};
        createMorphBuffers(*model, data);
        modelData_[modelPtr] = std::move(data);

        std::cout << "[" << GREEN << "MorphTargetManager" << RESET << "] Initialized model with " << data.morphTargetCount << " morph targets, " << data.vertexCount << " vertices" << '\n';
    }

    void MorphTargetManager::createMorphBuffers(const Model& model, ModelMorphData& data) {
        const auto& morphTargetSets = model.getMorphTargetSets();

        if (morphTargetSets.empty()) {
            return;
        }

        const auto& morphSet = morphTargetSets[0];

        data.morphTargetCount = static_cast<uint32_t>(morphSet.targets.size());
        data.vertexCount      = morphSet.vertexCount;
        data.vertexOffset     = morphSet.vertexOffset;

        size_t const morphDeltaCount = data.morphTargetCount * data.vertexCount;
        size_t const weightsCount    = data.morphTargetCount;

        std::vector<MorphDelta> deltas;
        deltas.reserve(morphDeltaCount);

        for (const auto& target : morphSet.targets) {
            for (uint32_t vertexIndex = 0; vertexIndex < data.vertexCount; vertexIndex++) {
                MorphDelta delta{};

                uint32_t     posIdx       = vertexIndex;
                size_t const vertexIndexS = static_cast<size_t>(vertexIndex);
                if (!morphSet.positionIndices.empty() && vertexIndexS < morphSet.positionIndices.size()) {
                    posIdx = morphSet.positionIndices[vertexIndexS];
                }

                delta.positionDelta = glm::vec4(target.positionDeltas[posIdx], 0.0f);
                delta.normalDelta   = glm::vec4(target.normalDeltas[posIdx], 0.0f);
                deltas.push_back(delta);
            }

            if (&target == morphSet.targets.data()) {
                std::cout << "[MorphTargetManager] Position index mapping sample: ";
                for (size_t i = 0; i < std::min(static_cast<size_t>(6), morphSet.positionIndices.size()); i++) {
                    std::cout << i << "->" << morphSet.positionIndices[i] << " ";
                }
                std::cout << '\n';
            }
        }

        data.morphDeltaBuffer = std::make_unique<Buffer>(device_,
            sizeof(MorphDelta),
            static_cast<uint32_t>(deltas.size()),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        Buffer stagingBuffer{device_,
            sizeof(MorphDelta),
            static_cast<uint32_t>(deltas.size()),
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};

        stagingBuffer.map();
        stagingBuffer.writeToBuffer(deltas.data(), sizeof(MorphDelta) * deltas.size());
        stagingBuffer.unmap();

        device_.memory().copyBufferImmediate(stagingBuffer.getBuffer(),
            data.morphDeltaBuffer->getBuffer(),
            sizeof(MorphDelta) * deltas.size(),
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_SHADER_READ_BIT);

        data.weightsBuffer = std::make_unique<Buffer>(device_,
            sizeof(float),
            static_cast<uint32_t>(weightsCount),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        data.weightsBuffer->map();

        std::vector<float> initialWeights(weightsCount, 0.0f);
        if (!morphSet.weights.empty()) {
            for (size_t i = 0; i < std::min(weightsCount, morphSet.weights.size()); i++) {
                initialWeights[i] = morphSet.weights[i];
            }
        }
        data.weightsBuffer->writeToBuffer(initialWeights.data(), sizeof(float) * weightsCount);

        data.blendedBuffer = std::make_unique<Buffer>(device_,
            sizeof(Model::Vertex),
            static_cast<uint32_t>(data.vertexCount),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    }

    void MorphTargetManager::updateAndBlend(VkCommandBuffer commandBuffer, const std::shared_ptr<Model>& model) {
        if (!model || !model->hasMorphTargets()) {
            return;
        }

        const Model* modelPtr = model.get();
        auto         it       = modelData_.find(modelPtr);

        if (it == modelData_.end()) {
            return;
        }

        auto& data = it->second;

        const auto&        nodes = model->getNodes();
        std::vector<float> currentWeights(data.morphTargetCount, 0.0f);

        for (const auto& node : nodes) {
            if (!node.morphWeights.empty()) {
                static int debugFrameCount = 0;
                if (debugFrameCount++ < 5) {
                    std::cout << "[MorphTargetManager] Frame weights: ";
                    for (float morphWeight : node.morphWeights) {
                        std::cout << morphWeight << " ";
                    }
                    std::cout << '\n';
                }
                for (size_t i = 0; i < std::min(currentWeights.size(), node.morphWeights.size()); i++) {
                    currentWeights[i] = node.morphWeights[i];
                }
                break;
            }
        }

        data.weightsBuffer->writeToBuffer(currentWeights.data(), sizeof(float) * currentWeights.size());

        static int frameCount = 0;
        if (frameCount++ < 3) {
            std::cout << "[MorphTargetManager] Weights: ";
            for (float currentWeight : currentWeights) {
                std::cout << currentWeight << " ";
            }
            std::cout << '\n';
        }

        MorphTargetCompute::PushConstants const pushConstants{
            .vertexOffset     = data.vertexOffset,
            .vertexCount      = static_cast<uint32_t>(data.vertexCount),
            .morphTargetCount = static_cast<uint32_t>(data.morphTargetCount),
            .deltaOffset      = 0,
        };

        static bool printedOnce = false;
        if (!printedOnce) {
            std::cout << "[MorphTargetManager] Compute dispatch: offset=" << pushConstants.vertexOffset << " count=" << pushConstants.vertexCount << " morphTargets=" << pushConstants.morphTargetCount
                      << '\n';
            printedOnce = true;
        }

        data.descriptorSet = compute_->blend(commandBuffer,
            data.descriptorSet,
            model->getVertexBuffer(),
            data.morphDeltaBuffer->getBuffer(),
            data.weightsBuffer->getBuffer(),
            data.blendedBuffer->getBuffer(),
            pushConstants);

        VkBufferMemoryBarrier const barrier{
            .sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .srcAccessMask       = VK_ACCESS_SHADER_WRITE_BIT,
            .dstAccessMask       = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .buffer              = data.blendedBuffer->getBuffer(),
            .offset              = 0,
            .size                = VK_WHOLE_SIZE,
        };

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 0, nullptr, 1, &barrier, 0, nullptr);
    }

    bool MorphTargetManager::isModelInitialized(const Model* model) const {
        return modelData_.contains(model);
    }

    VkBuffer MorphTargetManager::getBlendedBuffer(const Model* model) const {
        auto it = modelData_.find(model);
        if (it == modelData_.end()) {
            return VK_NULL_HANDLE;
        }
        return it->second.blendedBuffer->getBuffer();
    }

    uint64_t MorphTargetManager::getBlendedBufferAddress(const Model* model) const {
        auto it = modelData_.find(model);
        if (it == modelData_.end()) {
            return 0;
        }
        return it->second.blendedBuffer->getDeviceAddress();
    }

}  // namespace engine
