# Shader Improvements for `pbr_shader.frag`

The current PBR fragment shader is functional and supports multiple workflows (Metallic-Roughness, Specular-Glossiness) and features (Anisotropy, Clearcoat, IBL). However, it can be significantly improved in terms of readability, maintainability, and performance.

## 1. Refactoring & Code Structure

### A. Consolidate Light Loops
**Status:** ✅ Completed
**Implementation:**
- Created `accumulateLight` function to abstract the light accumulation logic.
- Updated `Surface` struct to include clearcoat and anisotropy parameters.
- Updated `getSurfaceProperties` to populate the new `Surface` fields.
- Updated `main` to use `accumulateLight` in all light loops, reducing code duplication.

### B. Extract PBR Math to Functions
**Status:** ✅ Completed
**Implementation:**
- The `calculateDirectLight` and `calculateClearcoat` functions now encapsulate the BRDF math (NDF, Geometry, Fresnel).
- These functions are used by `accumulateLight`, ensuring the logic is centralized and not repeated in loops.

### C. Centralize Material Property Extraction
**Status:** ✅ Completed
**Implementation:**
- Created `Surface` struct to hold all processed material data.
- Implemented `getSurfaceProperties()` to handle UV scaling, texture sampling, workflow conversion, and normal mapping.
- `main()` now follows the clean sequence: `getSurfaceProperties` -> `accumulateLight` (in loops) -> `applyIBL` -> `finalize`.

## 2. Performance Optimizations

### A. Minimize Branching
**Status:** ✅ Completed
**Implementation:**
- Updated `DistributionGGXAnisotropic` to correctly use `alpha = roughness * roughness`, ensuring it mathematically reduces to standard GGX when anisotropy is 0.
- Removed the `if (surf.anisotropy > 0.01)` branch in `calculateDirectLight` and now always use the unified `DistributionGGXAnisotropic` function.

### B. Precompute View-Dependent Terms
**Status:** ✅ Completed
**Implementation:**
- Added `NdotV` and `R` (reflection vector) to the `Surface` struct.
- Calculated these terms once in `getSurfaceProperties`.
- Updated `calculateDirectLight`, `calculateClearcoat`, and `calculateIBL` to use the precomputed values, avoiding redundant calculations.

### C. Light Culling
**Current State:** Simple distance-based culling is present (`distance2 > intensity * 250.0`).
**Improvement:**
- Ensure this culling logic is robust.
- Consider moving light culling to a Compute Shader (tiled/clustered rendering) in the future to avoid iterating over 16+ lights per fragment if most are far away.

## 3. Feature & Visual Improvements

### A. Move Tone Mapping & Gamma Correction
**Status:** ✅ Completed
**Implementation:**
- Tone mapping (ACES Filmic) and Gamma correction are now performed in `post_process.frag`.
- The PBR shader outputs linear HDR color to an offscreen buffer.
- The PostProcessingSystem applies the effects during the final composition pass.

### B. Improve IBL
**Status:** ✅ Completed
**Implementation:**
- Added horizon occlusion term to prevent light leaking from below the surface.
- Updated specular IBL calculation to correctly use the pre-integrated DFG LUT (`brdfLUT`) for proper energy conservation.

### C. Debugging Modes
**Status:** ✅ Completed
**Implementation:**
- Added `debugMode` to `GlobalUbo` (C++) and `UBO` (GLSL).
- Implemented debug visualization logic in `pbr_shader.frag` (Albedo, Normal, Roughness, Metallic, Lighting, AO).
- Added `DebugPanel` to the UI to switch between debug modes.

## 4. Proposed Architecture

```glsl
struct Surface {
    vec3 P;          // World Position
    vec3 N;          // World Normal
    vec3 V;          // View Vector
    vec3 albedo;
    float roughness;
    float metallic;
    float ao;
    vec3 F0;
    // ... other params (anisotropy, clearcoat)
};

vec3 calculateDirectLight(Surface surf, vec3 L, vec3 radiance) {
    // Standard PBR logic
}

void main() {
    Surface surf = getSurfaceProperties();
    
    vec3 Lo = vec3(0.0);
    
    // Single loop over all lights
    for (int i = 0; i < lightCount; ++i) {
        Light light = getLight(i);
        vec3 L = normalize(light.pos - surf.P);
        vec3 radiance = calculateRadiance(light, surf.P);
        
        Lo += calculateDirectLight(surf, L, radiance);
        
        if (surf.hasClearcoat) {
            Lo += calculateClearcoat(surf, L, radiance);
        }
    }
    
    vec3 ambient = calculateIBL(surf);
    vec3 color = ambient + Lo + surf.emissive;
    
    outColor = vec4(color, surf.alpha);
}
```
