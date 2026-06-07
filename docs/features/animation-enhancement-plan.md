# Animation System Enhancement Plan

## Goal
Upgrade the animation system from single-clip playback to a multi-clip controller with blending, animation events, and proper layering — enabling complex character animation (idle + walk + wave simultaneously) without breaking the existing pipeline.

## Current State Analysis

### What exists
- **Skeletal animation**: glTF node transforms interpolated per-frame (translation/rotation/scale)
- **Morph targets**: GPU compute-based blend shapes (VertexShaderBlendCompute)
- **AnimationComponent**: `currentAnimationIndex`, `currentTime`, `playbackSpeed`, `isPlaying`, `loop`
- **AnimationPanel**: Basic play/pause + speed slider per entity
- **Interpolation**: Linear and step (CUBIC_SPLINE struct exists but not used in CPU path)

### What's missing
1. **One-clip-per-entity ceiling** — `currentAnimationIndex` is a single int, no blending
2. **No crossfade/transitions** — switching clips is instant (hard cut)
3. **No animation layers** — can't play walk + wave simultaneously
4. **No animation events** — no callbacks at specific timestamps (footstep sounds, attack triggers)
5. **No animation state machine** — no way to chain clips conditionally
6. **Timeline UI** — no visual feedback in editor

## Design Decisions

### 1. `AnimationClip` — lightweight playback handle
- Wraps a single clip from the model
- Tracks its own playback time, speed, weight, blend direction
- Does NOT own the Model — it references a clip by index
- Copyable, cheap to create/destroy

### 2. `AnimationController` — multi-clip manager
- Manages a set of active `AnimationClip` instances
- Each clip has a `priority` (0-100); higher priority overrides lower on conflicting bones
- Blending modes:
  - **Additive**: result = base + (clip - base) * weight
  - **Override**: clip fully replaces lower-priority bone transforms
  - **Crossfade**: time-based interpolation between old and new clip
- Computes final bone transforms by combining all active clips

### 3. `AnimationComponent` — thin wrapper
- Holds a `std::shared_ptr<AnimationController>` instead of direct clip state
- Exposes convenience methods: `addClip()`, `play()`, `stop()`, `blendTo()`, `setSpeed()`
- Maintains backward compatibility: `play(animIndex)` still works as a convenience

### 4. Animation Events
- Each clip has a list of events: `{time, name, userData}`
- `AnimationController::update()` fires events whose time <= currentTime
- Callback type: `std::function<void(const std::string& eventName, void* userData)>`

### 5. No runtime graph editor (yet)
- State machines and graphs are Phase 3
- This phase is purely about runtime capabilities: multiple clips, blending, events

## File Structure

```
include/Engine/Scene/Components/
  AnimationComponent.hpp          ← modified: holds AnimationController
  AnimationClip.hpp               ← NEW: single clip handle
  AnimationController.hpp         ← NEW: multi-clip manager + blending

src/Engine/Systems/
  AnimationSystem.cpp             ← modified: uses AnimationController

src/Editor/ui/
  AnimationPanel.cpp              ← modified: clip list, crossfade UI

tests/
  AnimationControllerTests.cpp    ← NEW: unit tests for blending
```

## Phase 1 — AnimationController + Blending (2-3 days)

### 1.1 `AnimationClip.hpp` — Clip handle
```cpp
struct AnimationClip {
    int clipIndex;          // Index into Model::animations
    std::string name;       // Human-readable name
    float duration;         // In seconds

    // Playback state
    float currentTime = 0.0f;
    float speed = 1.0f;
    float weight = 1.0f;    // 0-1, for fade in/out
    bool loop = true;
    bool active = false;

    // Blending
    enum Mode { OVERRIDE, ADDITIVE };
    Mode mode = OVERRIDE;
    int priority = 0;       // Higher = wins on bone conflicts

    // Events
    struct Event {
        float time;
        std::string name;
        void* userData = nullptr;
    };
    std::vector<Event> events;
    size_t nextEventIndex = 0;

    // Methods
    void reset();
    void step(float deltaTime, const Model& model, std::vector<glm::vec3>& translations,
              std::vector<glm::quat>& rotations, std::vector<glm::vec3>& scales);
};
```

### 1.2 `AnimationController.hpp` — Multi-clip manager
```cpp
class AnimationController {
public:
    using EventCallback = std::function<void(const std::string& name, void* userData)>;

    void addClip(int clipIndex, const Model& model, int priority = 0);
    void removeClip(int clipIndex);
    void setClipWeight(int clipIndex, float weight);
    void setClipSpeed(int clipIndex, float speed);
    void play(int clipIndex, const Model& model);
    void stop(int clipIndex);
    void stopAll();
    void reset();

    // Core update — fires events, blends bones
    void update(float deltaTime, const Model& model, std::vector<glm::vec3>& outTranslations,
                std::vector<glm::quat>& outRotations, std::vector<glm::vec3>& outScales);

    // Query
    std::vector<std::pair<int, AnimationClip>>& getActiveClips();
    const std::vector<glm::mat4>& computeGlobalTransforms(const Model& model);
    bool hasActiveClips() const;

    // Events
    void setEventCallback(EventCallback cb);
    std::vector<std::pair<std::string, void*>> takeEvents(); // drain fired events

private:
    std::vector<AnimationClip> clips_;
    EventCallback eventCallback_;
    std::vector<std::string> firedEvents_; // dedup buffer
};
```

### 1.3 Blending Strategy
- **Default: OVERRIDE** — each bone gets the transform from the highest-priority active clip
- **ADDITIVE** — result = baseTransform + (clipTransform - bindPoseTransform) * weight
  - Base = lowest-priority clip (or default bind pose)
  - Used for: secondary motion (breathing, head looking around), layered animations

### 1.4 `AnimationComponent` changes
```cpp
struct AnimationComponent {
    std::shared_ptr<Model> model;
    std::shared_ptr<AnimationController> controller; // NEW

    // Backward-compatible convenience (keeps existing code working)
    void play(int animationIndex = 0, bool shouldLoop = true) {
        if (!model || !controller) return;
        controller->play(animationIndex, *model);
    }
    void stop() {
        controller->stopAll();
    }
};
```

### 1.5 `AnimationSystem` changes
- `updateAnimations()` calls `controller->update(deltaTime, model, translations, rotations, scales)`
- Removes per-clip state management from the system
- System becomes a dispatcher, not a state holder

## Phase 2 — Timeline UI (completed incrementally)

### 2.1 Timeline widget ✅
- Horizontal bar with grid lines (5s, 1s, 0.25s, 0.1s step based on zoom)
- Playhead indicator (blue line with triangle marker)
- Time labels at grid intervals
- Click-to-scrub (drag playhead)
- Zoom In / Zoom Out buttons
- Auto-calculated duration from active clips

### 2.2 Animated clip tracks ✅
- Per-entity tracks rendered on timeline bar
- Colored bars showing clip duration and position
- Highlighted track for selected clip
- Keyframe markers (diamond shape) displayed on tracks

### 2.3 Clip list ✅
- Per-entity section with expandable tree
- Active clips listed by name
- Playhead time display per entity
- Clip name as selectable row
- Per-clip weight slider (DragFloat 0-1)
- Per-clip speed slider (DragFloat 0.01-5)
- Per-clip crossfade duration slider (0-10s)
- Tooltip showing weight/speed/crossfade on hover

### 2.4 Playback controls ✅
- "▶ Play All" button — stops all clips, plays selected clip per entity
- "⏹ Stop All" button — stops all clips per entity

### Build & Test
- Build: Green
- Tests: 23/23 animation tests pass
- Window tests fail due to headless environment (pre-existing)

## Phase 3 — Animation Graph

### 3.1 Graph core ✅
- `AnimationGraphNode` — nodes representing clips or states (entry/exit/blend)
- `AnimationTransition` — directed edges with conditions (NONE, TIME_BASED, EVENT_BASED, PARAM_BASED, BLEND_COMPLETE)
- `AnimationGraph` — state machine with node/transition management
  - `addNode()` — create nodes with entry/exit flags
  - `addTransition()` — define transitions with blend duration, condition type, threshold
  - `getNode()` / `getNodeByName()` — lookup
  - `getTransitions()` / `getDefaultTransition()` — query outgoing edges
  - `evaluateNextTransition()` — condition evaluation
  - `step()` — advance time, trigger time-based transitions
  - `getRequiredEvents()` — collect event names from transitions
  - `setCurrentNode()` — manual node switching

### 3.2 Editor UI — Graph Editor widget (future)
- Node-based graph view in ImGui
- Add/remove nodes and transitions
- Visual connections between nodes (colored lines)
- Inline property editing for transition conditions
- Play/pause/step preview within editor

### Build & Test
- Build: Green
- Tests: 10/10 AnimationGraph tests pass, 33/33 total animation tests

## Verification Checklist

### Phase 1
- [ ] `AnimationClip` compiles independently
- [ ] `AnimationController` unit tests pass (play/stop/blend/events)
- [ ] `AnimationComponent` compiles with `shared_ptr<AnimationController>`
- [ ] `AnimationSystem::updateAnimations()` uses controller
- [ ] Build succeeds (xmake)
- [ ] All existing tests pass (604/604)
- [ ] Manual test: load a model with animations, play two clips simultaneously, verify blending

### Phase 2
- [ ] Timeline widget renders
- [ ] Scrubbing updates animation time
- [ ] Clip list shows all loaded animations
- [ ] Crossfade UI works
- [ ] Build succeeds
- [ ] All tests pass

## Risks & Mitigations

| Risk | Mitigation |
|------|-----------|
| Breaking existing code that uses `AnimationComponent` directly | Keep `play()`, `stop()`, `currentTime`, `isPlaying` as thin wrappers over the controller |
| Performance hit from multiple clips | Only compute blended transforms for clips that are active; early-out if only one clip |
| Blending artifacts on skeletal rigs | Default OVERRIDE mode is safe (no blending); ADDITIVE is opt-in |
| Model lifecycle — model destroyed while clip active | Controller holds `weak_ptr` to model, checks validity in update |

## Implementation Order

1. `AnimationClip.hpp` — standalone, no deps on AnimationController
2. `AnimationController.hpp` — uses AnimationClip
3. `AnimationClip.cpp` + `AnimationController.cpp` — implementation
4. Update `AnimationComponent.hpp` — add controller field, convenience methods
5. Update `AnimationSystem.cpp` — use controller in update path
6. Tests — `AnimationControllerTests.cpp`
7. Build + test verification
