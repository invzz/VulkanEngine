#include "ModelLib/Resources/ResourceManager.hpp"

#include <algorithm>
#include <chrono>
#include <exception>
#include <functional>
#include <future>
#include <iomanip>
#include <ios>
#include <iostream>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <sstream>
#include <unordered_set>
#include <utility>

#include "Engine/Graphics/Device.hpp"
#include "Engine/Systems/IBL/VTexIO.hpp"

#include "ModelLib/Resources/MeshManager.hpp"
#include "ModelLib/Resources/Model.hpp"
#include "ModelLib/Resources/PBRMaterial.hpp"
#include "ModelLib/Resources/Texture.hpp"
#include "ModelLib/Resources/TextureManager.hpp"

// Simple SHA256 implementation for content hashing
#include <cstdint>
#include <cstring>
#include <string>
#include <thread>

namespace engine {

    // Simple FNV-1a hash (fast, good distribution)
    namespace {
        uint64_t hashBytes(const unsigned char* data, size_t length) {
            uint64_t hash = 14695981039346656037ULL;  // FNV offset basis
            for (size_t i = 0; i < length; ++i) {
                hash ^= data[i];
                hash *= 1099511628211ULL;  // FNV prime
            }
            return hash;
        }

    }  // namespace

    ResourceManager::ResourceManager(Device& device) : device_(device) {
        textureManager_ = std::make_unique<TextureManager>(device);
        meshManager_    = std::make_unique<MeshManager>(device);

        // Initialize thread pool with hardware concurrency
        size_t numThreads = std::thread::hardware_concurrency();
        if (numThreads == 0) {
            numThreads = 4;  // Fallback
        }
        initThreadPool(numThreads);
    }

    ResourceManager::~ResourceManager() {
        shutdownThreadPool();
    }

    std::string ResourceManager::makeTextureKey(const std::string& path, bool srgb) {
        // Include srgb flag in key since same texture can be loaded with different
        // formats
        return path + (srgb ? "|srgb" : "|linear");
    }

    std::string ResourceManager::makeModelKey(const std::string& path, bool enableTextures, bool loadMaterials, bool enableMorphTargets) {
        // Include loading flags in key since same model can be loaded with different
        // settings
        std::ostringstream oss;
        oss << path << "|tex=" << enableTextures << "|mat=" << loadMaterials << "|morph=" << enableMorphTargets;
        return oss.str();
    }

    std::shared_ptr<Texture> ResourceManager::loadTexture(const std::string& path, bool srgb, bool flipY, ResourcePriority priority) {
        std::string key = makeTextureKey(path, srgb) + (flipY ? "|flipY" : "");

        // Lock for thread-safe access
        std::scoped_lock const lock(textureMutex_);

        // Check if texture is already cached
        auto it = textureCache_.find(key);
        if (it != textureCache_.end()) {
            // Try to lock the weak_ptr to get a shared_ptr
            if (auto cachedTexture = it->second.lock()) {
                // Texture still exists, update LRU access time and priority
                updateTextureAccess(key, cachedTexture->getMemorySize(), priority);
                return cachedTexture;
            }

            // Texture was deleted, remove stale entry
            textureCache_.erase(it);
            // Remove from access tracking
            textureAccessOrder_.erase(std::remove_if(textureAccessOrder_.begin(), textureAccessOrder_.end(), [&key](const ResourceInfo& info) { return info.key == key; }), textureAccessOrder_.end());
        }

        // Load new texture
        auto         texture = std::make_shared<Texture>(device_, path, srgb, flipY);
        size_t const memSize = texture->getMemorySize();

        // Check memory budget and evict if necessary
        if (memoryBudget_ > 0) {
            cachedTextureMemory_ += memSize;
            while (cachedTextureMemory_ > memoryBudget_ && !textureCache_.empty()) {
                evictLRUTextures();
            }
        }

        // Cache the texture (as weak_ptr)
        textureCache_[key] = texture;
        updateTextureAccess(key, memSize, priority);

        // Register with TextureManager
        uint32_t const globalIndex = textureManager_->addTexture(texture);
        texture->setGlobalIndex(globalIndex);

        return texture;
    }

    std::shared_ptr<Model> ResourceManager::loadModel(const std::string& path, bool enableTextures, bool loadMaterials, bool enableMorphTargets, ResourcePriority priority) {
        std::string key = makeModelKey(path, enableTextures, loadMaterials, enableMorphTargets);

        // Lock for thread-safe access
        std::scoped_lock const lock(modelMutex_);

        // Check if model is already cached
        auto it = modelCache_.find(key);
        if (it != modelCache_.end()) {
            // Try to lock the weak_ptr to get a shared_ptr
            if (auto cachedModel = it->second.lock()) {
                // Model still exists, update LRU access time and priority
                updateModelAccess(key, cachedModel->getMemorySize(), priority);
                return cachedModel;
            }

            // Model was deleted, remove stale entry
            modelCache_.erase(it);
            // Remove from access tracking
            modelAccessOrder_.erase(std::remove_if(modelAccessOrder_.begin(), modelAccessOrder_.end(), [&key](const ResourceInfo& info) { return info.key == key; }), modelAccessOrder_.end());
        }

        // Load new model: choose importer based on file extension
        auto toLower = [](std::string s) {
            std::transform(s.begin(), s.end(), s.begin(), ::tolower);
            return s;
        };

        std::string ext;
        auto        pos = path.find_last_of('.');
        if (pos != std::string::npos) {
            ext = toLower(path.substr(pos + 1));
        }

        std::shared_ptr<Model> model;
        try {
            if (ext == "gltf" || ext == "glb") {
                // Use glTF importer for glTF files
                model = std::shared_ptr<Model>(Model::createModelFromGLTF(device_, path, false, true, true));
            } else {
                // Fall back to file loader (OBJ)
                model = std::shared_ptr<Model>(Model::createModelFromFile(device_, path, false, true, true));
            }
        } catch (const std::exception& e) {
            // Propagate error to caller
            throw;
        }

        size_t const memSize = model->getMemorySize();

        // Optionally load material textures according to flags
        if (enableTextures || loadMaterials) {
            try {
                for (auto& mat : model->getMaterials()) {
                    if (!mat.diffuseTexPath.empty() && enableTextures) {
                        mat.pbrMaterial.albedoMap = loadTexture(mat.diffuseTexPath, true, true);
                    }
                    if (!mat.normalTexPath.empty() && enableTextures) {
                        mat.pbrMaterial.normalMap = loadTexture(mat.normalTexPath, false, true);
                    }
                    if (!mat.roughnessTexPath.empty() && loadMaterials) {
                        mat.pbrMaterial.roughnessMap = loadTexture(mat.roughnessTexPath, false, true);
                    }
                    if (!mat.aoTexPath.empty() && loadMaterials) {
                        mat.pbrMaterial.aoMap = loadTexture(mat.aoTexPath, false, true);
                    }
                    if (!mat.emissiveTexPath.empty() && enableTextures) {
                        mat.pbrMaterial.emissiveMap = loadTexture(mat.emissiveTexPath, true, true);
                    }
                    if (!mat.specularGlossinessTexPath.empty() && loadMaterials) {
                        mat.pbrMaterial.specularGlossinessMap = loadTexture(mat.specularGlossinessTexPath, true, true);
                    }
                    if (!mat.transmissionTexPath.empty() && loadMaterials) {
                        mat.pbrMaterial.transmissionMap = loadTexture(mat.transmissionTexPath, false, true);
                    }
                    if (!mat.clearcoatTexPath.empty() && loadMaterials) {
                        mat.pbrMaterial.clearcoatMap = loadTexture(mat.clearcoatTexPath, false, true);
                    }
                    if (!mat.clearcoatRoughnessTexPath.empty() && loadMaterials) {
                        mat.pbrMaterial.clearcoatRoughnessMap = loadTexture(mat.clearcoatRoughnessTexPath, false, true);
                    }
                    if (!mat.clearcoatNormalTexPath.empty() && loadMaterials) {
                        mat.pbrMaterial.clearcoatNormalMap = loadTexture(mat.clearcoatNormalTexPath, false, true);
                    }
                }
            } catch (const std::exception& e) {
                std::cerr << "ResourceManager: failed loading material textures for " << path << ": " << e.what() << '\n';
            }
        }

        // Check memory budget and evict if necessary
        if (memoryBudget_ > 0) {
            cachedModelMemory_ += memSize;
            while (cachedModelMemory_ > memoryBudget_ && !modelCache_.empty()) {
                evictLRUModels();
            }
        }

        // Cache the model (as weak_ptr)
        modelCache_[key] = model;
        updateModelAccess(key, memSize, priority);

        // Register with MeshManager
        uint32_t const meshId = meshManager_->registerModel(model.get());
        model->setMeshId(meshId);

        return model;
    }

    std::shared_ptr<Texture> ResourceManager::loadTextureFromMemory(const unsigned char* data, size_t dataSize, const std::string& debugName, bool srgb, ResourcePriority priority) {
        // Compute content hash for deduplication
        std::string const contentHash = computeContentHash(data, dataSize);
        std::string       cacheKey;

        // Lock for thread-safe access
        std::scoped_lock const lock(textureMutex_);

        // Check if we've already loaded this exact content
        auto hashIt = contentHashToKey_.find(contentHash);
        if (hashIt != contentHashToKey_.end()) {
            cacheKey = hashIt->second;
            auto it  = textureCache_.find(cacheKey);
            if (it != textureCache_.end()) {
                if (auto cachedTexture = it->second.lock()) {
                    // Same content already loaded, return cached instance
                    updateTextureAccess(cacheKey, cachedTexture->getMemorySize(), priority);
                    return cachedTexture;
                }
            }
        }

        // Create unique cache key: hash + debug name + format
        cacheKey = "embedded:" + contentHash + "|" + debugName + (srgb ? "|srgb" : "|linear");

        // Check if this specific key is cached (shouldn't happen, but safe check)
        auto it = textureCache_.find(cacheKey);
        if (it != textureCache_.end()) {
            if (auto cachedTexture = it->second.lock()) {
                updateTextureAccess(cacheKey, cachedTexture->getMemorySize(), priority);
                return cachedTexture;
            }
        }

        // Load texture from memory
        // Note: This requires a Texture constructor that accepts memory data
        // For now, we'll need to save to a temp file or extend Texture class
        // As a workaround, we use the file-based loader with a unique temp path

        // TODO: Implement Texture::createFromMemory() for true zero-copy loading
        // For now, fall back to file-based loading
        std::string const tempPath = "/tmp/embedded_texture_" + contentHash + ".dat";
        // In production, you'd write data to tempPath here

        auto         texture = std::make_shared<Texture>(device_, tempPath, srgb);
        size_t const memSize = texture->getMemorySize();

        // Check memory budget and evict if necessary
        if (memoryBudget_ > 0) {
            cachedTextureMemory_ += memSize;
            while (cachedTextureMemory_ > memoryBudget_ && !textureCache_.empty()) {
                evictLRUTextures();
            }
        }

        // Cache the texture
        textureCache_[cacheKey]        = texture;
        contentHashToKey_[contentHash] = cacheKey;
        updateTextureAccess(cacheKey, memSize, priority);

        // Register with TextureManager
        uint32_t const globalIndex = textureManager_->addTexture(texture);
        texture->setGlobalIndex(globalIndex);

        return texture;
    }

    size_t ResourceManager::garbageCollect() {
        size_t removedCount = 0;

        // Clean up textures
        {
            std::scoped_lock const lock(textureMutex_);
            cachedTextureMemory_ = 0;
            std::unordered_set<std::string> removedKeys;

            for (auto it = textureCache_.begin(); it != textureCache_.end();) {
                const std::string& key = it->first;
                if (auto texture = it->second.lock()) {
                    cachedTextureMemory_ += texture->getMemorySize();
                    ++it;
                    continue;
                }

                // Expired entry; erase from cache and track for access-order cleanup.
                removedKeys.insert(key);
                it = textureCache_.erase(it);
                ++removedCount;
            }

            if (!removedKeys.empty()) {
                // Remove all dead entries from access tracking in one pass.
                auto const removed = std::ranges::remove_if(textureAccessOrder_, [&removedKeys](const ResourceInfo& info) { return removedKeys.contains(info.key); });
                textureAccessOrder_.erase(removed.begin(), removed.end());

                // Remove stale content-hash indirections for textures that are gone.
                for (auto it = contentHashToKey_.begin(); it != contentHashToKey_.end();) {
                    if (removedKeys.contains(it->second)) {
                        it = contentHashToKey_.erase(it);
                    } else {
                        ++it;
                    }
                }
            }
        }

        // Clean up models
        {
            std::scoped_lock const lock(modelMutex_);
            cachedModelMemory_ = 0;
            std::unordered_set<std::string> removedKeys;

            for (auto it = modelCache_.begin(); it != modelCache_.end();) {
                const std::string& key = it->first;
                if (auto model = it->second.lock()) {
                    cachedModelMemory_ += model->getMemorySize();
                    ++it;
                    continue;
                }

                // Expired entry; erase from cache and track for access-order cleanup.
                removedKeys.insert(key);
                it = modelCache_.erase(it);
                ++removedCount;
            }

            if (!removedKeys.empty()) {
                auto const removed = std::ranges::remove_if(modelAccessOrder_, [&removedKeys](const ResourceInfo& info) { return removedKeys.contains(info.key); });
                modelAccessOrder_.erase(removed.begin(), removed.end());
            }
        }

        return removedCount;
    }

    size_t ResourceManager::getMemoryUsage() const {
        size_t totalMemory = 0;

        // Texture memory (accurate calculation)
        {
            std::scoped_lock const lock(textureMutex_);
            for (const auto& [key, weakTexture] : textureCache_) {
                if (auto texture = weakTexture.lock()) {
                    totalMemory += texture->getMemorySize();
                }
            }
        }

        // Model memory (accurate calculation)
        {
            std::scoped_lock const lock(modelMutex_);
            for (const auto& [key, weakModel] : modelCache_) {
                if (auto model = weakModel.lock()) {
                    totalMemory += model->getMemorySize();
                }
            }
        }

        return totalMemory;
    }

    size_t ResourceManager::getCachedTextureCount() const {
        std::scoped_lock const lock(textureMutex_);

        // Count only alive textures
        size_t count = 0;
        for (const auto& [key, weakTexture] : textureCache_) {
            if (!weakTexture.expired()) {
                ++count;
            }
        }
        return count;
    }

    size_t ResourceManager::getCachedModelCount() const {
        std::scoped_lock const lock(modelMutex_);

        // Count only alive models
        size_t count = 0;
        for (const auto& [key, weakModel] : modelCache_) {
            if (!weakModel.expired()) {
                ++count;
            }
        }
        return count;
    }

    void ResourceManager::clearAll() {
        {
            std::scoped_lock const lock(textureMutex_);
            textureCache_.clear();
            textureAccessOrder_.clear();
            cachedTextureMemory_ = 0;
        }

        {
            std::scoped_lock const lock(modelMutex_);
            modelCache_.clear();
            modelAccessOrder_.clear();
            cachedModelMemory_ = 0;
        }
    }

    bool ResourceManager::isTextureCached(const std::string& path) const {
        std::scoped_lock const lock(textureMutex_);

        // Check both srgb and linear variants
        std::string const srgbKey   = makeTextureKey(path, true);
        std::string const linearKey = makeTextureKey(path, false);

        auto srgbIt   = textureCache_.find(srgbKey);
        auto linearIt = textureCache_.find(linearKey);

        bool const srgbCached   = (srgbIt != textureCache_.end() && !srgbIt->second.expired());
        bool const linearCached = (linearIt != textureCache_.end() && !linearIt->second.expired());

        return srgbCached || linearCached;
    }

    bool ResourceManager::isModelCached(const std::string& path) const {
        std::scoped_lock const lock(modelMutex_);

        // Check if any variant of this model path is cached
        return std::ranges::any_of(modelCache_, [&path](const auto& pair) {
            const auto& [key, weakModel] = pair;
            return key.starts_with(path) && !weakModel.expired();
        });
    }

    void ResourceManager::setMemoryBudget(size_t budgetBytes) {
        memoryBudget_ = budgetBytes;

        // Evict resources if we're already over budget
        if (budgetBytes > 0) {
            {
                std::scoped_lock const lock(textureMutex_);
                while (cachedTextureMemory_ > memoryBudget_ && !textureCache_.empty()) {
                    evictLRUTextures();
                }
            }

            {
                std::scoped_lock const lock(modelMutex_);
                while (cachedModelMemory_ > memoryBudget_ && !modelCache_.empty()) {
                    evictLRUModels();
                }
            }
        }
    }

    void ResourceManager::updateTextureAccess(const std::string& key, size_t memorySize, ResourcePriority priority) {
        // Remove existing entry if present
        auto const removed = std::ranges::remove_if(textureAccessOrder_, [&key](const ResourceInfo& info) { return info.key == key; });
        textureAccessOrder_.erase(removed.begin(), removed.end());

        // Add to end (most recently used) with priority
        textureAccessOrder_.push_back({key, memorySize, getCurrentTime(), priority});
    }

    void ResourceManager::updateModelAccess(const std::string& key, size_t memorySize, ResourcePriority priority) {
        // Remove existing entry if present
        auto const removed = std::ranges::remove_if(modelAccessOrder_, [&key](const ResourceInfo& info) { return info.key == key; });
        modelAccessOrder_.erase(removed.begin(), removed.end());

        // Add to end (most recently used) with priority
        modelAccessOrder_.push_back({key, memorySize, getCurrentTime(), priority});
    }

    void ResourceManager::evictLRUTextures() {
        if (textureAccessOrder_.empty()) {
            return;
        }

        // Sort by priority first (low priority first), then by access time (oldest
        // first)
        std::ranges::sort(textureAccessOrder_, [](const ResourceInfo& a, const ResourceInfo& b) {
            if (a.priority != b.priority) {
                return a.priority < b.priority;  // Lower priority evicted first
            }
            return a.lastAccessTime < b.lastAccessTime;  // Then oldest
        });

        // Skip CRITICAL priority resources
        size_t evictIndex = 0;
        while (evictIndex < textureAccessOrder_.size() && textureAccessOrder_[evictIndex].priority == ResourcePriority::CRITICAL) {
            ++evictIndex;
        }

        if (evictIndex >= textureAccessOrder_.size()) {
            // All resources are CRITICAL, cannot evict
            return;
        }

        // Evict resource at evictIndex
        const auto& toEvict = textureAccessOrder_[evictIndex];
        auto        it      = textureCache_.find(toEvict.key);
        if (it != textureCache_.end()) {
            textureCache_.erase(it);
            cachedTextureMemory_ -= toEvict.memorySize;
        }
        textureAccessOrder_.erase(std::next(textureAccessOrder_.begin(), static_cast<std::vector<ResourceInfo>::difference_type>(evictIndex)));
    }

    void ResourceManager::evictLRUModels() {
        if (modelAccessOrder_.empty()) {
            return;
        }

        // Sort by priority first (low priority first), then by access time (oldest
        // first)
        std::ranges::sort(modelAccessOrder_, [](const ResourceInfo& a, const ResourceInfo& b) {
            if (a.priority != b.priority) {
                return a.priority < b.priority;  // Lower priority evicted first
            }
            return a.lastAccessTime < b.lastAccessTime;  // Then oldest
        });

        // Skip CRITICAL priority resources
        size_t evictIndex = 0;
        while (evictIndex < modelAccessOrder_.size() && modelAccessOrder_[evictIndex].priority == ResourcePriority::CRITICAL) {
            ++evictIndex;
        }

        if (evictIndex >= modelAccessOrder_.size()) {
            // All resources are CRITICAL, cannot evict
            return;
        }

        // Evict resource at evictIndex
        const auto& toEvict = modelAccessOrder_[evictIndex];
        auto        it      = modelCache_.find(toEvict.key);
        if (it != modelCache_.end()) {
            modelCache_.erase(it);
            cachedModelMemory_ -= toEvict.memorySize;
        }
        modelAccessOrder_.erase(modelAccessOrder_.begin() + static_cast<std::vector<ResourceInfo>::difference_type>(evictIndex));
    }

    uint64_t ResourceManager::getCurrentTime() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    std::string ResourceManager::computeContentHash(const unsigned char* data, size_t dataSize) {
        // Use FNV-1a hash for fast content-based deduplication
        uint64_t const hash = hashBytes(data, dataSize);

        // Convert to hex string
        std::ostringstream oss;
        oss << std::hex << std::setfill('0') << std::setw(16) << hash;
        return oss.str();
    }

    // ============================================================================
    // ASYNC LOADING IMPLEMENTATION
    // ============================================================================

    void ResourceManager::initThreadPool(size_t numThreads) {
        workerThreads_.reserve(numThreads);
        for (size_t i = 0; i < numThreads; ++i) {
            workerThreads_.emplace_back(&ResourceManager::workerThreadLoop, this);
        }
    }

    void ResourceManager::shutdownThreadPool() {
        {
            std::scoped_lock const lock(taskQueueMutex_);
            shutdownThreadPool_ = true;
        }
        taskQueueCV_.notify_all();

        for (auto& thread : workerThreads_) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        workerThreads_.clear();
    }

    void ResourceManager::workerThreadLoop() {
        while (true) {
            std::function<void()> task;

            {
                std::unique_lock<std::mutex> lock(taskQueueMutex_);
                taskQueueCV_.wait(lock, [this] { return shutdownThreadPool_ || !taskQueue_.empty(); });

                if (shutdownThreadPool_ && taskQueue_.empty()) {
                    return;
                }

                if (!taskQueue_.empty()) {
                    task = std::move(taskQueue_.front());
                    taskQueue_.pop();
                    activeTasks_++;
                }
            }

            if (task) {
                task();
                activeTasks_--;
            }
        }
    }

    std::future<std::shared_ptr<Texture>> ResourceManager::loadTextureAsync(const std::string& path, bool srgb, ResourcePriority priority) {
        // Check if already cached (fast path)
        std::string const key = makeTextureKey(path, srgb);
        {
            std::scoped_lock const lock(textureMutex_);
            auto                   it = textureCache_.find(key);
            if (it != textureCache_.end()) {
                if (auto existingTexture = it->second.lock()) {
                    // Update access time
                    updateTextureAccess(key, existingTexture->getMemorySize(), priority);

                    // Return immediately resolved future
                    std::promise<std::shared_ptr<Texture>> promise;
                    promise.set_value(existingTexture);
                    return promise.get_future();
                }
            }
        }

        // Create promise/future pair
        auto                                  promise = std::make_shared<std::promise<std::shared_ptr<Texture>>>();
        std::future<std::shared_ptr<Texture>> future  = promise->get_future();

        // Enqueue async task
        {
            std::scoped_lock const lock(taskQueueMutex_);
            taskQueue_.emplace([this, path, srgb, priority, promise]() {
                try {
                    // Load texture synchronously on worker thread
                    auto texture = loadTexture(path, srgb, false, priority);
                    promise->set_value(texture);
                } catch (const std::exception& /*e*/) {
                    promise->set_exception(std::current_exception());
                }
            });
        }
        taskQueueCV_.notify_one();

        return future;
    }

    std::future<std::shared_ptr<Model>> ResourceManager::loadModelAsync(const std::string& path, bool enableTextures, bool loadMaterials, bool enableMorphTargets, ResourcePriority priority) {
        // Check if already cached (fast path)
        std::string const key = makeModelKey(path, enableTextures, loadMaterials, enableMorphTargets);
        {
            std::scoped_lock const lock(modelMutex_);
            auto                   it = modelCache_.find(key);
            if (it != modelCache_.end()) {
                if (auto existingModel = it->second.lock()) {
                    // Update access time
                    updateModelAccess(key, existingModel->getMemorySize(), priority);

                    // Return immediately resolved future
                    std::promise<std::shared_ptr<Model>> promise;
                    promise.set_value(existingModel);
                    return promise.get_future();
                }
            }
        }

        // Create promise/future pair
        auto                                promise = std::make_shared<std::promise<std::shared_ptr<Model>>>();
        std::future<std::shared_ptr<Model>> future  = promise->get_future();

        // Enqueue async task
        {
            std::scoped_lock const lock(taskQueueMutex_);
            taskQueue_.emplace([this, path, enableTextures, loadMaterials, enableMorphTargets, priority, promise]() {
                try {
                    // Load model synchronously on worker thread
                    auto model = loadModel(path, enableTextures, loadMaterials, enableMorphTargets, priority);
                    promise->set_value(model);
                } catch (const std::exception& /*e*/) {
                    promise->set_exception(std::current_exception());
                }
            });
        }
        taskQueueCV_.notify_one();

        return future;
    }

    AsyncLoadId ResourceManager::enqueueModelLoad(
        const std::string&                                 path,
        bool                                               enableTextures,
        bool                                               loadMaterials,
        bool                                               enableMorphTargets,
        ResourcePriority                                   priority,
        std::function<void(const std::shared_ptr<Model>&)> onComplete,
        std::function<void(const std::string&)>            onFailed) {
        AsyncLoadId const id = nextAsyncLoadId_.fetch_add(1);

        // Start from existing async path and wrap into a tracked shared_future.
        std::future<std::shared_ptr<Model>>        future = loadModelAsync(path, enableTextures, loadMaterials, enableMorphTargets, priority);
        std::shared_future<std::shared_ptr<Model>> shared = std::move(future).share();

        AsyncModelTaskRecord record;
        record.id         = id;
        record.path       = path;
        record.status     = LoadStatus::PENDING;
        record.progress   = 0.0f;
        record.future     = shared;
        record.onComplete = std::move(onComplete);
        record.onFailed   = std::move(onFailed);

        {
            std::scoped_lock const lock(asyncTasksMutex_);
            asyncModelTasks_[id] = std::move(record);
        }

        return id;
    }

    bool ResourceManager::tryGetModelLoadResult(AsyncLoadId id, std::shared_ptr<Model>& outModel, std::string* outError) {
        std::shared_future<std::shared_ptr<Model>> shared;
        {
            std::scoped_lock const lock(asyncTasksMutex_);
            auto                   it = asyncModelTasks_.find(id);
            if (it == asyncModelTasks_.end()) {
                if (outError != nullptr) {
                    *outError = "Invalid async load id";
                }
                return true;
            }

            auto& task = it->second;
            if (task.cancelled) {
                if (outError != nullptr) {
                    *outError = "Cancelled";
                }
                return true;
            }

            if (task.status == LoadStatus::COMPLETE) {
                outModel = task.result;
                if (outError != nullptr) {
                    outError->clear();
                }
                return true;
            }

            if (task.status == LoadStatus::FAILED) {
                if (outError != nullptr) {
                    *outError = task.error;
                }
                return true;
            }

            shared = task.future;
        }

        if (!shared.valid() || shared.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
            return false;
        }

        try {
            auto result = shared.get();
            {
                std::scoped_lock const lock(asyncTasksMutex_);
                auto                   it = asyncModelTasks_.find(id);
                if (it != asyncModelTasks_.end()) {
                    it->second.result   = result;
                    it->second.status   = LoadStatus::COMPLETE;
                    it->second.progress = 1.0f;
                }
            }
            outModel = std::move(result);
            if (outError != nullptr) {
                outError->clear();
            }
        } catch (const std::exception& e) {
            std::string error = e.what();
            {
                std::scoped_lock const lock(asyncTasksMutex_);
                auto                   it = asyncModelTasks_.find(id);
                if (it != asyncModelTasks_.end()) {
                    it->second.error    = error;
                    it->second.status   = LoadStatus::FAILED;
                    it->second.progress = 1.0f;
                }
            }
            if (outError != nullptr) {
                *outError = error;
            }
        }

        return true;
    }

    std::vector<AsyncLoadSnapshot> ResourceManager::getAsyncLoadSnapshots() const {
        std::vector<AsyncLoadSnapshot> snapshots;

        std::scoped_lock const lock(asyncTasksMutex_);
        snapshots.reserve(asyncModelTasks_.size());

        for (const auto& [id, task] : asyncModelTasks_) {
            AsyncLoadSnapshot snap;
            snap.id       = id;
            snap.status   = task.cancelled ? LoadStatus::FAILED : task.status;
            snap.path     = task.path;
            snap.progress = task.progress;
            snap.isModel  = true;
            snap.hasError = !task.error.empty();
            snap.error    = task.error;
            snapshots.push_back(std::move(snap));
        }

        return snapshots;
    }

    void ResourceManager::updateAsyncCallbacks() {
        std::vector<AsyncLoadId> toDispatch;

        {
            std::scoped_lock const lock(asyncTasksMutex_);
            for (auto& [id, task] : asyncModelTasks_) {
                if (task.cancelled) {
                    task.callbackDispatched = true;
                    continue;
                }

                if (task.status == LoadStatus::COMPLETE || task.status == LoadStatus::FAILED) {
                    if (!task.callbackDispatched && (task.onComplete || task.onFailed)) {
                        toDispatch.push_back(id);
                    }
                    continue;
                }

                // Transition to LOADING once worker has picked up work.
                if (task.status == LoadStatus::PENDING) {
                    task.status   = LoadStatus::LOADING;
                    task.progress = 0.25f;
                }

                if (task.future.valid() && task.future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                    try {
                        task.result   = task.future.get();
                        task.status   = LoadStatus::COMPLETE;
                        task.progress = 1.0f;
                    } catch (const std::exception& e) {
                        task.error    = e.what();
                        task.status   = LoadStatus::FAILED;
                        task.progress = 1.0f;
                    }
                    if (task.onComplete || task.onFailed) {
                        toDispatch.push_back(id);
                    }
                }
            }
        }

        for (AsyncLoadId id : toDispatch) {
            std::function<void(const std::shared_ptr<Model>&)> onComplete;
            std::function<void(const std::string&)>            onFailed;
            std::shared_ptr<Model>                             result;
            std::string                                        error;
            bool                                               success = false;

            {
                std::scoped_lock const lock(asyncTasksMutex_);
                auto                   it = asyncModelTasks_.find(id);
                if (it == asyncModelTasks_.end() || it->second.callbackDispatched || it->second.cancelled) {
                    continue;
                }

                onComplete                    = it->second.onComplete;
                onFailed                      = it->second.onFailed;
                result                        = it->second.result;
                error                         = it->second.error;
                success                       = (it->second.status == LoadStatus::COMPLETE);
                it->second.callbackDispatched = true;
            }

            if (success) {
                if (onComplete) {
                    onComplete(result);
                }
            } else {
                if (onFailed) {
                    onFailed(error.empty() ? std::string("Async model load failed") : error);
                }
            }
        }

        {
            std::scoped_lock const lock(asyncTasksMutex_);
            for (auto it = asyncModelTasks_.begin(); it != asyncModelTasks_.end();) {
                const auto& task     = it->second;
                const bool  terminal = task.cancelled || task.status == LoadStatus::COMPLETE || task.status == LoadStatus::FAILED;
                if (terminal && task.callbackDispatched) {
                    it = asyncModelTasks_.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }

    void ResourceManager::cancelModelLoad(AsyncLoadId id) {
        std::scoped_lock const lock(asyncTasksMutex_);
        auto                   it = asyncModelTasks_.find(id);
        if (it != asyncModelTasks_.end()) {
            it->second.cancelled = true;
            it->second.status    = LoadStatus::FAILED;
            it->second.progress  = 1.0f;
            if (it->second.error.empty()) {
                it->second.error = "Cancelled";
            }
        }
    }

    size_t ResourceManager::getPendingAsyncLoads() const {
        std::scoped_lock const lock(taskQueueMutex_);
        return taskQueue_.size() + activeTasks_;
    }

    void ResourceManager::waitForAsyncLoads() const {
        while (getPendingAsyncLoads() > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

}  // namespace engine
