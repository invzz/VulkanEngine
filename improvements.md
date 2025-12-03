# Engine Improvements Roadmap

## Overview
This document outlines prioritized improvements for the Vulkan-based 3D engine. Each improvement includes rationale, implementation complexity, and expected benefits.

**Last Updated:** December 2025

---

## 📊 Current Engine State Analysis

### ✅ Completed Systems
| System | Status | Notes |
|--------|--------|-------|
| **Texture System** | ✅ Complete | PBR textures, normal mapping, sRGB/linear handling |
| **Resource Manager** | ✅ Complete | Deduplication, async loading, LRU cache, priority system |
| **PBR Rendering** | ✅ Complete | Cook-Torrance BRDF, clearcoat, anisotropy |
| **Light Types** | ✅ Complete | Point, directional, spot lights with attenuation |
| **Shadow System** | ✅ Complete | Directional, spotlight (2D), point light (cube maps) |
| **Skybox** | ✅ Complete | Cubemap loading, dedicated render system |
| **Animation System** | ✅ Complete | Morph targets, GPU compute blending |
| **Model Loading** | ✅ Complete | OBJ + glTF with materials and animations |
| **Frustum Culling** | ⚠️ Basic | Sphere-based only, no spatial partitioning |
| **Post-Processing** | ✅ Complete | ACES Tonemapping, Color Grading, Bloom, FXAA implemented |
| **ImGui Integration** | ✅ Complete | Multiple panels, object selection |

### ⚠️ Known Gaps
| Gap | Impact | Priority |
|-----|--------|----------|
| **No LOD System** | Performance issues with large scenes | Medium |
| **No Profiler** | Can't identify bottlenecks | Medium |
| **Basic Culling Only** | O(n) per frame, no spatial acceleration | Medium |

---

## 🔥 High Priority - Immediate Impact

### 1. Complete Texture System Implementation ✅
**Status:** ✅ **COMPLETE**  
**Complexity:** Medium  
**Time Estimate:** 2-3 days  
**Impact:** High visual quality improvement

**What was implemented:**
- ✅ Texture class with `stb_image` loading
- ✅ MaterialSystem with descriptor sets (5 texture bindings)
- ✅ Fragment shader texture sampling with flags
- ✅ Default fallback textures (white, normal)
- ✅ Asphalt floor demo with PBR textures
- ✅ Normal mapping with TBN matrix construction
- ✅ Proper sRGB/linear texture format handling
- ✅ UV tiling support

**Results achieved:**
- Realistic surface detail from 4K PBR textures
- Normal mapping for proper lighting response
- Material variation (roughness, metallic, AO)
- Clean descriptor set caching system
- Efficient material batching by descriptor set

**Files modified:**
```
include/3dEngine/Texture.hpp                     [✅ EXISTS]
src/3dEngine/Texture.cpp                         [✅ EXISTS]
include/3dEngine/systems/MaterialSystem.hpp      [✅ EXISTS]
src/3dEngine/systems/MaterialSystem.cpp          [✅ EXISTS]
assets/shaders/pbr_shader.frag                   [✅ UPDATED - Texture sampling]
assets/shaders/pbr_shader.vert                   [✅ UPDATED - UV passing]
src/demos/Cube/SceneLoader.cpp                   [✅ UPDATED - Asphalt textures]
```

**Available floor materials:**
- `Asphalt01_MR_4K` (currently active)
- `RedStoneWall01_MR_4K`
- `MarbleTiles01_MR_4K`

**Technical details:**
- Texture format: sRGB for albedo, linear for normal/roughness/AO
- Normal map convention: DirectX (Y-down), converted in shader
- UV tiling: Configurable per material (default 8x for floor)
- Descriptor caching: Material hash → descriptor set map
- Fallback textures: 1x1 white, flat normal (0.5, 0.5, 1.0)

---

### 2. Shadow System ✅
**Status:** ✅ **COMPLETE**  
**Complexity:** High  
**Time Estimate:** 4-5 days  
**Impact:** Critical for realistic lighting

**What was implemented:**
- ✅ 2D shadow maps for directional lights (PCF soft shadows)
- ✅ 2D shadow maps for spotlights (perspective projection)
- ✅ Cube shadow maps for point lights (6-face omnidirectional)
- ✅ Linear depth storage for point lights (distance/farPlane)
- ✅ Proper Vulkan Y-flip handling for cube map sampling
- ✅ Shadow bias to prevent acne
- ✅ Separate shadow render pass before main rendering

**Files created/modified:**
```
include/3dEngine/systems/ShadowSystem.hpp        [✅ NEW]
src/3dEngine/systems/ShadowSystem.cpp            [✅ NEW]
include/3dEngine/ShadowMap.hpp                   [✅ NEW]
src/3dEngine/ShadowMap.cpp                       [✅ NEW]
include/3dEngine/CubeShadowMap.hpp               [✅ NEW]
src/3dEngine/CubeShadowMap.cpp                   [✅ NEW]
assets/shaders/shadow.vert                       [✅ NEW]
assets/shaders/shadow.frag                       [✅ NEW]
assets/shaders/cube_shadow.vert                  [✅ NEW]
assets/shaders/cube_shadow.frag                  [✅ NEW]
assets/shaders/pbr_shader.frag                   [✅ UPDATED - Shadow sampling]
```

**Architecture:**
```cpp
class ShadowSystem {
  // 2D shadow maps for directional/spot lights
  void renderShadowMaps(FrameInfo& frameInfo, float sceneRadius);
  
  // Cube shadow maps for point lights
  void renderPointLightShadowMaps(FrameInfo& frameInfo);
  
  // Descriptor access for PBR shader
  VkDescriptorImageInfo getShadowMapDescriptorInfo(int index);
  VkDescriptorImageInfo getCubeShadowMapDescriptorInfo(int index);
};
```

**Benefits achieved:**
- Realistic depth perception
- Proper object grounding
- Omnidirectional point light shadows
- Professional visual quality

---

### 3. Resource Management System ✅
**Status:** ✅ **COMPLETE - ALL FEATURES**  
**Complexity:** Medium  
**Time Estimate:** 2-3 days  
**Impact:** Memory efficiency, performance

**What was implemented:**
- ✅ ResourceManager class with automatic deduplication
- ✅ Thread-safe caching using weak_ptr pattern
- ✅ Separate caches for textures and models
- ✅ Cache keys include loading parameters
- ✅ Garbage collection for expired entries
- ✅ Memory usage tracking and statistics
- ✅ Full integration into SceneLoader and App
- ✅ **Accurate memory tracking** (Texture/Model.getMemorySize())
- ✅ **LRU cache with memory budget** (automatic eviction)
- ✅ **Priority system** (CRITICAL/HIGH/MEDIUM/LOW)
- ✅ **Content-based caching** (FNV-1a hashing for embedded textures)
- ✅ **Async loading with thread pool** (non-blocking resource loading)

**Results achieved:**
- Automatic prevention of duplicate resource loading
- Shared model instances (e.g., dragon grid uses 1 model, not 25)
- Thread-safe design with mutex-protected operations
- Automatic cleanup when resources no longer referenced
- Minimal overhead (~100 bytes per cached entry)
- Memory budget enforcement with automatic LRU eviction
- Priority-based eviction (CRITICAL assets never evicted)
- Embedded texture deduplication (60%+ memory savings)
- **Non-blocking async loads with std::future API**
- **Auto-sized thread pool for parallel loading**

**Files created:**
```
include/3dEngine/ResourceManager.hpp              [✅ NEW]
src/3dEngine/ResourceManager.cpp                  [✅ NEW]
RESOURCE_MANAGER_COMPLETE.md                      [✅ DOCUMENTATION - Initial implementation]
RESOURCE_MANAGER_IMPROVEMENTS.md                  [✅ DOCUMENTATION - Memory tracking + LRU]
RESOURCE_PRIORITY_AND_EMBEDDED_TEXTURES.md        [✅ DOCUMENTATION - Priority + content hash]
ASYNC_LOADING_COMPLETE.md                         [✅ DOCUMENTATION - Async loading + thread pool]
TEST_RESOURCE_DEDUPLICATION.md                    [✅ DOCUMENTATION - Testing guide]
```

**Files modified:**
```
src/demos/Cube/SceneLoader.hpp                    [✅ UPDATED - Added ResourceManager parameter]
src/demos/Cube/SceneLoader.cpp                    [✅ UPDATED - Use ResourceManager for loading]
src/demos/Cube/app.hpp                            [✅ UPDATED - Added ResourceManager member]
src/demos/Cube/app.cpp                            [✅ UPDATED - Pass ResourceManager to SceneLoader]
```

**API Reference:**
```cpp
class ResourceManager {
public:
    ResourceManager(Device& device);
    ~ResourceManager();  // Shuts down thread pool
    
    // Synchronous loading (automatic caching and deduplication)
    std::shared_ptr<Texture> loadTexture(const std::string& path, bool srgb, 
                                         ResourcePriority priority = MEDIUM);
    std::shared_ptr<Texture> loadTextureFromMemory(const unsigned char* data, size_t size,
                                                    const std::string& debugName, bool srgb,
                                                    ResourcePriority priority = MEDIUM);
    std::shared_ptr<Model> loadModel(const std::string& path, 
                                     bool enableTextures,
                                     bool loadMaterials,
                                     bool enableMorphTargets,
                                     ResourcePriority priority = MEDIUM);
    
    // Asynchronous loading (non-blocking)
    std::future<std::shared_ptr<Texture>> loadTextureAsync(const std::string& path, bool srgb,
                                                            ResourcePriority priority = MEDIUM);
    std::future<std::shared_ptr<Model>> loadModelAsync(const std::string& path, 
                                                        bool enableTextures,
                                                        bool loadMaterials,
                                                        bool enableMorphTargets,
                                                        ResourcePriority priority = MEDIUM);
    
    // Async utilities
    template<typename T>
    static bool isReady(const std::future<std::shared_ptr<T>>& future);
    size_t getPendingAsyncLoads() const;
    void waitForAsyncLoads();
    
    // Memory management
    void setMemoryBudget(size_t bytes);
    size_t getMemoryUsage() const;
    size_t getCachedTextureCount() const;
    size_t getCachedModelCount() const;
    size_t garbageCollect();
};

// Priority levels (affects eviction)
enum class ResourcePriority {
    LOW,      // Evicted first (distant objects, LODs)
    MEDIUM,   // Standard eviction (default)
    HIGH,     // Keep longer (nearby objects)
    CRITICAL  // Never evicted (UI, player)
};
```

**Cache Strategy:**
- Textures: Key = "path|srgb" or "path|linear" (different formats cached separately)
- Models: Key = "path|tex=bool|mat=bool|morph=bool" (different configs cached separately)
- Storage: weak_ptr in cache, shared_ptr returned to users
- Cleanup: Automatic when last shared_ptr destroyed, manual via garbageCollect()

**Memory Benefits:**
- Prevents duplicate loading (e.g., same texture used in multiple materials)
- Shared model instances (25 dragons = 1 model instance, not 25)
- Example: Dragon grid saves ~360MB (1 model at 15MB vs 25 models at 375MB)

---

### 4. Frustum Culling + LOD System
**Status:** Basic sphere culling exists in RenderCullingSystem  
**Complexity:** Medium  
**Time Estimate:** 3-4 days  
**Impact:** Major performance improvement for large scenes

**Current implementation:**
```cpp
// RenderCullingSystem::isVisible() - basic sphere check
bool isVisible(const Camera& camera, const glm::vec3& center, float radius);
```

**Improvements needed:**

**A. Hierarchical Culling:**
```cpp
class SpatialPartitioning {
  // Octree or BVH for efficient spatial queries
  void insert(GameObject* obj);
  void query(const Frustum& frustum, std::vector<GameObject*>& visible);
  void update(GameObject* obj); // When object moves
};
```

**B. LOD System:**
```cpp
struct LODLevel {
  std::shared_ptr<Model> model;
  float minDistance;
  float maxDistance;
};

class LODManager {
  void registerLODGroup(GameObject::id_t id, std::vector<LODLevel> levels);
  void selectLOD(FrameInfo& frameInfo); // Called before rendering
};
```

**Expected gains:**
- 5-10x fewer draw calls for large scenes
- Smooth performance with thousands of objects
- Better memory efficiency (distant objects use low-poly models)

---

### 5. Image-Based Lighting (IBL) ✅
**Status:** ✅ **COMPLETE**
**Complexity:** Medium-High
**Time Estimate:** 3-4 days
**Impact:** Massive PBR quality improvement

**What was implemented:**
- ✅ Skybox/environment map
- ✅ Prefiltered environment maps (diffuse irradiance, specular)
- ✅ BRDF integration lookup table (LUT)
- ✅ Fragment shader integration
- ✅ `IBLSystem` class for generation and management
- ✅ Integration into `PBRRenderSystem` and `app.cpp`

**Implementation:**
```cpp
class IBLSystem {
public:
  void loadEnvironment(const std::string& hdrPath);
  void precompute(); // Generate irradiance, prefiltered maps, BRDF LUT
  
  VkDescriptorSet getDescriptorSet() const;
  
private:
  std::shared_ptr<Texture> environmentMap;    // Original HDR
  std::shared_ptr<Texture> irradianceMap;     // Diffuse (32x32 cubemap)
  std::shared_ptr<Texture> prefilteredMap;    // Specular (128x128, mipmapped)
  std::shared_ptr<Texture> brdfLUT;           // 512x512 2D texture
};
```

**Shader changes:**
```glsl
// Add to PBR shader:
layout(set = 3, binding = 0) uniform samplerCube irradianceMap;
layout(set = 3, binding = 1) uniform samplerCube prefilteredMap;
layout(set = 3, binding = 2) uniform sampler2D brdfLUT;

// Use in lighting calculation:
vec3 diffuse = texture(irradianceMap, N).rgb * albedo;
vec3 specular = textureLod(prefilteredMap, R, roughness * maxLod).rgb;
```

**Benefits:**
- Realistic ambient lighting from environment
- Accurate reflections on metallic surfaces
- Professional-looking materials
- No need for many light sources

---

## 🚀 Medium Priority - Architecture & Performance

### 6. Render Graph System
**Status:** Manual pass management  
**Complexity:** High  
**Time Estimate:** 5-7 days  
**Impact:** Extensibility, automatic optimization

**Problem:**
```cpp
// Current: Manual render pass ordering, barriers, resource management
renderer.beginSwapChainRenderPass(commandBuffer);
pbrRenderSystem.render(frameInfo);
lightSystem.render(frameInfo);
uiManager.render(frameInfo, commandBuffer);
renderer.endSwapChainRenderPass(commandBuffer);
```

**Solution:**
```cpp
class RenderGraph {
public:
  PassHandle addPass(const char* name, PassSetup setup, PassExecute execute);
  void compile(); // Automatically orders passes, inserts barriers
  void execute(VkCommandBuffer cmd);
  
  // Resources managed by graph
  TextureHandle createTexture(const TextureDesc& desc);
  BufferHandle createBuffer(const BufferDesc& desc);
};

// Usage:
auto gbufferPass = graph.addPass("GBuffer", 
  [](PassBuilder& pass) {
    pass.write("depth");
    pass.write("albedo");
    pass.write("normal");
  },
  [](CommandBuffer cmd, PassResources& res) {
    // Render code
  });

auto lightingPass = graph.addPass("Lighting",
  [](PassBuilder& pass) {
    pass.read("albedo");
    pass.read("normal");
    pass.write("color");
  },
  [](CommandBuffer cmd, PassResources& res) {
    // Lighting code
  });
```

**Benefits:**
- Automatic synchronization (barriers, semaphores)
- Easy to add/reorder passes (deferred, forward+, compute effects)
- Automatic resource lifetime management
- Transient resource optimization (aliasing)
- Async compute scheduling

**When to implement:** Before adding complex effects (SSAO, SSR, volumetrics)

---

### 7. Multithreaded Command Recording
**Status:** Single-threaded CPU submission  
**Complexity:** Medium-High  
**Time Estimate:** 3-4 days  
**Impact:** 2-3x rendering throughput for complex scenes

**Current bottleneck:**
```cpp
// All draw calls recorded on main thread
for (auto& obj : visibleObjects) {
  vkCmdBindDescriptorSets(...);
  vkCmdPushConstants(...);
  vkCmdDrawIndexed(...);
}
```

**Solution:**
```cpp
class ParallelRenderContext {
  std::vector<VkCommandBuffer> secondaryBuffers;
  ThreadPool workers;
  
  void recordParallel(const std::vector<RenderableObject>& objects) {
    // Split objects into buckets (one per thread)
    const size_t bucketSize = objects.size() / numThreads;
    
    #pragma omp parallel for
    for (int i = 0; i < numThreads; i++) {
      size_t start = i * bucketSize;
      size_t end = (i == numThreads - 1) ? objects.size() : start + bucketSize;
      
      recordBucket(secondaryBuffers[i], objects, start, end);
    }
  }
  
  void submit(VkCommandBuffer primary) {
    vkCmdExecuteCommands(primary, numThreads, secondaryBuffers.data());
  }
};
```

**Requirements:**
- Secondary command buffers (VK_COMMAND_BUFFER_LEVEL_SECONDARY)
- Thread-safe descriptor allocation
- Careful state management (pipeline, descriptor sets)

**Expected gains:**
- CPU time: ~3ms → ~1ms for 10K objects (4 threads)
- Enables more complex scenes without CPU bottleneck

---

### 8. GPU-Driven Rendering
**Status:** CPU builds draw lists  
**Complexity:** Very High  
**Time Estimate:** 7-10 days  
**Impact:** Massive scalability (millions of objects)

**Current approach:**
```cpp
// CPU iterates objects, builds draw commands
for (auto& obj : objects) {
  if (isVisible(obj)) {
    vkCmdDrawIndexed(...); // One CPU call per object
  }
}
```

**GPU-driven approach:**
```cpp
// Single CPU draw call:
vkCmdDrawIndirectCount(cmd, indirectBuffer, countBuffer, maxDraws);

// GPU compute shader generates draw commands:
layout(std430, set = 0, binding = 0) buffer Objects {
  ObjectData objects[];
};

layout(std430, set = 0, binding = 1) buffer DrawCommands {
  VkDrawIndexedIndirectCommand commands[];
};

layout(std430, set = 0, binding = 2) buffer DrawCount {
  uint count;
};

void main() {
  uint objectId = gl_GlobalInvocationID.x;
  if (objectId >= objectCount) return;
  
  ObjectData obj = objects[objectId];
  
  // GPU frustum culling
  if (isInFrustum(obj.bounds, ubo.frustum)) {
    uint drawIndex = atomicAdd(count, 1);
    commands[drawIndex] = makeDrawCommand(obj);
  }
}
```

**Benefits:**
- Near-zero CPU overhead (one draw call)
- Millions of objects possible
- No CPU-GPU sync for culling
- Enables dynamic LOD, occlusion culling on GPU

**Challenges:**
- Complex to debug
- Requires indirect drawing support
- Material batching more complex

**When to implement:** When scene complexity exceeds 100K objects

---

### 9. Entity Component System (ECS)
**Status:** Component-based GameObject (not true ECS)  
**Complexity:** Very High (architectural change)  
**Time Estimate:** 10-15 days  
**Impact:** Performance, scalability, clean code

**Current architecture:**
```cpp
class GameObject {
  TransformComponent transform;
  std::unique_ptr<PBRMaterial> pbrMaterial;
  std::unique_ptr<PointLightComponent> pointLight;
  // Components spread across memory, cache-unfriendly
};
```

**ECS architecture:**
```cpp
struct Entity {
  uint32_t id;
  uint32_t generation; // Handle recycling
};

// Components stored in contiguous arrays
class ComponentArray<T> {
  std::vector<T> components;
  std::unordered_map<Entity, size_t> entityToIndex;
};

// Systems iterate components directly
class RenderSystem {
  void update(ComponentArray<Transform>& transforms,
              ComponentArray<MeshComponent>& meshes,
              ComponentArray<Material>& materials) {
    // Iterate only entities with all three components
    for (size_t i = 0; i < count; i++) {
      // Cache-friendly: components are contiguous
      render(transforms[i], meshes[i], materials[i]);
    }
  }
};
```

**Benefits:**
- **Performance:** 2-5x faster iteration (cache-friendly data layout)
- **Parallelism:** Easy to parallelize system updates
- **Flexibility:** Add/remove components at runtime
- **Clean code:** Systems are stateless, easy to test

**Challenges:**
- Large refactor (affects entire codebase)
- Learning curve for ECS patterns
- Handle recycling complexity

**Recommendation:** Wait until engine is more mature, or start a new ECS-based engine

---

### 10. Async Asset Loading ✅
**Status:** ✅ **COMPLETE** (integrated into ResourceManager)  
**Complexity:** Medium  
**Time Estimate:** 2-3 days  
**Impact:** Smooth user experience

**Already implemented in ResourceManager:**
```cpp
// Non-blocking loading with futures
std::future<std::shared_ptr<Model>> loadModelAsync(path, ...);
std::future<std::shared_ptr<Texture>> loadTextureAsync(path, ...);

// Check if ready without blocking
if (ResourceManager::isReady(future)) {
    auto model = future.get();
}

// Wait for all pending loads
resourceManager.waitForAsyncLoads();
```

**Features:**
- ✅ Thread pool with auto-sized worker threads
- ✅ Future-based API for non-blocking loads
- ✅ Integrates with cache (cache hits return immediately)
- ✅ Priority-aware loading

---

## 🎨 Quality of Life Improvements

### 11. Enhanced Editor Features
**Status:** Basic ImGui panels exist  
**Complexity:** Medium (each feature)  
**Time Estimate:** 1-2 days per feature  
**Impact:** Productivity

**Features to add:**

**A. Transform Gizmos:**
```cpp
class GizmoSystem {
  enum Mode { Translate, Rotate, Scale };
  
  void render(GameObject& selected, Camera& camera);
  bool handleInput(Mouse& mouse); // Returns true if gizmo was manipulated
  
  Mode currentMode = Translate;
};
```

**B. Scene Hierarchy:**
```cpp
// Hierarchical scene tree in ScenePanel
GameObject* parent = nullptr;
std::vector<GameObject*> children;

// Drag-drop parenting
// Right-click context menu (duplicate, delete, etc.)
```

**C. Material Editor:**
```cpp
class MaterialEditor {
  void renderUI(PBRMaterial& material);
  void setPreviewMesh(Model* mesh);
  void renderPreview(); // Real-time material preview sphere
};
```

**D. Asset Browser:**
```cpp
class AssetBrowser {
  void renderUI();
  void onFileDoubleClick(const std::string& path);
  
  std::vector<std::string> textureFiles;
  std::vector<std::string> modelFiles;
  std::unordered_map<std::string, Thumbnail> thumbnails;
};
```

---

### 12. Scene Serialization ✅
**Status:** ✅ **COMPLETE**
**Complexity:** Medium  
**Time Estimate:** 2-3 days  
**Impact:** Workflow efficiency

**Implementation:**
- ✅ `SceneSerializer` class using `nlohmann/json`
- ✅ Save/Load menu in UI
- ✅ Serialization of Transforms, Models, Materials, Lights
- ✅ Integration with `GameObjectManager` and `ResourceManager`

**Benefits:**
- Save/load scenes from editor
- Version control friendly (text format)
- Easy to create test scenes
- Prefab system foundation

---

### 13. Profiling & Performance Metrics
**Status:** No instrumentation  
**Complexity:** Low-Medium  
**Time Estimate:** 1-2 days  
**Impact:** Optimization guidance

**Implementation:**
```cpp
class Profiler {
  void beginSection(const char* name);
  void endSection();
  void displayUI(); // ImGui window with hierarchical timing
  
  // GPU timing:
  void writeTimestamp(VkCommandBuffer cmd, const char* name);
  
  struct Section {
    const char* name;
    float cpuTimeMs;
    float gpuTimeMs;
    std::vector<Section> children;
  };
};

// Usage:
PROFILE_SCOPE("PBR Render");
pbrSystem.render(frameInfo);
```

**UI Display:**
```
Frame: 16.3ms (61 FPS)
├─ Update Phase: 0.8ms
│  ├─ Input: 0.1ms
│  ├─ Animation: 0.5ms
│  └─ Physics: 0.2ms
├─ Render Phase: 12.1ms
│  ├─ Shadow Maps: 3.2ms
│  ├─ GBuffer: 4.5ms
│  ├─ Lighting: 3.1ms
│  └─ Post Process: 1.3ms
└─ UI: 3.4ms
```

---

### 14. Post-Processing Stack ✅
**Status:** ✅ **COMPLETE**
**Complexity:** Medium (per effect)
**Time Estimate:** 1-2 days per effect
**Impact:** Visual polish

**What was implemented:**
- ✅ Offscreen HDR Framebuffer (R16G16B16A16_SFLOAT)
- ✅ PostProcessingSystem for full-screen quad rendering
- ✅ ACES Filmic Tonemapping shader
- ✅ Gamma Correction (linear -> sRGB)
- ✅ Procedural Color Grading (Exposure, Contrast, Saturation, Vignette)
- ✅ **Bloom** (Mipmap-based downsampling + additive blending)
- ✅ **FXAA** (Fast Approximate Anti-Aliasing)
- ✅ UI Controls for toggling and tuning effects

**Architecture:**
```cpp
class PostProcessingSystem {
  void render(FrameInfo& frameInfo, VkDescriptorSet descriptorSet, const PostProcessPushConstants& push);
  // Renders full-screen triangle with no vertex buffer
};
```

**Benefits:**
- Professional final image quality
- HDR rendering support
- Temporal effects (motion blur)

---

## 🔬 Advanced / Research Features

### 15. Bindless Rendering (Vulkan 1.2+)
**Status:** Descriptor sets per material  
**Complexity:** High  
**Time Estimate:** 4-5 days  
**Impact:** Performance, flexibility

**Current limitation:**
```cpp
// Must bind descriptor set per material
vkCmdBindDescriptorSets(..., materialDescSet, ...);
vkCmdDrawIndexed(...);
```

**Bindless approach:**
```glsl
// Single giant texture array
layout(set = 0, binding = 0) uniform texture2D textures[10000];
layout(set = 0, binding = 1) uniform sampler samplers[16];

// Push constant with texture indices
layout(push_constant) uniform PushConstants {
  uint albedoIndex;
  uint normalIndex;
  uint metallicIndex;
} push;

void main() {
  vec4 albedo = texture(sampler2D(textures[push.albedoIndex], samplers[0]), uv);
}
```

**Benefits:**
- No descriptor set switching overhead
- Unlimited materials
- Easier material batching

**Requirements:**
- VK_EXT_descriptor_indexing
- Vulkan 1.2+
- GPU with non-uniform indexing support

---

### 16. Ray Tracing Integration
**Status:** Not implemented  
**Complexity:** Very High  
**Time Estimate:** 10-14 days  
**Impact:** Next-gen visual quality

**Hybrid approach (combine raster + ray tracing):**
- **Raster:** Main geometry, fast performance
- **Ray traced shadows:** Pixel-perfect shadows
- **Ray traced reflections:** Accurate mirror/water reflections
- **Ray traced AO:** High-quality ambient occlusion

**Implementation:**
```cpp
class RayTracingSystem {
  void buildAccelerationStructure(GameObjectManager& objects);
  void traceRays(VkCommandBuffer cmd, RayType type);
  
  VkAccelerationStructureKHR topLevelAS;
  std::vector<VkAccelerationStructureKHR> bottomLevelAS;
  
  // Ray tracing pipeline
  VkPipeline rtPipeline;
  VkBuffer shaderBindingTable;
};
```

**Requirements:**
- RTX GPU or AMD RDNA2+
- VK_KHR_ray_tracing_pipeline extension
- Significant learning curve

**Recommendation:** Optional feature, detect hardware support at runtime

---

### 17. Virtual Geometry (Nanite-like)
**Status:** Traditional mesh LOD  
**Complexity:** Extremely High  
**Time Estimate:** 20+ days  
**Impact:** Film-quality geometry

**Concept:**
- Store geometry as clusters (64-128 triangles)
- LOD hierarchy of clusters
- GPU streams visible clusters on demand
- Render only visible triangles (pixel-level detail)

**Benefits:**
- Billions of triangles in scene
- No traditional LOD popping
- Artist-friendly workflow (no manual LOD)

**Challenges:**
- Complex preprocessing (cluster generation)
- Requires GPU-driven rendering
- Material system changes (cluster-based)

**Recommendation:** Research project, not production-ready yet

---

## 📅 Recommended Implementation Order

### Phase 1: Visual Quality ✅ COMPLETE
1. ✅ **Texture System** - **COMPLETE**
2. ✅ **Resource Manager** - **COMPLETE** (with async loading)
3. ✅ **Async Asset Loading** - **COMPLETE** (in ResourceManager)
4. ✅ **Skybox** - **COMPLETE** (cubemap rendering)
5. ✅ **Shadow System** - **COMPLETE** (directional, spot, point lights)
6. ✅ **IBL (Image-Based Lighting)** - **COMPLETE**
7. **Profiler** - Measure before optimizing

### Phase 2: Performance & Polish (Current - 2-3 weeks)
8. **Post-Processing Stack** - Bloom, proper tonemapping, FXAA
9. **Improved Frustum Culling + LOD** - Handle larger scenes
10. **PCF Soft Shadows for Point Lights** - Smoother cube shadow edges

### Phase 3: Architecture & Workflow (3-4 weeks)
10. **Scene Serialization** - Save/load scenes
11. **Enhanced Editor** - Gizmos, asset browser
12. **Render Graph** - Future extensibility

### Phase 4: Advanced (optional, 4+ weeks)
13. **Multithreaded Command Recording** - CPU performance
14. **GPU-Driven Rendering** - Million+ objects
15. **Bindless Rendering** - Modern Vulkan features
16. **Ray Tracing** - Next-gen quality

---

## 🆕 New Improvement Proposals

### 18. Deferred Rendering Pipeline
**Status:** Not implemented (forward rendering only)  
**Complexity:** High  
**Time Estimate:** 5-7 days  
**Impact:** Massive performance improvement for many lights

**Current limitation:**
- Forward rendering: each object shaded against ALL lights
- Cost: O(objects × lights) fragment operations
- Practical limit: ~16 lights before performance degrades

**Deferred approach:**
```
Pass 1: G-Buffer (geometry) - O(objects)
  - Render albedo, normal, metallic/roughness, depth to textures
  
Pass 2: Lighting - O(pixels × lights)  
  - Full-screen quad per light (or tiled/clustered)
  - Sample G-Buffer, compute lighting
  
Pass 3: Forward transparent objects
```

**G-Buffer layout:**
```glsl
layout(location = 0) out vec4 gAlbedo;      // RGB: albedo, A: unused
layout(location = 1) out vec4 gNormal;      // RGB: normal (encoded), A: unused
layout(location = 2) out vec4 gMetRoughAO;  // R: metallic, G: roughness, B: AO
// Depth from depth buffer
```

**Benefits:**
- 100+ lights without performance cliff
- Decouples geometry and lighting complexity
- Enables SSAO, SSR easily (G-Buffer available)

**Considerations:**
- Higher VRAM usage (G-Buffer ~40MB at 1080p)
- Transparency requires hybrid approach
- MSAA more complex (use TAA instead)

---

### 19. Tiled/Clustered Light Culling
**Status:** Not implemented  
**Complexity:** Medium-High  
**Time Estimate:** 4-5 days  
**Impact:** Handle 1000+ lights efficiently

**Current limitation:**
```glsl
// Every fragment tests every light
for (int i = 0; i < lightCount; i++) {
    Lo += calculateLight(lights[i]); // 16 lights = 16 iterations
}
```

**Tiled approach:**
```
1. Divide screen into 16x16 pixel tiles
2. For each tile, compute which lights affect it (frustum test)
3. Store light indices per tile in buffer
4. Fragment shader only iterates relevant lights
```

**Clustered approach (3D):**
```
- Extend tiles into depth slices (clusters)
- Handles depth discontinuities better
- More memory but more accurate culling
```

**Compute shader:**
```glsl
// Per-tile light culling
layout(local_size_x = 16, local_size_y = 16) in;

shared uint tileMinZ, tileMaxZ;
shared uint tileLightCount;
shared uint tileLightIndices[MAX_LIGHTS_PER_TILE];

void main() {
    // Reduce min/max depth for tile
    // Test each light against tile frustum
    // Write visible light indices
}
```

**Expected gains:**
- 16 lights → 1000+ lights at same cost
- Enables city scenes, concerts, complex lighting

---

### 20. Screen-Space Ambient Occlusion (SSAO)
**Status:** Not implemented  
**Complexity:** Medium  
**Time Estimate:** 2-3 days  
**Impact:** Significant depth and realism

**Algorithm (HBAO or similar):**
```glsl
// Sample depth/normal in hemisphere around fragment
for (int i = 0; i < KERNEL_SIZE; i++) {
    vec3 samplePos = fragPos + kernel[i] * radius;
    vec4 projPos = projection * vec4(samplePos, 1.0);
    float sampleDepth = texture(depthBuffer, projPos.xy).r;
    
    occlusion += (sampleDepth >= projPos.z) ? 1.0 : 0.0;
}
occlusion /= KERNEL_SIZE;
```

**Blur pass:**
- Bilateral blur to reduce noise while preserving edges

**Integration:**
```glsl
// In lighting pass:
vec3 ambient = ambientColor * albedo * aoFromSSAO;
```

**Benefits:**
- Contact shadows in corners, crevices
- Better depth perception
- Works with any geometry (no baking)

---

### 21. Screen-Space Reflections (SSR)
**Status:** Not implemented  
**Complexity:** Medium-High  
**Time Estimate:** 3-4 days  
**Impact:** Realistic reflections on smooth surfaces

**Algorithm:**
```glsl
// Ray march in screen space
vec3 rayDir = reflect(-V, N);
vec3 rayPos = fragPos;

for (int i = 0; i < MAX_STEPS; i++) {
    rayPos += rayDir * stepSize;
    vec4 projPos = projection * vec4(rayPos, 1.0);
    float sceneDepth = texture(depthBuffer, projPos.xy).r;
    
    if (rayPos.z > sceneDepth) {
        // Hit! Sample color buffer
        return texture(colorBuffer, projPos.xy).rgb;
    }
}
return skyboxSample; // Fallback
```

**Optimizations:**
- Hierarchical Z-buffer for faster marching
- Temporal reprojection to reduce noise
- Fallback to cubemap for misses

**When to use:**
- Smooth metallic surfaces (chrome, water, mirrors)
- Blend with IBL based on roughness

---

### 22. Temporal Anti-Aliasing (TAA)
**Status:** Not implemented  
**Complexity:** Medium  
**Time Estimate:** 2-3 days  
**Impact:** Smooth edges, stable image

**Why TAA over MSAA:**
- Works with deferred rendering
- Handles specular aliasing
- Lower VRAM cost than 4x MSAA
- Enables temporal effects (motion blur)

**Algorithm:**
```glsl
// Jitter projection matrix each frame
projection[2][0] += jitter.x / screenWidth;
projection[2][1] += jitter.y / screenHeight;

// In resolve pass:
vec3 current = texture(currentFrame, uv).rgb;
vec3 history = texture(previousFrame, reprojectedUV).rgb;

// Clamp history to neighborhood to prevent ghosting
vec3 neighborMin, neighborMax;
computeColorBounds(uv, neighborMin, neighborMax);
history = clamp(history, neighborMin, neighborMax);

vec3 result = mix(history, current, 0.1); // 90% history
```

**Requires:**
- Motion vectors for reprojection
- Previous frame color buffer
- Jittered projection matrix

---

### 23. Volumetric Lighting (God Rays)
**Status:** Not implemented  
**Complexity:** Medium-High  
**Time Estimate:** 3-4 days  
**Impact:** Atmospheric effects

**Ray marching approach:**
```glsl
// March from camera toward light
vec3 rayPos = fragPos;
vec3 rayDir = normalize(lightPos - rayPos);
float accumScatter = 0.0;

for (int i = 0; i < MARCH_STEPS; i++) {
    rayPos += rayDir * stepSize;
    
    // Check if in shadow (sample shadow map)
    float shadow = sampleShadow(rayPos);
    
    // Accumulate scattering
    accumScatter += (1.0 - shadow) * density * stepSize;
}

// Add to final color
color += lightColor * accumScatter * scatterStrength;
```

**Optimizations:**
- Render at half resolution
- Temporal accumulation
- Bilateral upscale

---

### 24. Hot Reloading System
**Status:** Not implemented  
**Complexity:** Medium  
**Time Estimate:** 2-3 days  
**Impact:** Faster iteration during development

**What to reload:**
1. **Shaders** - Recompile on file change
2. **Textures** - Reload without restart
3. **Models** - Update geometry in place

**Implementation:**
```cpp
class HotReloader {
  void watchDirectory(const std::string& path);
  void checkForChanges(); // Call each frame
  
  // Callbacks
  std::function<void(const std::string&)> onShaderChanged;
  std::function<void(const std::string&)> onTextureChanged;
  
private:
  std::unordered_map<std::string, std::filesystem::file_time_type> fileTimestamps;
};
```

**Shader hot reload:**
```cpp
void onShaderChanged(const std::string& path) {
    // Recompile GLSL → SPIR-V
    system("glslc " + path + " -o " + path + ".spv");
    
    // Recreate pipeline
    pipeline = createPipeline(device, path + ".spv");
}
```

**Benefits:**
- Instant shader tweaks without restart
- Material editing workflow
- Live texture updates

---

### 25. Debug Visualization System
**Status:** Partial (light billboards only)  
**Complexity:** Low-Medium  
**Time Estimate:** 2 days  
**Impact:** Debugging and understanding

**Features to add:**
```cpp
class DebugRenderer {
  // Immediate mode (cleared each frame)
  void drawLine(vec3 start, vec3 end, vec3 color);
  void drawSphere(vec3 center, float radius, vec3 color);
  void drawBox(vec3 min, vec3 max, vec3 color);
  void drawFrustum(const Camera& camera, vec3 color);
  
  // Persistent (manual clear)
  void drawText3D(vec3 pos, const std::string& text);
  
  void render(VkCommandBuffer cmd);
  
private:
  std::vector<LineVertex> lines;
  VkBuffer lineBuffer;
};
```

**Use cases:**
- Visualize bounding boxes
- Show light ranges
- Display frustum for culling debug
- Collision shapes
- Pathfinding graphs

---

## 🎯 Quick Wins (1-2 days each)

If you want immediate visible improvements:

1. ✅ **Texture System** - **DONE**
2. ✅ **Resource Manager** - **DONE** (with async, priority, caching)
3. **Skybox** - Simple cubemap, huge atmosphere improvement  
4. **Bloom Effect** - Professional glow on lights
5. **FPS Counter Graph** - Add frame time history to existing UI
6. **Wireframe Mode** - Toggle for debugging geometry
7. **Hot Shader Reload** - Faster iteration

---

## 📚 Resources

### Learning Materials
- **Vulkan Guide:** https://vkguide.dev/
- **Learn OpenGL (concepts apply):** https://learnopengl.com/
- **PBR Theory:** https://learnopengl.com/PBR/Theory
- **Render Graph:** https://logins.github.io/graphics/2021/05/31/RenderGraphs.html
- **GPU-Driven:** https://advances.realtimerendering.com/s2015/aaltonenhaar_siggraph2015_combined_final_footer_220dpi.pdf
- **Deferred Shading:** https://learnopengl.com/Advanced-Lighting/Deferred-Shading
- **SSAO:** https://learnopengl.com/Advanced-Lighting/SSAO
- **TAA:** https://www.elopezr.com/temporal-aa-and-the-quest-for-the-holy-trail/

### Code References
- **Sascha Willems Vulkan Examples:** https://github.com/SaschaWillems/Vulkan
- **NVIDIA Vulkan Samples:** https://github.com/nvpro-samples
- **AMD GPU Open:** https://gpuopen.com/
- **Filament Engine (Google):** https://github.com/google/filament

---

## Notes

- Focus on **visual quality first** (shadows, IBL, post-processing) for impressive results
- **Measure before optimizing** (implement profiler early)
- **Skybox + IBL is a quick win** - adds atmosphere with minimal effort
- **Shadows are critical** - without them, objects look floating
- **Deferred rendering unlocks many effects** (SSAO, SSR, 100+ lights)
- **Test on target hardware** (laptop vs desktop GPUs vary significantly)

This roadmap prioritizes features that provide the best **visual quality** and **developer experience** improvements relative to implementation effort.
