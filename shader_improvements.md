# Shader Improvements for `pbr_shader.frag`

The current PBR fragment shader is functional and supports multiple workflows (Metallic-Roughness, Specular-Glossiness) and features (Anisotropy, Clearcoat, IBL). However, it can be significantly improved in terms of readability, maintainability, and performance.

## 1. Refactoring & Code Structure

### A. Consolidate Light Loops
**Current State:** The shader iterates over point, directional, and spot lights separately. Later, it iterates over them *again* for the clearcoat layer.
**Improvement:**
- Create a unified `Light` struct or helper functions to abstract the light type differences.
- Iterate over all lights **once**.
- Inside the loop, calculate both the Base PBR contribution and the Clearcoat contribution.
- This reduces the overhead of loop control and repeated memory accesses (e.g., fetching light data).

### B. Extract PBR Math to Functions
**Current State:** The BRDF math (NDF, Geometry, Fresnel) is repeated inside each light loop (and again for clearcoat).
**Improvement:**
- Create a function `calculatePBR(vec3 N, vec3 V, vec3 L, vec3 radiance, ...)` that returns the calculated `Lo` (radiance out) for a single light.
- This function can handle the `NDF * G * F / denominator` logic centrally.

### C. Centralize Material Property Extraction
**Current State:** Texture sampling and parameter logic (like the Specular-Glossiness to Metallic-Roughness conversion) are mixed into `main()`.
**Improvement:**
- Create a `SurfaceProperties` struct to hold all processed material data (albedo, roughness, metallic, F0, normal, etc.).
- Create a function `getSurfaceProperties()` that handles:
    - UV scaling.
    - Texture sampling (checking flags).
    - Workflow conversion (Spec/Gloss -> Metal/Rough).
    - Normal mapping.
- `main()` becomes a clean sequence of: `getSurfaceProperties` -> `accumulateLights` -> `applyIBL` -> `finalize`.

## 2. Performance Optimizations

### A. Minimize Branching
**Current State:** There are checks like `if (material.anisotropic > 0.01)` inside the inner loops.
**Improvement:**
- While modern GPUs handle uniform branching well, complex nested branching can still be costly.
- For features like anisotropy, consider if the branch is worth it or if a unified math model (where anisotropy=0 reduces to standard GGX) is cheaper than the branch overhead.
- Alternatively, use specialization constants (if moving to a more advanced Vulkan pipeline) to compile different shader permutations.

### B. Precompute View-Dependent Terms
**Current State:** Terms like `NdotV` are calculated repeatedly or passed around.
**Improvement:**
- Calculate `NdotV`, `R` (reflection vector), and other view-dependent terms once at the start of `main` (or after normal mapping) and pass them to lighting functions.

### C. Light Culling
**Current State:** Simple distance-based culling is present (`distance2 > intensity * 250.0`).
**Improvement:**
- Ensure this culling logic is robust.
- Consider moving light culling to a Compute Shader (tiled/clustered rendering) in the future to avoid iterating over 16+ lights per fragment if most are far away.

## 3. Feature & Visual Improvements

### A. Move Tone Mapping & Gamma Correction
**Current State:** Reinhard tone mapping and Gamma correction are applied at the end of the fragment shader.
**Improvement:**
- **Move to Post-Processing:** Tone mapping and gamma correction should ideally happen in a separate full-screen pass (or compute pass).
- **Why?**
    - Allows for other post-process effects (Bloom, Depth of Field, Motion Blur) to work on linear HDR data *before* tone mapping.
    - Reinhard is a simple operator; more cinematic operators like ACES Filmic are preferred for realistic PBR.

### B. Improve IBL
**Current State:** IBL is added at the end.
**Improvement:**
- Ensure the IBL calculation properly accounts for the Fresnel term and horizon occlusion (to prevent light leaking from below the surface).
- Add support for a pre-integrated DFG LUT (which seems to be `brdfLUT` in the code) for better energy conservation.

### C. Debugging Modes
**Current State:** Debugging is done by commenting out code.
**Improvement:**
- Add a `debugMode` uniform (or push constant).
- Allow switching output to visualize: Albedo, Normal, Roughness, Metallic, Lighting Only, etc.

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
