#include "3dEngine/ResourceManager.hpp"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>

#include "3dEngine/Model.hpp"
#include "3dEngine/Texture.hpp"
#include "3dEngine/TextureManager.hpp"

// Simple SHA256 implementation for content hashing
#include <cstdint>
#include <cstring>

namespace engine {

  // Simple FNV-1a hash (fast, good distribution)
  static uint64_t hashBytes(const unsigned char* data, size_t length)
  {
    uint64_t hash = 14695981039346656037ULL; // FNV offset basis
    for (size_t i = 0; i < length; ++i)
    {
      hash ^= data[i];
      hash *= 1099511628211ULL; // FNV prime
    }
    return hash;
  }

  ResourceManager::ResourceManager(Device& device) : device_(device)
  {
    textureManager_ = std::make_unique<TextureManager>(device);
    meshManager_    = std::make_unique<MeshManager>(device);

    // Initialize thread pool with hardware concurrency
    size_t numThreads = std::thread::hardware_concurrency();
    if (numThreads == 0) numThreads = 4; // Fallback
    initThreadPool(numThreads);
  }

  ResourceManager::~ResourceManager()
  {
    shutdownThreadPool();
  }

  std::string ResourceManager::makeTextureKey(const std::string& path, bool srgb) const
  {
    // Include srgb flag in key since same texture can be loaded with different formats
    return path + (srgb ? "|srgb" : "|linear");
  }

  std::string ResourceManager::makeModelKey(const std::string& path, bool enableTextures, bool loadMaterials, bool enableMorphTargets) const
  {
    // Include loading flags in key since same model can be loaded with different settings
    std::ostringstream oss;
    oss << path << "|tex=" << enableTextures << "|mat=" << loadMaterials << "|morph=" << enableMorphTargets;
    return oss.str();
  }

  std::shared_ptr<Texture> ResourceManager::loadTexture(const std::string& path, bool srgb, ResourcePriority priority)
  {
    std::string key = makeTextureKey(path, srgb);

    // Lock for thread-safe access
    std::lock_guard<std::mutex> lock(textureMutex_);

    // Check if texture is already cached
    auto it = textureCache_.find(key);
    if (it != textureCache_.end())
    {
      // Try to lock the weak_ptr to get a shared_ptr
      if (auto cachedTexture = it->second.lock())
      {
        // Texture still exists, update LRU access time and priority
        updateTextureAccess(key, cachedTexture->getMemorySize(), priority);
        return cachedTexture;
      }
      else
      {
        // Texture was deleted, remove stale entry
        textureCache_.erase(it);
        // Remove from access tracking
        textureAccessOrder_.erase(
                std::remove_if(textureAccessOrder_.begin(), textureAccessOrder_.end(), [&key](const ResourceInfo& info) { return info.key == key; }),
                textureAccessOrder_.end());
      }
    }

    // Load new texture
    auto   texture = std::make_shared<Texture>(device_, path, srgb);
    size_t memSize = texture->getMemorySize();

    // Check memory budget and evict if necessary
    if (memoryBudget_ > 0)
    {
      cachedTextureMemory_ += memSize;
      while (cachedTextureMemory_ > memoryBudget_ && !textureCache_.empty())
      {
        evictLRUTextures();
      }
    }

    // Cache the texture (as weak_ptr)
    textureCache_[key] = texture;
    updateTextureAccess(key, memSize, priority);

    // Register with TextureManager
    uint32_t globalIndex = textureManager_->addTexture(texture);
    texture->setGlobalIndex(globalIndex);

    return texture;
  }

  std::shared_ptr<Model>
  ResourceManager::loadModel(const std::string& path, bool enableTextures, bool loadMaterials, bool enableMorphTargets, ResourcePriority priority)
  {
    std::string key = makeModelKey(path, enableTextures, loadMaterials, enableMorphTargets);

    // Lock for thread-safe access
    std::lock_guard<std::mutex> lock(modelMutex_);

    // Check if model is already cached
    auto it = modelCache_.find(key);
    if (it != modelCache_.end())
    {
      // Try to lock the weak_ptr to get a shared_ptr
      if (auto cachedModel = it->second.lock())
      {
        // Model still exists, update LRU access time and priority
        updateModelAccess(key, cachedModel->getMemorySize(), priority);
        return cachedModel;
      }
      else
      {
        // Model was deleted, remove stale entry
        modelCache_.erase(it);
        // Remove from access tracking
        modelAccessOrder_.erase(
                std::remove_if(modelAccessOrder_.begin(), modelAccessOrder_.end(), [&key](const ResourceInfo& info) { return info.key == key; }),
                modelAccessOrder_.end());
      }
    }

    // Load new model
    auto   model   = std::shared_ptr<Model>(Model::createModelFromFile(device_, path, enableTextures, loadMaterials, enableMorphTargets));
    size_t memSize = model->getMemorySize();

    // Check memory budget and evict if necessary
    if (memoryBudget_ > 0)
    {
      cachedModelMemory_ += memSize;
      while (cachedModelMemory_ > memoryBudget_ && !modelCache_.empty())
      {
        evictLRUModels();
      }
    }

    // Cache the model (as weak_ptr)
    modelCache_[key] = model;
    updateModelAccess(key, memSize, priority);

    // Register with MeshManager
    uint32_t meshId = meshManager_->registerModel(model.get());
    model->setMeshId(meshId);

    return model;
  }

  std::shared_ptr<Texture>
  ResourceManager::loadTextureFromMemory(const unsigned char* data, size_t dataSize, const std::string& debugName, bool srgb, ResourcePriority priority)
  {
    // Compute content hash for deduplication
    std::string contentHash = computeContentHash(data, dataSize);
    std::string cacheKey;

    // Lock for thread-safe access
    std::lock_guard<std::mutex> lock(textureMutex_);

    // Check if we've already loaded this exact content
    auto hashIt = contentHashToKey_.find(contentHash);
    if (hashIt != contentHashToKey_.end())
    {
      cacheKey = hashIt->second;
      auto it  = textureCache_.find(cacheKey);
      if (it != textureCache_.end())
      {
        if (auto cachedTexture = it->second.lock())
        {
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
    if (it != textureCache_.end())
    {
      if (auto cachedTexture = it->second.lock())
      {
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
    std::string tempPath = "/tmp/embedded_texture_" + contentHash + ".dat";
    // In production, you'd write data to tempPath here

    auto   texture = std::make_shared<Texture>(device_, tempPath, srgb);
    size_t memSize = texture->getMemorySize();

    // Check memory budget and evict if necessary
    if (memoryBudget_ > 0)
    {
      cachedTextureMemory_ += memSize;
      while (cachedTextureMemory_ > memoryBudget_ && !textureCache_.empty())
      {
        evictLRUTextures();
      }
    }

    // Cache the texture
    textureCache_[cacheKey]        = texture;
    contentHashToKey_[contentHash] = cacheKey;
    updateTextureAccess(cacheKey, memSize, priority);

    // Register with TextureManager
    uint32_t globalIndex = textureManager_->addTexture(texture);
    texture->setGlobalIndex(globalIndex);

    return texture;
  }

  size_t ResourceManager::garbageCollect()
  {
    size_t removedCount = 0;

    // Clean up textures
    {
      std::lock_guard<std::mutex> lock(textureMutex_);
      for (auto it = textureCache_.begin(); it != textureCache_.end();)
      {
        if (it->second.expired())
        {
          // Remove from access tracking
          textureAccessOrder_.erase(
                  std::remove_if(textureAccessOrder_.begin(), textureAccessOrder_.end(), [&it](const ResourceInfo& info) { return info.key == it->first; }),
                  textureAccessOrder_.end());

          it = textureCache_.erase(it);
          ++removedCount;
        }
        else
        {
          ++it;
        }
      }

      // Recalculate cached memory
      cachedTextureMemory_ = 0;
      for (const auto& [key, weakTexture] : textureCache_)
      {
        if (auto texture = weakTexture.lock())
        {
          cachedTextureMemory_ += texture->getMemorySize();
        }
      }
    }

    // Clean up models
    {
      std::lock_guard<std::mutex> lock(modelMutex_);
      for (auto it = modelCache_.begin(); it != modelCache_.end();)
      {
        if (it->second.expired())
        {
          // Remove from access tracking
          modelAccessOrder_.erase(
                  std::remove_if(modelAccessOrder_.begin(), modelAccessOrder_.end(), [&it](const ResourceInfo& info) { return info.key == it->first; }),
                  modelAccessOrder_.end());

          it = modelCache_.erase(it);
          ++removedCount;
        }
        else
        {
          ++it;
        }
      }

      // Recalculate cached memory
      cachedModelMemory_ = 0;
      for (const auto& [key, weakModel] : modelCache_)
      {
        if (auto model = weakModel.lock())
        {
          cachedModelMemory_ += model->getMemorySize();
        }
      }
    }

    return removedCount;
  }

  size_t ResourceManager::getMemoryUsage() const
  {
    size_t totalMemory = 0;

    // Texture memory (accurate calculation)
    {
      std::lock_guard<std::mutex> lock(textureMutex_);
      for (const auto& [key, weakTexture] : textureCache_)
      {
        if (auto texture = weakTexture.lock())
        {
          totalMemory += texture->getMemorySize();
        }
      }
    }

    // Model memory (accurate calculation)
    {
      std::lock_guard<std::mutex> lock(modelMutex_);
      for (const auto& [key, weakModel] : modelCache_)
      {
        if (auto model = weakModel.lock())
        {
          totalMemory += model->getMemorySize();
        }
      }
    }

    return totalMemory;
  }

  size_t ResourceManager::getCachedTextureCount() const
  {
    std::lock_guard<std::mutex> lock(textureMutex_);

    // Count only alive textures
    size_t count = 0;
    for (const auto& [key, weakTexture] : textureCache_)
    {
      if (!weakTexture.expired())
      {
        ++count;
      }
    }
    return count;
  }

  size_t ResourceManager::getCachedModelCount() const
  {
    std::lock_guard<std::mutex> lock(modelMutex_);

    // Count only alive models
    size_t count = 0;
    for (const auto& [key, weakModel] : modelCache_)
    {
      if (!weakModel.expired())
      {
        ++count;
      }
    }
    return count;
  }

  void ResourceManager::clearAll()
  {
    {
      std::lock_guard<std::mutex> lock(textureMutex_);
      textureCache_.clear();
      textureAccessOrder_.clear();
      cachedTextureMemory_ = 0;
    }

    {
      std::lock_guard<std::mutex> lock(modelMutex_);
      modelCache_.clear();
      modelAccessOrder_.clear();
      cachedModelMemory_ = 0;
    }
  }

  bool ResourceManager::isTextureCached(const std::string& path) const
  {
    std::lock_guard<std::mutex> lock(textureMutex_);

    // Check both srgb and linear variants
    std::string srgbKey   = makeTextureKey(path, true);
    std::string linearKey = makeTextureKey(path, false);

    auto srgbIt   = textureCache_.find(srgbKey);
    auto linearIt = textureCache_.find(linearKey);

    bool srgbCached   = (srgbIt != textureCache_.end() && !srgbIt->second.expired());
    bool linearCached = (linearIt != textureCache_.end() && !linearIt->second.expired());

    return srgbCached || linearCached;
  }

  bool ResourceManager::isModelCached(const std::string& path) const
  {
    std::lock_guard<std::mutex> lock(modelMutex_);

    // Check if any variant of this model path is cached
    for (const auto& [key, weakModel] : modelCache_)
    {
      if (key.find(path) == 0 && !weakModel.expired())
      {
        return true;
      }
    }
    return false;
  }

  void ResourceManager::setMemoryBudget(size_t budgetBytes)
  {
    memoryBudget_ = budgetBytes;

    // Evict resources if we're already over budget
    if (budgetBytes > 0)
    {
      {
        std::lock_guard<std::mutex> lock(textureMutex_);
        while (cachedTextureMemory_ > memoryBudget_ && !textureCache_.empty())
        {
          evictLRUTextures();
        }
      }

      {
        std::lock_guard<std::mutex> lock(modelMutex_);
        while (cachedModelMemory_ > memoryBudget_ && !modelCache_.empty())
        {
          evictLRUModels();
        }
      }
    }
  }

  void ResourceManager::updateTextureAccess(const std::string& key, size_t memorySize, ResourcePriority priority)
  {
    // Remove existing entry if present
    textureAccessOrder_.erase(
            std::remove_if(textureAccessOrder_.begin(), textureAccessOrder_.end(), [&key](const ResourceInfo& info) { return info.key == key; }),
            textureAccessOrder_.end());

    // Add to end (most recently used) with priority
    textureAccessOrder_.push_back({key, memorySize, getCurrentTime(), priority});
  }

  void ResourceManager::updateModelAccess(const std::string& key, size_t memorySize, ResourcePriority priority)
  {
    // Remove existing entry if present
    modelAccessOrder_.erase(std::remove_if(modelAccessOrder_.begin(), modelAccessOrder_.end(), [&key](const ResourceInfo& info) { return info.key == key; }),
                            modelAccessOrder_.end());

    // Add to end (most recently used) with priority
    modelAccessOrder_.push_back({key, memorySize, getCurrentTime(), priority});
  }

  void ResourceManager::evictLRUTextures()
  {
    if (textureAccessOrder_.empty()) return;

    // Sort by priority first (low priority first), then by access time (oldest first)
    std::sort(textureAccessOrder_.begin(), textureAccessOrder_.end(), [](const ResourceInfo& a, const ResourceInfo& b) {
      if (a.priority != b.priority) return a.priority < b.priority; // Lower priority evicted first
      return a.lastAccessTime < b.lastAccessTime;                   // Then oldest
    });

    // Skip CRITICAL priority resources
    size_t evictIndex = 0;
    while (evictIndex < textureAccessOrder_.size() && textureAccessOrder_[evictIndex].priority == ResourcePriority::CRITICAL)
    {
      ++evictIndex;
    }

    if (evictIndex >= textureAccessOrder_.size())
    {
      // All resources are CRITICAL, cannot evict
      return;
    }

    // Evict resource at evictIndex
    const auto& toEvict = textureAccessOrder_[evictIndex];
    auto        it      = textureCache_.find(toEvict.key);
    if (it != textureCache_.end())
    {
      textureCache_.erase(it);
      cachedTextureMemory_ -= toEvict.memorySize;
    }
    textureAccessOrder_.erase(textureAccessOrder_.begin() + evictIndex);
  }

  void ResourceManager::evictLRUModels()
  {
    if (modelAccessOrder_.empty()) return;

    // Sort by priority first (low priority first), then by access time (oldest first)
    std::sort(modelAccessOrder_.begin(), modelAccessOrder_.end(), [](const ResourceInfo& a, const ResourceInfo& b) {
      if (a.priority != b.priority) return a.priority < b.priority; // Lower priority evicted first
      return a.lastAccessTime < b.lastAccessTime;                   // Then oldest
    });

    // Skip CRITICAL priority resources
    size_t evictIndex = 0;
    while (evictIndex < modelAccessOrder_.size() && modelAccessOrder_[evictIndex].priority == ResourcePriority::CRITICAL)
    {
      ++evictIndex;
    }

    if (evictIndex >= modelAccessOrder_.size())
    {
      // All resources are CRITICAL, cannot evict
      return;
    }

    // Evict resource at evictIndex
    const auto& toEvict = modelAccessOrder_[evictIndex];
    auto        it      = modelCache_.find(toEvict.key);
    if (it != modelCache_.end())
    {
      modelCache_.erase(it);
      cachedModelMemory_ -= toEvict.memorySize;
    }
    modelAccessOrder_.erase(modelAccessOrder_.begin() + evictIndex);
  }

  uint64_t ResourceManager::getCurrentTime() const
  {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
  }

  std::string ResourceManager::computeContentHash(const unsigned char* data, size_t dataSize) const
  {
    // Use FNV-1a hash for fast content-based deduplication
    uint64_t hash = hashBytes(data, dataSize);

    // Convert to hex string
    std::ostringstream oss;
    oss << std::hex << std::setfill('0') << std::setw(16) << hash;
    return oss.str();
  }

  // ============================================================================
  // ASYNC LOADING IMPLEMENTATION
  // ============================================================================

  void ResourceManager::initThreadPool(size_t numThreads)
  {
    workerThreads_.reserve(numThreads);
    for (size_t i = 0; i < numThreads; ++i)
    {
      workerThreads_.emplace_back(&ResourceManager::workerThreadLoop, this);
    }
  }

  void ResourceManager::shutdownThreadPool()
  {
    {
      std::lock_guard<std::mutex> lock(taskQueueMutex_);
      shutdownThreadPool_ = true;
    }
    taskQueueCV_.notify_all();

    for (auto& thread : workerThreads_)
    {
      if (thread.joinable())
      {
        thread.join();
      }
    }
    workerThreads_.clear();
  }

  void ResourceManager::workerThreadLoop()
  {
    while (true)
    {
      std::function<void()> task;

      {
        std::unique_lock<std::mutex> lock(taskQueueMutex_);
        taskQueueCV_.wait(lock, [this] { return shutdownThreadPool_ || !taskQueue_.empty(); });

        if (shutdownThreadPool_ && taskQueue_.empty())
        {
          return;
        }

        if (!taskQueue_.empty())
        {
          task = std::move(taskQueue_.front());
          taskQueue_.pop();
          activeTasks_++;
        }
      }

      if (task)
      {
        task();
        activeTasks_--;
      }
    }
  }

  std::future<std::shared_ptr<Texture>> ResourceManager::loadTextureAsync(const std::string& path, bool srgb, ResourcePriority priority)
  {
    // Check if already cached (fast path)
    std::string key = makeTextureKey(path, srgb);
    {
      std::lock_guard<std::mutex> lock(textureMutex_);
      auto                        it = textureCache_.find(key);
      if (it != textureCache_.end())
      {
        if (auto existingTexture = it->second.lock())
        {
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
      std::lock_guard<std::mutex> lock(taskQueueMutex_);
      taskQueue_.push([this, path, srgb, priority, promise]() {
        try
        {
          // Load texture synchronously on worker thread
          auto texture = loadTexture(path, srgb, priority);
          promise->set_value(texture);
        }
        catch (const std::exception& e)
        {
          promise->set_exception(std::current_exception());
        }
      });
    }
    taskQueueCV_.notify_one();

    return future;
  }

  std::future<std::shared_ptr<Model>>
  ResourceManager::loadModelAsync(const std::string& path, bool enableTextures, bool loadMaterials, bool enableMorphTargets, ResourcePriority priority)
  {
    // Check if already cached (fast path)
    std::string key = makeModelKey(path, enableTextures, loadMaterials, enableMorphTargets);
    {
      std::lock_guard<std::mutex> lock(modelMutex_);
      auto                        it = modelCache_.find(key);
      if (it != modelCache_.end())
      {
        if (auto existingModel = it->second.lock())
        {
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
      std::lock_guard<std::mutex> lock(taskQueueMutex_);
      taskQueue_.push([this, path, enableTextures, loadMaterials, enableMorphTargets, priority, promise]() {
        try
        {
          // Load model synchronously on worker thread
          auto model = loadModel(path, enableTextures, loadMaterials, enableMorphTargets, priority);
          promise->set_value(model);
        }
        catch (const std::exception& e)
        {
          promise->set_exception(std::current_exception());
        }
      });
    }
    taskQueueCV_.notify_one();

    return future;
  }

  size_t ResourceManager::getPendingAsyncLoads() const
  {
    std::lock_guard<std::mutex> lock(taskQueueMutex_);
    return taskQueue_.size() + activeTasks_;
  }

  void ResourceManager::waitForAsyncLoads()
  {
    while (getPendingAsyncLoads() > 0)
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }

} // namespace engine
