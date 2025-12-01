#pragma once

#include <condition_variable>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>

#include "Device.hpp"

namespace engine {

  // Forward declarations
  class Texture;
  class Model;

  /**
   * @brief Async loading status for tracking resource load progress
   */
  enum class LoadStatus
  {
    PENDING,  // Queued for loading
    LOADING,  // Currently loading
    COMPLETE, // Successfully loaded
    FAILED    // Load failed
  };

  /**
   * @brief Handle for tracking async resource loading
   */
  template <typename T> struct AsyncLoadHandle
  {
    std::future<std::shared_ptr<T>> future;
    LoadStatus                      status;
    std::string                     path;
    float                           progress; // 0.0 to 1.0
  };

  /**
   * @brief Resource priority for eviction policy
   * Higher priority resources are kept in cache longer
   */
  enum class ResourcePriority
  {
    LOW      = 0, // Evict first (distant objects, unused LODs)
    MEDIUM   = 1, // Standard priority (most resources)
    HIGH     = 2, // Evict last (nearby objects, current level)
    CRITICAL = 3  // Never evict (UI, player character, essential assets)
  };

  /**
   * @brief Centralized resource management with automatic deduplication and lifetime tracking
   *
   * Features:
   * - Automatic resource deduplication (same path loaded once)
   * - Memory tracking and budgeting
   * - Automatic cleanup of unused resources via weak_ptr
   * - Thread-safe resource access
   * - Prepared for future async loading support
   */
  class ResourceManager
  {
  public:
    explicit ResourceManager(Device& device);
    ~ResourceManager();

    // Delete copy and move operations (contains mutexes which are not movable)
    ResourceManager(const ResourceManager&)            = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;
    ResourceManager(ResourceManager&&)                 = delete;
    ResourceManager& operator=(ResourceManager&&)      = delete;

    /**
     * @brief Load a texture from file with automatic caching
     * @param path Absolute or relative path to texture file
     * @param srgb Whether to load as sRGB format (true for color textures, false for data)
     * @param priority Resource priority for eviction policy
     * @return Shared pointer to texture (returns cached instance if already loaded)
     */
    std::shared_ptr<Texture> loadTexture(const std::string& path, bool srgb = true, ResourcePriority priority = ResourcePriority::MEDIUM);

    /**
     * @brief Load a texture from memory with automatic caching (for embedded textures)
     * @param data Texture data in memory
     * @param dataSize Size of texture data in bytes
     * @param debugName Debug name for the texture (used for cache key)
     * @param srgb Whether to load as sRGB format
     * @param priority Resource priority for eviction policy
     * @return Shared pointer to texture (returns cached instance if same data already loaded)
     */
    std::shared_ptr<Texture> loadTextureFromMemory(const unsigned char* data,
                                                   size_t               dataSize,
                                                   const std::string&   debugName,
                                                   bool                 srgb     = true,
                                                   ResourcePriority     priority = ResourcePriority::MEDIUM);

    /**
     * @brief Load a model from file with automatic caching
     * @param path Absolute or relative path to model file
     * @param enableTextures Whether to load textures from MTL file
     * @param loadMaterials Whether to load materials from MTL file
     * @param enableMorphTargets Whether to enable morph target support
     * @param priority Resource priority for eviction policy
     * @return Shared pointer to model (returns cached instance if already loaded)
     */
    std::shared_ptr<Model> loadModel(const std::string& path,
                                     bool               enableTextures     = false,
                                     bool               loadMaterials      = true,
                                     bool               enableMorphTargets = false,
                                     ResourcePriority   priority           = ResourcePriority::MEDIUM);

    /**
     * @brief Remove unused resources from cache (those with no external references)
     * Call periodically (e.g., after scene transitions) to free memory
     * @return Number of resources removed
     */
    size_t garbageCollect();

    /**
     * @brief Get actual memory usage of all cached resources
     * @return Memory usage in bytes (actual, not estimated)
     */
    size_t getMemoryUsage() const;

    /**
     * @brief Get number of cached textures
     */
    size_t getCachedTextureCount() const;

    /**
     * @brief Get number of cached models
     */
    size_t getCachedModelCount() const;

    /**
     * @brief Set memory budget (resources evicted when exceeded)
     * @param budgetBytes Memory budget in bytes (0 = unlimited)
     */
    void setMemoryBudget(size_t budgetBytes);

    /**
     * @brief Get current memory budget
     * @return Memory budget in bytes (0 = unlimited)
     */
    size_t getMemoryBudget() const { return memoryBudget_; }

    /**
     * @brief Clear all cached resources immediately
     * Warning: This will invalidate all external shared_ptr references
     */
    void clearAll();

    /**
     * @brief Check if a texture is cached
     */
    bool isTextureCached(const std::string& path) const;

    /**
     * @brief Check if a model is cached
     */
    bool isModelCached(const std::string& path) const;

    // ========================================================================
    // ASYNC LOADING API
    // ========================================================================

    /**
     * @brief Load a texture asynchronously in background thread
     * @param path Absolute or relative path to texture file
     * @param srgb Whether to load as sRGB format
     * @param priority Resource priority for eviction policy
     * @return Future that resolves to texture when loading completes
     */
    std::future<std::shared_ptr<Texture>> loadTextureAsync(const std::string& path, bool srgb = true, ResourcePriority priority = ResourcePriority::MEDIUM);

    /**
     * @brief Load a model asynchronously in background thread
     * @param path Absolute or relative path to model file
     * @param enableTextures Whether to load textures from MTL file
     * @param loadMaterials Whether to load materials from MTL file
     * @param enableMorphTargets Whether to enable morph target support
     * @param priority Resource priority for eviction policy
     * @return Future that resolves to model when loading completes
     */
    std::future<std::shared_ptr<Model>> loadModelAsync(const std::string& path,
                                                       bool               enableTextures     = false,
                                                       bool               loadMaterials      = true,
                                                       bool               enableMorphTargets = false,
                                                       ResourcePriority   priority           = ResourcePriority::MEDIUM);

    /**
     * @brief Check if async loading is ready (non-blocking)
     * @return True if future is ready, false if still loading
     */
    template <typename T> static bool isReady(const std::future<std::shared_ptr<T>>& future)
    {
      return future.valid() && future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
    }

    /**
     * @brief Get number of pending async load tasks
     */
    size_t getPendingAsyncLoads() const;

    /**
     * @brief Wait for all pending async loads to complete
     */
    void waitForAsyncLoads();

  private:
    Device& device_;

    // Resource caches (weak_ptr allows automatic cleanup)
    mutable std::mutex                                      textureMutex_;
    std::unordered_map<std::string, std::weak_ptr<Texture>> textureCache_;

    mutable std::mutex                                    modelMutex_;
    std::unordered_map<std::string, std::weak_ptr<Model>> modelCache_;

    // LRU tracking for eviction policy
    struct ResourceInfo
    {
      std::string      key;
      size_t           memorySize;
      uint64_t         lastAccessTime;
      ResourcePriority priority;
    };
    std::vector<ResourceInfo> textureAccessOrder_;
    std::vector<ResourceInfo> modelAccessOrder_;

    // Content hash cache for embedded textures (hash -> cache key)
    std::unordered_map<std::string, std::string> contentHashToKey_;

    // Memory management
    size_t         memoryBudget_        = 0; // 0 = unlimited
    mutable size_t cachedTextureMemory_ = 0;
    mutable size_t cachedModelMemory_   = 0;

    // Helper to generate cache key from path and parameters
    std::string makeTextureKey(const std::string& path, bool srgb) const;
    std::string makeModelKey(const std::string& path, bool enableTextures, bool loadMaterials, bool enableMorphTargets) const;

    // Memory management helpers
    void        updateTextureAccess(const std::string& key, size_t memorySize, ResourcePriority priority);
    void        updateModelAccess(const std::string& key, size_t memorySize, ResourcePriority priority);
    void        evictLRUTextures();
    void        evictLRUModels();
    uint64_t    getCurrentTime() const;
    std::string computeContentHash(const unsigned char* data, size_t dataSize) const;

    // ========================================================================
    // ASYNC LOADING INFRASTRUCTURE
    // ========================================================================

    // Thread pool for async loading
    std::vector<std::thread>          workerThreads_;
    std::queue<std::function<void()>> taskQueue_;
    mutable std::mutex                taskQueueMutex_;
    std::condition_variable           taskQueueCV_;
    std::atomic<bool>                 shutdownThreadPool_{false};
    std::atomic<size_t>               activeTasks_{0};

    // Thread pool management
    void initThreadPool(size_t numThreads = 4);
    void shutdownThreadPool();
    void workerThreadLoop();

    // Async loading helpers
    template <typename T> void enqueueTask(std::function<void()> task);
  };

} // namespace engine
