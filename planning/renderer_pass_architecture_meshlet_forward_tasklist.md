# Renderer Pass Architecture — Task List (Meshlet Forward)

This checklist tracks the work needed to reach the architecture described in `planning/renderer_pass_architecture_meshlet_forward.md`.

Legend:

- **P0** = unblocks core architecture
- **P1** = required for target feature set
- **P2** = polish / scalability / maintainability

---

## P0 — Correctness + Pass Order Foundations

- [x] **Fix HZB descriptor wiring (critical correctness)**

  - **Problem**: Global set binding 2 (`hzbTexture`) is currently fed by `Renderer::getDepthImageInfo(...)` (depth view/sampler), not the HZB pyramid image. The task shader samples `textureLod(...)` assuming a mip pyramid, so this is incorrect and can cause invalid sampling / wrong results.
  - **Do**:
    - Add `Renderer::getHzbImageInfo(int frameIndex)` returning `{ imageView = FrameBuffer::getHzbImageView(frameIndex), sampler = FrameBuffer::getHzbSampler(), imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }`.
    - Update `App::render()` to call `renderContext->updateHZBDescriptor(frameIndex, renderer.getHzbImageInfo(prevFrameIndex))` (or current frame once pass order is fixed).
    - Transition HZB images to `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL` on swapchain creation so the first frame has a valid readable layout.
  - **Touchpoints**: `src/Engine/Graphics/Renderer.cpp`, `include/Engine/Graphics/Renderer.hpp`, `src/demos/Cube/app.cpp`, `src/demos/Cube/RenderContext.cpp`
  - **Acceptance**: `assets/shaders/simple_mesh.task` can sample multiple mips without validation errors and occlusion behavior is stable.

- [x] **Split scene rendering into explicit per-pass entry points**

  - **Do**: Refactor `App::renderScenePhase(...)` into pass-scoped functions (e.g. `renderDepthPrepass(...)`, `renderOpaque(...)`, `renderSky(...)`, `renderTransparent(...)`, `renderDebug(...)`).
  - **Touchpoints**: `src/demos/Cube/app.cpp`
  - **Acceptance**: Each phase can be independently scheduled in `RenderGraph` without side-effects.

- [x] **Make a real Depth Prepass possible**

  - **Do**: Provide a depth-only render path in the offscreen pipeline:
    - Color writes OFF, depth test ON, depth write ON
    - Opaque-only draw list
  - **Touchpoints**: `src/Engine/Graphics/Renderer.cpp`, offscreen render pass/framebuffer setup, `MeshRenderSystem`
  - **Acceptance**: Depth is populated before shading; Main pass can use depth test `EQUAL`/`LESS_EQUAL` with depth write OFF.

- [x] **Add Depth Prepass node to RenderGraph (after CPU setup, before HZB)**

  - **Do**: Insert a dedicated `DepthPrepass` RenderGraph pass.
  - **Touchpoints**: `src/demos/Cube/app.cpp`
  - **Acceptance**: Depth prepass runs without drawing sky/transparent.

- [x] **Move HZB build earlier (after Depth Prepass, same frame)**
  - **Do**: Schedule `Renderer::generateDepthPyramid(...)` immediately after Depth Prepass.
  - **Touchpoints**: `src/demos/Cube/app.cpp`, `src/Engine/Graphics/Renderer.cpp`, `src/Engine/Graphics/HZBGenerator.cpp`
  - **Acceptance**: Task shader HZB culling uses current-frame depth pyramid.

---

## P0 — Meshlet Culling Modes Match the Doc

- [ ] **Implement frustum-only task shader variant**

  - **Do**: Add a frustum-only variant (separate file or compile-time define) for:
    - Depth Prepass
    - Shadow passes
    - Transparent pass
  - **Touchpoints**: `assets/shaders/simple_mesh.task` (+ shader compile rules)
  - **Acceptance**: Those passes never depend on HZB sampling.

- [ ] **Enable/parameterize culling controls**
  - **Do**:
    - Remove the hard-disable `if (false && ...)` for cone culling.
    - Add a culling mode enum/bitmask in push constants to control:
      - frustum
      - screen-size
      - cone
      - HZB
  - **Touchpoints**: `assets/shaders/simple_mesh.task`, `src/Engine/Systems/MeshRenderSystem.cpp`
  - **Acceptance**: Transparent pass uses frustum-only; Opaque main uses full culling.

---

## P1 — Opaque / Sky / Transparent Ordering

- [ ] **Separate opaque and transparent rendering in MeshRenderSystem**

  - **Do**: Split into `renderOpaque(...)` and `renderTransparent(...)` entry points.
  - **Touchpoints**: `src/Engine/Systems/MeshRenderSystem.cpp`
  - **Acceptance**: Transparent pass is isolated and uses blending ON, depth write OFF.

- [ ] **Reorder sky rendering to match target pipeline**
  - **Do**: Render sky after Opaque Main and before Transparent.
  - **Touchpoints**: `src/demos/Cube/app.cpp`, `src/Engine/Systems/SkyboxRenderSystem.*`
  - **Acceptance**: Sky never touches depth prepass; it draws behind opaque.

---

## P1 — Forward+ Clustered Lighting

- [ ] **Move light data to a dynamic Light SSBO**

  - **Do**: Replace fixed-size light arrays in the UBO with a light SSBO (dynamic count). Keep camera + constants in UBO.
  - **Touchpoints**: `Engine/Systems/LightSystem.*`, UBO structs, descriptors
  - **Acceptance**: Supports >16 lights without UBO growth.

- [ ] **Add Light Clustering compute pass (Forward+)**

  - **Do**: Implement compute shader that produces:
    - `clusterLightIndices[]`
    - `clusterOffsets[]`
    - `clusterCounts[]`
  - **Touchpoints**: new shader under `assets/shaders/`, new system (e.g. `LightClusteringSystem`), `src/demos/Cube/app.cpp` RenderGraph scheduling
  - **Acceptance**: Outputs are updated per-frame and readable by fragment shader.

- [ ] **Update PBR shaders to use clustered lights**
  - **Do**: Compute cluster ID in fragment shader and iterate only that cluster’s lights.
  - **Touchpoints**: `assets/shaders/pbr_shader.frag` (+ `pbr_shader_standard.frag` if applicable)
  - **Acceptance**: Lighting cost scales with local light density rather than global light count.

---

## P1 — Debug Rules From the Doc

- [ ] **Debug modes force full PBR variant**

  - **Do**: Ensure debug modes force `pbr_full` variant selection and disable feature stripping.
  - **Touchpoints**: material/shader variant selection code, PBR shader defines
  - **Acceptance**: Debug visualizations never disappear due to missing compile-time features.

- [ ] **Add at least two debug overlays**
  - **Do**: Implement (minimum):
    - HZB visualization
    - Cluster heatmap or light influence
  - **Touchpoints**: post-processing/fullscreen debug shaders or existing `debugMode` plumbing
  - **Acceptance**: Can validate HZB + clusters visually in-engine.

---

## P2 — RenderGraph Evolution (Optional but likely)

- [ ] **Upgrade RenderGraph beyond ordered lambdas**
  - **Do**: Either:
    - Add a lightweight resource declaration + barrier helper per pass, or
    - Move toward dependency scheduling.
  - **Touchpoints**: `src/Engine/Graphics/RenderGraph.cpp`
  - **Acceptance**: Adding passes (DepthPrepass/HZB/Clustering/Opaque/Sky/Transparent/Post/Debug/UI) doesn’t turn into manual hazard management.

---

## Notes / Current State Snapshot

- Current demo pass order: Update → Compute → Shadow → Offscreen (scene) → Composition (post + UI).
- Current depth pyramid generation happens after the Offscreen shaded pass.
- Task shader already contains frustum + screen-size + HZB culling logic, but the HZB binding is currently not actually an HZB pyramid.

# transparency:
