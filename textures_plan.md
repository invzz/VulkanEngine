# PBR Texture System Implementation Plan

## Overview
Add full texture support to the PBR rendering system, enabling materials to use image-based properties (albedo, normal, roughness, metallic, AO) instead of just scalar values.

---

## Phase 1: Foundation - Texture Class

### 1.1 Create Texture Class
**File**: `include/3dEngine/Texture.hpp`, `src/3dEngine/Texture.cpp`

**Purpose**: Load images and manage Vulkan texture resources.

**Dependencies**:
- `stb_image.h` for PNG/JPG loading (add to xmake.lua)
- Existing `Device`, `Buffer` classes

**Class Structure**:
```cpp
class Texture {
public:
  static std::unique_ptr<Texture> createFromFile(
    Device& device, 
    const std::string& filepath,
    VkFormat format = VK_FORMAT_R8G8B8A8_SRGB
  );
  
  VkImageView getImageView() const { return imageView; }
  VkSampler getSampler() const { return sampler; }
  
private:
  Texture(Device& device);
  void createTextureImage(const std::string& filepath, VkFormat format);
  void createTextureImageView(VkFormat format);
  void createTextureSampler();
  void generateMipmaps(VkImage image, VkFormat format, int32_t width, int32_t height, uint32_t mipLevels);
  
  Device& device;
  VkImage image;
  VkDeviceMemory imageMemory;
  VkImageView imageView;
  VkSampler sampler;
  uint32_t mipLevels;
};
```

**Implementation Steps**:
1. Load image data with `stb_image`
2. Create staging buffer with image data
3. Create VkImage with optimal tiling and mip levels
4. Copy from staging buffer to image
5. Transition image layout: UNDEFINED → TRANSFER_DST → SHADER_READ_ONLY
6. Generate mipmaps using vkCmdBlitImage
7. Create VkImageView for shader access
8. Create VkSampler with anisotropic filtering

**Image Formats**:
- Albedo/BaseColor: `VK_FORMAT_R8G8B8A8_SRGB` (sRGB for color)
- Normal: `VK_FORMAT_R8G8B8A8_UNORM` (linear space)
- Roughness/Metallic/AO: `VK_FORMAT_R8_UNORM` (single channel)

**Sampler Settings**:
- Filter: Linear (min/mag/mipmap)
- Anisotropy: 16x (quality)
- Address mode: Repeat (for tiling)

---

## Phase 2: Material Integration

### 2.1 Update PBRMaterial Structure
**File**: `include/3dEngine/PBRMaterial.hpp`

**Add texture pointers**:
```cpp
struct PBRMaterial {
  // Scalar values (used when no texture or as multipliers)
  glm::vec3 albedo{1.0f, 1.0f, 1.0f};
  float metallic{0.0f};
  float roughness{0.5f};
  float ao{1.0f};
  float clearcoat{0.0f};
  float clearcoatRoughness{0.03f};
  float anisotropic{0.0f};
  float anisotropicRotation{0.0f};
  
  // Texture maps (optional - nullptr if not used)
  std::shared_ptr<Texture> albedoMap;
  std::shared_ptr<Texture> normalMap;
  std::shared_ptr<Texture> metallicMap;
  std::shared_ptr<Texture> roughnessMap;
  std::shared_ptr<Texture> aoMap;
  
  // Helper methods
  bool hasAlbedoMap() const { return albedoMap != nullptr; }
  bool hasNormalMap() const { return normalMap != nullptr; }
  bool hasMetallicMap() const { return metallicMap != nullptr; }
  bool hasRoughnessMap() const { return roughnessMap != nullptr; }
  bool hasAOMap() const { return aoMap != nullptr; }
  bool hasAnyTexture() const { return hasAlbedoMap() || hasNormalMap() || hasMetallicMap() || hasRoughnessMap() || hasAOMap(); }
};
```

**Behavior**:
- If texture present: Sample texture, multiply by scalar value
- If no texture: Use scalar value directly
- Example: `finalAlbedo = push.albedo * texture(albedoMap, uv).rgb`

---

## Phase 3: Descriptor System

### 3.1 Create Material Descriptor Set Layout
**File**: `src/3dEngine/systems/PBRRenderSystem.cpp`

**Descriptor Set 1 Layout** (textures):
```cpp
VkDescriptorSetLayoutBinding bindings[] = {
  { .binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, ... }, // albedoMap
  { .binding = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, ... }, // normalMap
  { .binding = 2, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, ... }, // metallicMap
  { .binding = 3, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, ... }, // roughnessMap
  { .binding = 4, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, ... }, // aoMap
};
```

**Note**: All bindings are COMBINED_IMAGE_SAMPLER (texture + sampler in one).

### 3.2 Descriptor Pool
**Expand existing pool** or **create material-specific pool**:
```cpp
VkDescriptorPoolSize poolSizes[] = {
  { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, maxFrames * maxObjects },
  { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 5 * maxMaterials }, // 5 textures per material
};
```

### 3.3 Descriptor Set Management
**Per-Material Descriptor Sets**:
- Create descriptor set for each unique material
- Bind dummy/default white texture if material doesn't have a specific texture
- Cache descriptor sets by material ID

**Material Manager Class** (optional):
```cpp
class MaterialManager {
  VkDescriptorSet getOrCreateDescriptorSet(const PBRMaterial& material);
  std::unordered_map<size_t, VkDescriptorSet> materialDescriptorSets;
};
```

---

## Phase 4: Pipeline & Shader Updates

### 4.1 Update Pipeline Layout
**File**: `src/3dEngine/systems/PBRRenderSystem.cpp`

**Current**:
```cpp
VkDescriptorSetLayout layouts[] = {globalSetLayout};
```

**New**:
```cpp
VkDescriptorSetLayout layouts[] = {globalSetLayout, materialSetLayout};
```

### 4.2 Update Push Constants
**Add texture enable flags**:
```cpp
struct PBRPushConstantData {
  glm::mat4 modelMatrix;
  glm::mat4 normalMatrix;
  glm::vec3 albedo;
  float metallic;
  float roughness;
  float ao;
  float isSelected;
  float clearcoat;
  float clearcoatRoughness;
  float anisotropic;
  float anisotropicRotation;
  
  // NEW: Texture flags (bitmask)
  uint32_t textureFlags; // Bit 0: hasAlbedoMap, Bit 1: hasNormalMap, etc.
};
```

### 4.3 Update Fragment Shader
**File**: `assets/shaders/pbr_shader.frag`

**Add texture bindings**:
```glsl
layout(set = 1, binding = 0) uniform sampler2D albedoMap;
layout(set = 1, binding = 1) uniform sampler2D normalMap;
layout(set = 1, binding = 2) uniform sampler2D metallicMap;
layout(set = 1, binding = 3) uniform sampler2D roughnessMap;
layout(set = 1, binding = 4) uniform sampler2D aoMap;
```

**Add push constant texture flags**:
```glsl
layout(push_constant) uniform PushConstants {
  // ... existing fields
  uint textureFlags;
} push;
```

**Update main() function**:
```glsl
void main() {
  vec3 N = normalize(fragmentNormalWorld);
  vec3 V = normalize(ubo.cameraPosition.xyz - fragmentWorldPos);
  vec2 uv = fragUV; // From vertex shader
  
  // Sample albedo
  vec3 albedo = push.albedo;
  if ((push.textureFlags & 0x01) != 0) {
    vec3 sampledAlbedo = texture(albedoMap, uv).rgb;
    sampledAlbedo = pow(sampledAlbedo, vec3(2.2)); // sRGB to linear
    albedo *= sampledAlbedo;
  }
  
  // Sample normal map
  if ((push.textureFlags & 0x02) != 0) {
    vec3 tangentNormal = texture(normalMap, uv).xyz * 2.0 - 1.0;
    // Transform tangent normal to world space (requires tangent/bitangent)
    vec3 T = normalize(fragTangent);
    vec3 B = normalize(fragBitangent);
    mat3 TBN = mat3(T, B, N);
    N = normalize(TBN * tangentNormal);
  }
  
  // Sample metallic
  float metallic = push.metallic;
  if ((push.textureFlags & 0x04) != 0) {
    metallic *= texture(metallicMap, uv).r;
  }
  
  // Sample roughness
  float roughness = push.roughness;
  if ((push.textureFlags & 0x08) != 0) {
    roughness *= texture(roughnessMap, uv).r;
  }
  
  // Sample AO
  float ao = push.ao;
  if ((push.textureFlags & 0x10) != 0) {
    ao *= texture(aoMap, uv).r;
  }
  
  // Continue with PBR calculation using textured values...
}
```

### 4.4 Tangent Space (for Normal Mapping)
**Option A - Generate in shader** (simpler):
```glsl
vec3 getTangent(vec3 N, vec3 p, vec2 uv) {
  vec3 dp1 = dFdx(p);
  vec3 dp2 = dFdy(p);
  vec2 duv1 = dFdx(uv);
  vec2 duv2 = dFdy(uv);
  vec3 T = normalize(dp1 * duv2.y - dp2 * duv1.y);
  return normalize(T - dot(T, N) * N);
}
```

**Option B - Pre-compute in model** (better quality):
- Add tangent/bitangent to `Model::Vertex`
- Compute during model loading using MikkTSpace algorithm
- Pass as vertex attributes

---

## Phase 5: Rendering Integration

### 5.1 Update PBRRenderSystem::render()
**New rendering flow**:
```cpp
void PBRRenderSystem::render(FrameInfo& frameInfo) {
  pipeline->bind(frameInfo.commandBuffer);
  
  // Bind global descriptor set (set 0: camera, lights)
  vkCmdBindDescriptorSets(..., 0, 1, &frameInfo.globalDescriptorSet, ...);
  
  // Group objects by material to minimize descriptor set switches
  std::map<size_t, std::vector<GameObject*>> objectsByMaterial;
  for (auto& [id, obj] : frameInfo.gameObjects) {
    if (obj.pbrMaterial) {
      size_t materialHash = hashMaterial(obj.pbrMaterial);
      objectsByMaterial[materialHash].push_back(&obj);
    }
  }
  
  // Render each material batch
  for (auto& [materialHash, objects] : objectsByMaterial) {
    // Get/create descriptor set for this material
    VkDescriptorSet materialSet = getMaterialDescriptorSet(objects[0]->pbrMaterial);
    
    // Bind material descriptor set (set 1: textures)
    vkCmdBindDescriptorSets(..., 1, 1, &materialSet, ...);
    
    // Draw all objects with this material
    for (auto* obj : objects) {
      // Update push constants with material scalars + texture flags
      PBRPushConstantData push = buildPushConstants(*obj);
      vkCmdPushConstants(..., &push);
      
      obj->model->bind(frameInfo.commandBuffer);
      obj->model->draw(frameInfo.commandBuffer);
    }
  }
}
```

### 5.2 Default Textures
**Create 1x1 pixel fallback textures**:
- White texture (1.0, 1.0, 1.0) - for albedo/roughness/metallic/AO
- Flat normal (0.5, 0.5, 1.0) - for normal maps
- Used when material doesn't have a specific texture

---

## Phase 6: Scene Loading (Asphalt Floor)

### 6.1 Update SceneLoader::createFloor()
**File**: `src/demos/Cube/SceneLoader.cpp`

```cpp
void SceneLoader::createFloor(Device& device, GameObject::Map& gameObjects) {
  auto floorModel = Model::createModelFromFile(device, "/quad.obj");
  
  // Create PBR object with neutral values (textures will override)
  auto floor = GameObject::makePBRObject(
    floorModel, 
    glm::vec3(1.0f),  // White - will be multiplied by albedo texture
    0.0f,              // Non-metallic asphalt
    1.0f,              // Will be multiplied by roughness texture
    1.0f               // Will be multiplied by AO texture
  );
  
  // Load asphalt textures
  const std::string texPath = "assets/textures/Asphalt01_MR_4K/";
  floor.pbrMaterial->albedoMap = Texture::createFromFile(
    device, texPath + "Asphalt01_4K_BaseColor.png", VK_FORMAT_R8G8B8A8_SRGB);
  floor.pbrMaterial->normalMap = Texture::createFromFile(
    device, texPath + "Asphalt01_4K_Normal.png", VK_FORMAT_R8G8B8A8_UNORM);
  floor.pbrMaterial->roughnessMap = Texture::createFromFile(
    device, texPath + "Asphalt01_4K_Roughness.png", VK_FORMAT_R8_UNORM);
  floor.pbrMaterial->aoMap = Texture::createFromFile(
    device, texPath + "Asphalt01_4K_AO.png", VK_FORMAT_R8_UNORM);
  
  floor.transform.scale = {4.0f, 1.0f, 4.0f};
  floor.transform.translation = {0.0f, 0.0f, 0.0f};
  
  gameObjects.try_emplace(floor.getId(), std::move(floor));
}
```

---

## Phase 7: Testing & Optimization

### 7.1 Initial Testing
1. Load single BaseColor texture on floor
2. Verify correct sampling and tiling
3. Add normal map, check lighting response
4. Add roughness/AO, verify material appearance

### 7.2 Debugging Tools
- Texture visualization modes (albedo only, normals only, etc.)
- Wireframe overlay
- UV coordinate visualization
- Mipmap level visualization

### 7.3 Performance Optimization
- **Descriptor Set Caching**: Reuse sets for identical materials
- **Texture Compression**: BC7 for albedo, BC5 for normals
- **Texture Streaming**: Load lower resolution first
- **Batch by Material**: Minimize descriptor set switches
- **Async Texture Loading**: Don't block main thread

### 7.4 Memory Management
- Smart pointer sharing for duplicate textures
- Texture unloading for unused materials
- Descriptor pool sizing based on scene complexity

---

## Implementation Checklist

### Phase 1 - Foundation
- [ ] Add stb_image to xmake.lua dependencies
- [ ] Create Texture.hpp/cpp with image loading
- [ ] Implement Vulkan image creation and transitions
- [ ] Add mipmap generation
- [ ] Create texture sampler with anisotropic filtering
- [ ] Test loading a single PNG texture

### Phase 2 - Material Integration  
- [ ] Update PBRMaterial with texture pointers
- [ ] Add texture helper methods (hasAlbedoMap, etc.)
- [ ] Create default white/normal textures

### Phase 3 - Descriptors
- [ ] Create material descriptor set layout (5 image samplers)
- [ ] Expand descriptor pool for textures
- [ ] Implement descriptor set creation per material
- [ ] Add descriptor set caching by material hash

### Phase 4 - Pipeline & Shaders
- [ ] Update pipeline layout with material descriptor set
- [ ] Add textureFlags to push constants
- [ ] Update fragment shader with texture bindings
- [ ] Implement texture sampling with flag checks
- [ ] Add tangent space calculation for normal mapping
- [ ] Handle sRGB → linear conversion for albedo

### Phase 5 - Rendering
- [ ] Update PBRRenderSystem to bind material descriptor sets
- [ ] Implement material batching (group by material)
- [ ] Build push constants with texture flags
- [ ] Test descriptor set switching performance

### Phase 6 - Scene Integration
- [ ] Load asphalt textures in SceneLoader
- [ ] Apply textures to floor GameObject
- [ ] Verify UV coordinates on quad model
- [ ] Test tiling with floor scale

### Phase 7 - Polish
- [ ] Add error handling for missing textures
- [ ] Implement texture format validation
- [ ] Add debug visualization modes
- [ ] Profile descriptor set switching cost
- [ ] Document texture system usage

---

## File Structure

```
include/3dEngine/
  Texture.hpp               # NEW - Texture loading and management
  PBRMaterial.hpp          # MODIFIED - Add texture pointers
  systems/
    PBRRenderSystem.hpp    # MODIFIED - Add material descriptor sets

src/3dEngine/
  Texture.cpp              # NEW - Texture implementation
  systems/
    PBRRenderSystem.cpp    # MODIFIED - Descriptor sets, material batching

assets/shaders/
  pbr_shader.frag          # MODIFIED - Texture sampling
  pbr_shader.vert          # MODIFIED - Pass UV to fragment shader

src/demos/Cube/
  SceneLoader.cpp          # MODIFIED - Load asphalt textures

xmake.lua                  # MODIFIED - Add stb_image dependency
```

---

## Dependencies

### External Libraries
- **stb_image**: PNG/JPG image loading (single header, public domain)
  - Add to xmake.lua: `add_includedirs("external/stb")`
  - Download: https://github.com/nothings/stb/blob/master/stb_image.h

### Vulkan Features
- VkImage, VkImageView, VkSampler
- vkCmdCopyBufferToImage
- vkCmdBlitImage (for mipmaps)
- VkDescriptorSetLayoutBinding with COMBINED_IMAGE_SAMPLER
- Anisotropic filtering (check device features)

---

## Expected Results

### Visual Improvements
1. **Asphalt Floor**: Realistic surface detail from 4K textures
2. **Normal Mapping**: Surface bumps respond to lighting
3. **Roughness Variation**: Specular highlights vary across surface
4. **Ambient Occlusion**: Subtle darkening in crevices

### Performance
- Texture sampling: ~0.5-1ms per frame (depending on resolution)
- Descriptor set switches: ~10-50μs per switch
- Total overhead: <5% with proper batching

---

## Future Enhancements

1. **Metallic-Roughness Combined Texture**: Use RG channels for both
2. **PBR from glTF**: Auto-load textures from glTF material definitions
3. **Texture Arrays**: Store multiple textures in one binding
4. **Bindless Textures**: Use descriptor indexing (Vulkan 1.2+)
5. **Virtual Texturing**: Stream tiles on demand for large textures
6. **Parallax Occlusion Mapping**: Use height map for depth effect
7. **Texture Compression**: BC7/BC5 for better memory usage
8. **HDR Environment Maps**: IBL for realistic reflections

---

## Notes

- **sRGB Correctness**: Albedo textures in sRGB format, linear in shader
- **Normal Map Convention**: OpenGL (Y+) vs DirectX (Y-) - check texture source
- **Texture Budget**: 4K textures = ~16MB each uncompressed
- **UV Tiling**: Ensure quad model has correct UVs for texture repeat
- **Descriptor Limits**: Check `maxDescriptorSetSamplers` device limit

---

## References

- Vulkan Tutorial: https://vulkan-tutorial.com/Texture_mapping
- stb_image: https://github.com/nothings/stb
- PBR Theory: https://learnopengl.com/PBR/Theory
- Normal Mapping: https://learnopengl.com/Advanced-Lighting/Normal-Mapping
- Mipmap Generation: https://www.khronos.org/opengl/wiki/Common_Mistakes#Automatic_mipmap_generation
