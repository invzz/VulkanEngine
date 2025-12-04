# Bindless Rendering Migration Plan

This document outlines the step-by-step migration from standard descriptor-set-per-material rendering to a modern Bindless GPU-Driven architecture.

## 🎯 Goals
1.  **Decouple CPU/GPU**: Remove the need to rebind descriptor sets per object.
2.  **Scalability**: Support thousands of textures and materials in a single draw call.
3.  **GPU-Driven**: Enable GPU culling and draw generation (Mesh Shaders).

---

## 📦 Phase 1: Foundation (Vulkan 1.2 Setup)
**Objective:** Enable required Vulkan features and extensions without breaking existing rendering.

- [x] **Upgrade Device Creation**
    - Update `Device::createLogicalDevice` to enable `VkPhysicalDeviceVulkan12Features`.
    - Required features:
        - `descriptorIndexing`
        - `runtimeDescriptorArray`
        - `descriptorBindingPartiallyBound`
        - `descriptorBindingVariableDescriptorCount`
        - `shaderSampledImageArrayNonUniformIndexing`
        - `bufferDeviceAddress` (for Phase 3)
        - `shaderInt64` (for 64-bit addresses)

- [x] **Verify Extension Support**
    - Add checks in `Device::isDeviceSuitable`.
    - Ensure `VK_EXT_descriptor_indexing` is available (core in 1.2).

- [x] **Update Descriptor Pool**
    - Update `DescriptorPool::Builder` to support `VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT`.

---

## 🎨 Phase 2: Bindless Textures (The "Mega-Descriptor")
**Objective:** Access all textures from a single global descriptor array in shaders.

- [x] **Global Descriptor Layout**
    - Create a new `GlobalDescriptorSet` layout.
    - **Binding 0**: Global UBO (Camera, Scene data).
    - **Binding 1**: Material SSBO (Array of material structs).
    - **Binding 2**: Sampler Array (Fixed size, e.g., 16 common samplers).
    - **Binding 3**: Texture Array (Unbounded `texture2D textures[]`).
        - Flags: `VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT`.

- [x] **Texture Manager**
    - Create `TextureManager` class.
    - Maintain a `std::vector<VkImageView>` of all loaded textures.
    - Assign a unique `uint32_t globalIndex` to each `Texture` upon loading.
    - Handle growing the descriptor set if texture count exceeds limit (or allocate a huge limit like 4096).

- [x] **Material System Update**
    - Refactor `PBRMaterial` to be a POD (Plain Old Data) struct for GPU upload.
    - Remove `VkDescriptorSet` from `PBRMaterial`.
    - Add `uint32_t albedoIndex`, `normalIndex`, etc., to the material struct.
    - Create a `MaterialBuffer` (SSBO) to store all material data.

- [x] **Shader Update (Fragment)**
    - Enable `GL_EXT_nonuniform_qualifier`.
    - Replace individual texture bindings with:
      ```glsl
      layout(set = 0, binding = 3) uniform texture2D globalTextures[];
      layout(set = 0, binding = 2) uniform sampler globalSamplers[];
      ```
    - Sample using indices:
      ```glsl
      uint texID = materials[matID].albedoIndex;
      vec4 color = texture(sampler2D(globalTextures[nonuniformEXT(texID)], globalSamplers[0]), uv);
      ```

---

## 📐 Phase 3: Bindless Geometry (Buffer Device Address)
**Objective:** Remove vertex buffer bindings; fetch vertex data manually in shader.

- [x] **Buffer Upgrade**
    - Add `VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT` to `Buffer` creation.
    - Implement `Buffer::getDeviceAddress()` returning `VkDeviceAddress` (uint64_t).

- [x] **Model Data Upload**
    - Create a `MeshBuffer` SSBO containing `VkDeviceAddress` for vertex and index buffers for each mesh.
    - Upload this data to GPU.

- [x] **Shader Update (Vertex)**
    - Enable `GL_EXT_buffer_reference`.
    - Define buffer reference types:
      ```glsl
      layout(buffer_reference, std430) readonly buffer VertexBuffer { 
          vec3 position;
          vec3 normal;
          vec2 uv;
      };
      ```
    - Pass `MeshIndex` via Push Constant or `gl_InstanceIndex`.
    - Fetch vertex:
      ```glsl
      VertexBuffer v = VertexBuffer(meshData[meshID].vertexAddress);
      vec3 pos = v[gl_VertexIndex].position;
      ```

---

## 🚀 Phase 4: GPU-Driven Culling & Mesh Shaders
**Objective:** Move scene traversal and culling to the GPU.

- [x] **Meshlet Generation (CPU)**
    - Implement a meshlet builder (using `meshoptimizer` or custom).
    - Split `Model` geometry into "meshlets" (e.g., 64 vertices, 126 triangles).
    - Store meshlet data (bounding sphere, vertex indices, primitive indices) in SSBOs.

- [ ] **Task Shader**
    - Enable `VK_EXT_mesh_shader`.
    - Implement Task Shader to cull meshlet groups against frustum.
    - Emit visible meshlets to Mesh Shader.

- [ ] **Mesh Shader**
    - Generate vertices and primitives for visible meshlets.
    - No Input Assembler usage.

---

## 🛠️ Implementation Notes

### Dependencies
- Vulkan SDK 1.2+
- Shader Model 6.0+ (glslangValidator)

### Risks
- **Hardware Compatibility**: Older GPUs (pre-Turing/RDNA) might struggle with full bindless or mesh shaders.
- **Debugging**: RenderDoc/NSight debugging becomes harder with bindless (dynamic indexing).

### Validation
- Use `VK_LAYER_KHRONOS_validation` with `GPU-Assisted Validation` enabled to catch out-of-bounds descriptor indexing.
