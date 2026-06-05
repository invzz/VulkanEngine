# Descriptor Lifecycle: Before vs After Clean Architecture Migration

## Overview

This document compares the descriptor lifecycle management between the original monolithic `EngineState` design and the new `DescriptorManager`-based architecture. The goal is to identify potential bugs introduced by the refactoring that could cause runtime descriptor allocation failures.

---

## 1. Architecture Comparison

### Before (Monolithic EngineState)

```
EngineState
├── gbufferPool (unique_ptr<DescriptorPool>)
├── gbufferSetLayout (unique_ptr<DescriptorSetLayout>)
├── gbufferDescriptorSets (vector<VkDescriptorSet>)
├── deferredIblPool
├── deferredIblSetLayout
├── deferredIblDescriptorSets
├── deferredShadowPool
├── deferredShadowSetLayout
├── deferredShadowDescriptorSets
├── postProcessPool
├── postProcessSetLayout
└── postProcessDescriptorSets
```

**Ownership:** All descriptor resources owned directly by `EngineState` as member variables.

**Lifecycle:**
1. `initDescriptorResources()` — creates pools and layouts via `DescriptorPool::Builder` and `DescriptorSetLayout::Builder`
2. `allocatePerFrameDescriptorSets()` — allocates per-frame descriptor sets using `DescriptorWriter::build()`
3. Runtime passes access descriptors via direct member access or `IDescriptorAccessPort` adapter

### After (DescriptorManager)

```
EngineState
└── descriptorManager (unique_ptr<DescriptorManager>)
    ├── gbufferPool_
    ├── gbufferSetLayout_
    ├── gbufferDescriptorSets_
    ├── deferredIblPool_
    ├── deferredIblSetLayout_
    ├── deferredIblDescriptorSets_
    ├── deferredShadowPool_
    ├── deferredShadowSetLayout_
    ├── deferredShadowDescriptorSets_
    ├── postProcessPool_
    ├── postProcessSetLayout_
    └── postProcessDescriptorSets_
```

**Ownership:** All descriptor resources delegated to `DescriptorManager`, owned by `EngineState` via `unique_ptr`.

**Lifecycle:**
1. `EngineState::initDescriptorResources()` — calls `descriptorManager->createDescriptorResources()`
2. `EngineState::allocatePerFrameDescriptorSets()` — calls `descriptorManager->allocatePerFrameDescriptors()`
3. Runtime passes access descriptors via `IDescriptorAccessPort` adapter (unchanged)

---

## 2. Descriptor Pool Creation Differences

### Before

```cpp
// EngineState.cpp — initDescriptorResources()
gbufferPool = DescriptorPool::Builder(device)
    .setMaxSets(SwapChain::maxFramesInFlight())
    .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SwapChain::maxFramesInFlight() * 4)
    .build();

gbufferSetLayout = DescriptorSetLayout::Builder(device)
    .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
    .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
    .addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
    .addBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
    .build();
```

**Key details:**
- Pool `maxSets` = `SwapChain::maxFramesInFlight()` (typically 2 or 3)
- Pool size for `VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER` = `maxFramesInFlight() * 4`
- Layout has 4 bindings, each expecting 1 combined image sampler

### After

```cpp
// DescriptorManager.cpp — createDescriptorResources()
gbufferPool_ = DescriptorPool::Builder(device)
    .setMaxSets(SwapChain::maxFramesInFlight())
    .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SwapChain::maxFramesInFlight() * 4)
    .build();

gbufferSetLayout_ = DescriptorSetLayout::Builder(device)
    .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
    .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
    .addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
    .addBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
    .build();
```

**Comparison:** ✅ **Identical.** Pool sizes, maxSets, and layout bindings match exactly.

---

## 3. Per-Frame Descriptor Allocation Differences

### Before

```cpp
// EngineState.cpp — allocatePerFrameDescriptorSets()
gbufferDescriptorSets.resize(SwapChain::maxFramesInFlight());
for (int i = 0; i < SwapChain::maxFramesInFlight(); i++) {
    auto nInfo = renderer.getGbufferNormalImageInfo(i);
    auto aInfo = renderer.getGbufferAlbedoImageInfo(i);
    auto mInfo = renderer.getGbufferMaterialImageInfo(i);
    auto dInfo = renderer.getDepthImageInfo(i);
    
    DescriptorWriter(*gbufferSetLayout, *gbufferPool)
        .writeImage(0, &nInfo)
        .writeImage(1, &aInfo)
        .writeImage(2, &mInfo)
        .writeImage(3, &dInfo)
        .build(gbufferDescriptorSets[i]);
}
```

**Key details:**
- Resizes vector to `maxFramesInFlight()`
- Allocates descriptor set via `DescriptorWriter::build()`
- `build()` calls `pool.allocateDescriptor()` then `overwrite()`

### After

```cpp
// DescriptorManager.cpp — allocatePerFrameDescriptors()
gbufferDescriptorSets_.resize(SwapChain::maxFramesInFlight());
for (int i = 0; i < static_cast<int>(gbufferDescriptorSets_.size()); ++i) {
    auto nInfo = renderer.getGbufferNormalImageInfo(i);
    auto aInfo = renderer.getGbufferAlbedoImageInfo(i);
    auto mInfo = renderer.getGbufferMaterialImageInfo(i);
    auto dInfo = renderer.getDepthImageInfo(i);
    
    DescriptorWriter(*gbufferSetLayout_, *gbufferPool_)
        .writeImage(0, &nInfo)
        .writeImage(1, &aInfo)
        .writeImage(2, &mInfo)
        .writeImage(3, &dInfo)
        .build(gbufferDescriptorSets_[i]);
}
```

**Comparison:** ✅ **Identical logic.** Same resize, same loop bounds, same `DescriptorWriter` usage.

---

## 4. Deferred IBL Descriptor Allocation — ⚠️ POTENTIAL BUG

### Before

```cpp
// EngineState.cpp — allocatePerFrameDescriptorSets()
// Deferred IBL descriptor sets are allocated in initPostProcessing()
// because they need IBLSystem access.
// In allocatePerFrameDescriptorSets():
deferredIblDescriptorSets.resize(SwapChain::maxFramesInFlight());
for (int i = 0; i < SwapChain::maxFramesInFlight(); i++) {
    deferredIblDescriptorSets[i] = VK_NULL_HANDLE;
}

// Later in initPostProcessing():
for (uint32_t i = 0; i < SwapChain::maxFramesInFlight(); i++) {
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = *deferredIblPool;
    allocInfo.pSetLayouts = &deferredIblSetLayout->getDescriptorSetLayout();
    allocInfo.descriptorSetCount = 1;
    vkAllocateDescriptorSets(device.device(), &allocInfo, &deferredIblDescriptorSets[i]);
}
```

**Key details:**
- IBL descriptor sets are allocated **later** in `initPostProcessing()`
- Uses **direct** `vkAllocateDescriptorSets()` call (not `DescriptorWriter::build()`)
- Requires `IBLSystem` to be initialized first

### After

```cpp
// DescriptorManager.cpp — allocatePerFrameDescriptors()
// Deferred IBL per-frame descriptor sets
deferredIblDescriptorSets_.resize(SwapChain::maxFramesInFlight());
for (int i = 0; i < static_cast<int>(deferredIblDescriptorSets_.size()); ++i) {
    // IBL descriptor sets are allocated in initPostProcessing via EngineState
    // because they need IBLSystem access. Initialize with null handles.
    deferredIblDescriptorSets_[i] = VK_NULL_HANDLE;
}
```

**Comparison:** ❌ **Same placeholder logic**, but **critical question**: Is `initPostProcessing()` still called in the new architecture? And does it still allocate IBL descriptor sets correctly?

**⚠️ BUG RISK:** If `initPostProcessing()` is not called, or if it's called before `IBLSystem` is initialized, or if the IBL descriptor allocation path is broken, all IBL descriptor sets will remain `VK_NULL_HANDLE`, causing descriptor update failures at runtime.

---

## 5. Deferred Shadow Descriptor Allocation — ⚠️ POTENTIAL BUG

### Before

```cpp
// EngineState.cpp — allocatePerFrameDescriptorSets()
deferredShadowDescriptorSets.resize(SwapChain::maxFramesInFlight());
for (int i = 0; i < SwapChain::maxFramesInFlight(); i++) {
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = *deferredShadowPool;
    allocInfo.pSetLayouts = &deferredShadowSetLayout->getDescriptorSetLayout();
    allocInfo.descriptorSetCount = 1;
    vkAllocateDescriptorSets(device.device(), &allocInfo, &deferredShadowDescriptorSets[i]);
}
```

**Key details:**
- Uses **direct** `vkAllocateDescriptorSets()` call
- No `DescriptorWriter` involved at allocation time
- Shadow descriptors are **updated later** in `refreshGbufferDescriptors()` or `updateShadowDescriptors()`

### After

```cpp
// DescriptorManager.cpp — allocatePerFrameDescriptors()
deferredShadowDescriptorSets_.resize(SwapChain::maxFramesInFlight());
for (auto& ds : deferredShadowDescriptorSets_) {
    if (!deferredShadowPool_->allocateDescriptor(deferredShadowSetLayout_->getDescriptorSetLayout(), ds)) {
        throw std::runtime_error("Failed to allocate deferred shadow descriptor set");
    }
}
```

**Comparison:** ⚠️ **Different allocation path.** Uses `DescriptorPool::allocateDescriptor()` instead of direct `vkAllocateDescriptorSets()`.

**⚠️ BUG RISK:** 
1. `DescriptorPool::allocateDescriptor()` has an **overflow/fallback pool mechanism** that the old code didn't have. If the primary pool fails, it creates a transient fallback pool. This could mask pool sizing issues.
2. More importantly, `allocateDescriptor()` returns `bool` and throws on failure. If the allocation fails silently (e.g., due to pool sizing mismatch), the `ds` variable may remain uninitialized or contain a stale value.

---

## 6. Post-Processing Descriptor Allocation — ⚠️ POTENTIAL BUG

### Before

```cpp
// EngineState.cpp — allocatePerFrameDescriptorSets()
postProcessDescriptorSets.resize(SwapChain::maxFramesInFlight());
for (int i = 0; i < SwapChain::maxFramesInFlight(); i++) {
    auto imageInfo = renderer.getOffscreenImageInfo(i);
    auto depthInfo = renderer.getDepthImageInfo(i);
    DescriptorWriter(*postProcessSetLayout, *postProcessPool)
        .writeImage(0, &imageInfo)
        .writeImage(1, &depthInfo)
        .build(postProcessDescriptorSets[i]);
}
```

### After

```cpp
// DescriptorManager.cpp — allocatePerFrameDescriptors()
postProcessDescriptorSets_.resize(SwapChain::maxFramesInFlight());
for (int i = 0; i < static_cast<int>(postProcessDescriptorSets_.size()); ++i) {
    auto imageInfo = renderer.getOffscreenImageInfo(i);
    auto depthInfo = renderer.getDepthImageInfo(i);
    DescriptorWriter(*postProcessSetLayout_, *postProcessPool_)
        .writeImage(0, &imageInfo)
        .writeImage(1, &depthInfo)
        .build(postProcessDescriptorSets_[i]);
}
```

**Comparison:** ✅ **Identical.** Same logic, same `DescriptorWriter` usage.

---

## 7. Descriptor Writer Usage — ⚠️ CRITICAL BUG RISK

### Before

```cpp
// CompositionPass.cpp — execute()
DescriptorWriter(descriptorAccess_.getPostProcessSetLayout(), 
                 descriptorAccess_.getDescriptorPool())
    .writeImage(0, &imageInfo)
    .writeImage(1, &depthInfo)
    .overwrite(descriptorAccess_.postProcessDescriptorSetRef(frameInfo.frameIndex));
```

**Key details:**
- `overwrite()` is called **separately** from `build()`
- `build()` allocates the descriptor set, `overwrite()` updates it
- The descriptor set is **pre-allocated** and stored in `EngineState`

### After

```cpp
// DescriptorManager.cpp — updateGbufferDescriptors()
DescriptorWriter(*gbufferSetLayout_, *gbufferPool_)
    .writeImage(0, &nInfo)
    .writeImage(1, &aInfo)
    .writeImage(2, &mInfo)
    .writeImage(3, &dInfo)
    .overwrite(gbufferDescriptorSets_[frameIndex]);
```

**Comparison:** ⚠️ **Same `overwrite()` pattern**, but:

**⚠️ BUG RISK:** If `gbufferDescriptorSets_[frameIndex]` is `VK_NULL_HANDLE` (not allocated), `overwrite()` will call `vkUpdateDescriptorSets()` with an invalid `dstSet`, causing a Vulkan validation error or crash.

---

## 8. IDescriptorAccessPort Adapter — ✅ UNCHANGED

The `IDescriptorAccessPort` interface and `DescriptorAccessAdapter` implementation are **unchanged** in the new architecture. They still delegate to `EngineState`, which now delegates to `DescriptorManager`. This layer is **not** a source of bugs.

---

## 9. Initialization Order — ⚠️ POTENTIAL BUG

### Before

```cpp
// EngineState::initialize()
void EngineState::initialize(...) {
    initCoreSystems(device, renderer, ...);
    initDescriptorResources(device, renderer);      // Step 1: pools + layouts
    allocatePerFrameDescriptorSets(renderer);        // Step 2: per-frame sets
    initPostProcessing(device, renderer);            // Step 3: IBL sets
    initInputRelatedSystems(window);
}
```

### After

```cpp
// EngineState::initialize() (new)
void EngineState::initialize(...) {
    initCoreSystems(device, renderer, ...);
    initDescriptorResources(device, renderer);      // Step 1: pools + layouts
    allocatePerFrameDescriptorSets(renderer);        // Step 2: per-frame sets
    initPostProcessing(device, renderer);            // Step 3: IBL sets
    initInputRelatedSystems(window);
}
```

**Comparison:** ✅ **Same order.** But the critical question is: **Is `initPostProcessing()` still being called?**

**⚠️ BUG RISK:** If `initPostProcessing()` is removed or conditionally skipped in the new architecture, IBL descriptor sets will never be allocated, remaining `VK_NULL_HANDLE`.

---

## 10. Summary of Potential Bugs

| # | Area | Risk Level | Description |
|---|------|-----------|-------------|
| 1 | **IBL Descriptor Allocation** | 🔴 **HIGH** | IBL descriptor sets are initialized to `VK_NULL_HANDLE` in `allocatePerFrameDescriptors()`. They are supposed to be allocated later in `initPostProcessing()`. If this function is not called, or called before `IBLSystem` is ready, IBL descriptors remain null. |
| 2 | **Shadow Descriptor Allocation** | 🟡 **MEDIUM** | Changed from direct `vkAllocateDescriptorSets()` to `DescriptorPool::allocateDescriptor()`. The new path has overflow pool logic that could mask sizing issues. Also, if allocation fails, the descriptor set may be left in an undefined state. |
| 3 | **Descriptor Writer overwrite()** | 🟡 **MEDIUM** | `overwrite()` is called on pre-allocated descriptor sets. If any descriptor set is `VK_NULL_HANDLE` (not allocated), `vkUpdateDescriptorSets()` will fail. |
| 4 | **Pool Sizing** | 🟢 **LOW** | Pool sizes and maxSets appear identical between old and new. No mismatch detected. |
| 5 | **Layout Bindings** | 🟢 **LOW** | Layout bindings appear identical between old and new. No mismatch detected. |
| 6 | **Initialization Order** | 🟡 **MEDIUM** | Must verify `initPostProcessing()` is still called after `IBLSystem` is initialized. |

---

## 11. Recommended Debugging Steps

1. **Add validation in `allocatePerFrameDescriptors()`:**
   ```cpp
   // After allocating all descriptor sets, verify none are VK_NULL_HANDLE
   for (int i = 0; i < SwapChain::maxFramesInFlight(); i++) {
       assert(gbufferDescriptorSets_[i] != VK_NULL_HANDLE && "G-buffer descriptor not allocated");
       assert(postProcessDescriptorSets_[i] != VK_NULL_HANDLE && "Post-process descriptor not allocated");
       // IBL sets may be null at this point — they're allocated in initPostProcessing()
   }
   ```

2. **Verify `initPostProcessing()` is called:**
   - Check that `EngineState::initialize()` still calls `initPostProcessing()`
   - Verify `IBLSystem` is initialized before `initPostProcessing()`

3. **Check `DescriptorPool::allocateDescriptor()` return values:**
   - The new shadow descriptor allocation checks the return value, but the IBL and post-process allocations do not (they use `DescriptorWriter::build()` which returns `bool`).

4. **Run with Vulkan validation layers:**
   - Enable `VK_DEBUG_REPORT_ERROR_BIT_EXT` and `VK_DEBUG_REPORT_WARNING_BIT_EXT`
   - Look for `vkUpdateDescriptorSets` errors with invalid `dstSet`

5. **Compare `getGbufferNormalImageInfo()`, `getGbufferAlbedoImageInfo()`, etc. return values:**
   - These `Renderer` methods must return valid `VkDescriptorImageInfo` structures
   - If they return invalid image layouts or null handles, `DescriptorWriter::writeImage()` will write invalid data

---

## 12. Key Files to Review

| File | What to Check |
|------|--------------|
| `src/Engine/EngineFacade.cpp` | Is `initPostProcessing()` still called? |
| `src/Engine/Graphics/DescriptorManager.cpp` | Are all allocation paths correct? |
| `src/Engine/Graphics/Passes/OffscreenPass.cpp` | Are descriptor updates using valid handles? |
| `src/Engine/Graphics/Passes/CompositionPass.cpp` | Are descriptor updates using valid handles? |
| `src/Engine/Systems/IBLSystem.cpp` | Is IBL descriptor allocation still correct? |
| `src/Engine/Systems/ShadowSystem.cpp` | Are shadow descriptor updates correct? |

---

## 13. Conclusion

The refactoring **preserves the descriptor pool sizes, layout bindings, and allocation logic** for G-buffer, post-processing, and deferred shadow descriptor sets. The **main risk areas** are:

1. **IBL descriptor sets** — allocated later in `initPostProcessing()`, which may not be called or may be called at the wrong time.
2. **Descriptor pool overflow behavior** — the new `DescriptorPool::allocateDescriptor()` has overflow pool logic that could mask sizing issues.
3. **Null descriptor set handles** — if any descriptor set is not allocated before `overwrite()` is called, Vulkan will fail.

**Recommended immediate action:** Verify `initPostProcessing()` is called in the new `EngineState::initialize()` and that IBL descriptor sets are allocated before any pass tries to update them.
