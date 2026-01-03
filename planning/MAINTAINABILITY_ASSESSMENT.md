# Maintainability Assessment (Jan 2026)

Scope: this assessment focuses on _maintainability_ (architecture clarity, API boundaries, build/tooling hygiene, and change-safety). Performance/optimization items are intentionally not the priority here unless they directly affect maintainability.

## Repo snapshot

- Build system: **xmake** with `Engine` (static lib) + `Cube` demo binary.
- Public API: `include/Engine/**` with implementation mirrored in `src/Engine/**`.
- Major modules:
  - Core: window/input helpers.
  - Graphics: Vulkan device/swapchain, renderer, render graph, descriptors.
  - Resources: texture/model import + caches + async loading.
  - Scene: minimal `entt::registry` wrapper.
  - Systems: ECS systems for rendering, lighting, IBL, post-processing, etc.

## Strengths (keep these)

- **Clean folder mirroring** between `include/Engine` and `src/Engine` (makes navigation + ownership easier).
- **Documented Windows setup** (`planning/WINDOWS_BUILD_GUIDE.md`) and cross-platform scripts (`compile_shaders.*`, `format_code.*`).
- A consistent `.clang-format` (2-space indent, 160 columns) + formatter scripts.

## Key maintainability findings

### 1) Tooling: clang-tidy config is currently invalid

- `clang-tidy` fails to parse `.clang-tidy` due to an **unknown top-level key**:
  - `AnalyzeTemporaryDtors: false`
- Impact:
  - Running `clang-tidy` emits config parse errors, and is harder to integrate into CI or local workflows.

Recommendation (quick win): remove/replace unsupported keys so `clang-tidy` runs cleanly.

Status (Jan 2026): addressed. `.clang-tidy` parses cleanly.

### 2) Build portability risk: hard-coded Vulkan SDK paths in xmake

In `xmake.lua`, Windows builds explicitly add:

- `C:/VulkanSDK/1.4.335.0/Include`
- `C:/VulkanSDK/1.4.335.0/Lib`

Impact:

- Requires _the exact SDK version + install path_, which makes onboarding and CI fragile.

Recommendation:

- Prefer `VULKAN_SDK` environment variable (or xmake package support) and avoid pinning absolute paths in `xmake.lua`.

Status (Jan 2026): addressed. Windows builds use `VULKAN_SDK` (and validate `Include/` + `Lib/`).

### 3) Public API coupling: engine boundaries leak Vulkan details everywhere

Example: `engine::Renderer` exposes and consumes raw Vulkan handles (`VkCommandBuffer`, `VkRenderPass`, `VkDescriptorImageInfo`). `engine::Device` exposes raw device/queues/instance.

This is normal for Vulkan engines, but for maintainability it has a cost:

- Larger blast radius for refactors.
- Harder to mock/test.
- Callers can become tightly coupled to lifetime/order constraints.

Recommendation:

- Keep “Vulkan handles everywhere” inside `Graphics/` where possible.
- For higher-level systems, prefer engine-level abstractions (`RenderContext`, `FrameContext`, lightweight wrapper types) so game/app code doesn’t depend on Vulkan types.

### 4) Header hygiene / compile-time coupling

`include/Engine/Graphics/FrameInfo.hpp` is a good example of maintainability debt:

- It includes `Scene.hpp` and `Camera.hpp` (heavy), while it could forward-declare.
- It uses types like `glm::vec4` / `glm::mat4` without including the corresponding GLM headers directly.

Impact:

- Builds become more brittle (“works because someone else included it first”).
- Slower incremental rebuilds.

Recommendation (quick win): apply “include what you use” to public headers and reduce transitive includes.

Status (Jan 2026): partially addressed. `FrameInfo.hpp` was fixed; additional IWYU cleanup is still worthwhile across the public headers.

### 5) “RenderGraph” naming mismatch

`include/Engine/Graphics/RenderGraph.hpp` currently represents a **linear list of passes**, not a dependency graph (no resource edges, topological ordering, etc.).

Impact:

- The name sets expectations that aren’t met, and new contributors can misinterpret it.

Recommendation:

- Either rename it to reflect reality (e.g., `RenderPipeline`, `RenderPassSequence`), or evolve it into an actual graph and keep the name.

### 6) Large responsibilities inside single classes

Example: `Renderer` currently handles swapchain frame lifecycle _and_ offscreen rendering _and_ depth pyramid/HZB generation. `clang-tidy` also flags high cognitive complexity in `Renderer::createHZBPipeline`.

Impact:

- Harder to change without breaking unrelated features.
- More difficult to test.

Recommendation:

- Split into focused components:
  - `FrameGraph/RenderGraph` orchestration
  - `SwapchainRenderer`
  - `OffscreenRenderer`
  - `HZBGenerator` (own pipeline/layout/descriptor pool)

### 7) Scripts: duplicated logic in shader compilation (minor but easy)

`compile_shaders.ps1` has repeated blocks that are copy/paste variants (e.g., the variable name `$geometryShaders` is reused for `.mesh` and `.task`).

Impact:

- Easy to introduce subtle errors, and harder to extend.

Recommendation (quick win): unify into a single function that compiles by extension list.

Status (Jan 2026): addressed. `compile_shaders.ps1` compiles via a single helper function.

## Automated signals (sample clang-tidy run)

I ran `clang-tidy` on a small representative set:

- `src/Engine/Graphics/Device.cpp`
- `src/Engine/Graphics/Renderer.cpp`
- `src/Engine/Resources/ResourceManager.cpp`

Notable maintainability signals:

- `.clang-tidy` parse errors due to `AnalyzeTemporaryDtors`.
- `Renderer::createHZBPipeline` flagged for **high cognitive complexity**.
- Multiple readability warnings:
  - missing braces on single-line `if`s
  - `else` after `return`/`throw`
  - “magic numbers” (e.g., dispatch group sizes)

These are normal in engines, but they’re exactly the kind of issues that accumulate and slow changes down over time.

## Prioritized recommendations

## Progress (implemented)

The following items from this assessment have been implemented during this iteration:

- Tooling: `.clang-tidy` now parses cleanly (removed unsupported keys).
- Build portability (Windows): `xmake.lua` now uses `VULKAN_SDK` instead of hard-coded `C:/VulkanSDK/...` paths.
  - Current behavior: Windows builds **require** `VULKAN_SDK` and validate `Include/` + `Lib/` directories.
- Header hygiene: `include/Engine/Graphics/FrameInfo.hpp` was made self-contained via IWYU + forward declarations.
  - Follow-up: several `src/Engine/Systems/*.cpp` files were updated to include `Scene.hpp` / `Camera.hpp` explicitly.
- Scripts: `compile_shaders.ps1` was refactored to remove duplicated blocks and compile via a single helper.

### Quick wins (0–2 days)

1. ✅ Fix `.clang-tidy` so it parses cleanly (remove unsupported keys).
2. Add minimal scripts/targets:
   - `xmake project -k compile_commands` (documented workflow)
   - `xmake` target or script to run `clang-tidy` on changed files
3. ✅ “Include what you use” cleanup for public headers (start with `FrameInfo.hpp`).
4. ✅ Refactor `compile_shaders.ps1` to remove duplicated blocks.

### Developer workflow (recommended)

- Generate/update `compile_commands.json` (helps IDEs and `clang-tidy`):
  - `xmake project -k compile_commands`
- Run `clang-tidy` on a file using the compilation database:
  - `clang-tidy -p . path/to/file.cpp`

Note: Windows builds expect `VULKAN_SDK` to be set.

### Medium-term (1–3 weeks)

1. Untangle `Renderer` responsibilities:
   - Extract HZB generation into its own class.
2. Clarify naming (`RenderGraph` vs actual behavior).
3. Standardize naming conventions (e.g., `WaitIdle` vs `beginFrame`) across Core/Graphics.
4. Reduce Vulkan type leakage outside `Graphics/` by introducing a `FrameContext`/`RenderContext` boundary.

### Longer-term (1–2 months)

1. Add CI (even minimal): format check + build + clang-tidy (or a smaller check subset).
2. Consider PIMPL or internal-only headers for heavy modules to reduce rebuild times.
3. Establish module dependency rules (e.g., Systems may depend on Scene/Resources but not on low-level Vulkan directly).

## PIMPL recommendation (actionable)

PIMPL (“pointer to implementation”) is a good fit when:

- A public header is heavy (pulls in Vulkan headers, STL containers, platform headers, or lots of engine headers).
- The type changes frequently, causing large rebuilds.
- You want to reduce include coupling and stabilize the public API surface.

In this repo, PIMPL is primarily a **compile-time coupling** tool (not an ABI-stability requirement). The goal is to make it harder for unrelated implementation changes to force rebuilds across the whole engine.

### Recommended targets

Start with classes that are both widely included and implementation-heavy:

- `Engine/Graphics/Renderer` (largest blast radius, lots of Vulkan types).
- `Engine/Graphics/Device` (VkInstance/VkDevice/queues/surface setup tends to grow).
- Potentially `Engine/Resources/ResourceManager` (caches, async plumbing).

Avoid applying PIMPL to small, hot-path value types where inlining is important and headers are already light.

### Suggested pattern

- Keep the public header minimal:
  - forward-declare `struct Impl;`
  - store `std::unique_ptr<Impl> impl_;`
  - move most private members (and heavy includes) into the `.cpp`
- Provide explicit move operations and out-of-line destructor:
  - out-of-line destructor is required because `Impl` is incomplete in the header.

### Trade-offs

- Pros:
  - fewer transitive includes
  - faster incremental rebuilds
  - smaller blast radius when changing implementation details
- Cons:
  - extra indirection and heap allocation
  - less opportunity for inlining
  - slightly more boilerplate for constructors/move/destructor

### Migration strategy (low risk)

1. Pick one class (recommend `Renderer`) and convert to PIMPL while keeping the public API unchanged.
2. Ensure all callers still build (this often exposes hidden include dependencies, similar to the `FrameInfo.hpp` IWYU work).
3. Only after the pattern is established, apply it to other heavy headers.

## Suggested next step

Now that the quick wins are in place, the best next maintainability ROI is:

1. Extract HZB generation from `Renderer` into a focused `HZBGenerator` (reduce cognitive complexity + isolate Vulkan plumbing).
2. Add a minimal `clang-tidy` runner script/target (changed-files workflow) so the hygiene improvements stick.
