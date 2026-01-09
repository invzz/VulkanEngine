# Custom C++ Engine Lightmap Baking -- Detailed Planning Document

## 0. Goals Recap

-   Export engine scenes into a portable **Scene.json**
-   Bake **static lighting for static objects**
-   Support **GLTF instancing**
-   Build a **separate offline LightMapBaker tool**
-   Inject baked lightmap data back into the scene
-   Runtime engine interprets lightmaps if present

------------------------------------------------------------------------

## 1. Core Architectural Principles

### 1.1 Separation of Responsibilities

  System           Responsibility
  ---------------- ----------------------------------------------
  Runtime Engine   Rendering, dynamic lights, loading lightmaps
  Scene.json       Authoritative scene description
  GLTF             Geometry + materials only
  LightMapBaker    Offline static lighting computation

------------------------------------------------------------------------

## 2. Scene Export System

### 2.1 Scene.json Responsibilities

-   Own **object instances**
-   Own **light definitions**
-   Reference assets (GLTF, textures)
-   Store **bake metadata**

### 2.2 Scene.json High-Level Schema

``` json
{
  "assets": {},
  "objects": [],
  "lights": [],
  "lightmaps": []
}
```

### 2.3 Object Definition

``` json
{
  "id": "object_01",
  "mesh": "mesh_01",
  "material": "mat_01",
  "transform": {},
  "lighting": {
    "mobility": "static"
  }
}
```

------------------------------------------------------------------------

## 3. Static vs Dynamic System

### 3.1 Mobility Enum

``` cpp
enum class Mobility {
    Static,
    Movable
};
```

### 3.2 Bake Rules

  Element   Static          Movable
  --------- --------------- ------------------
  Objects   Lightmapped     Runtime lighting
  Lights    Baked           Runtime
  Sky       Baked via IBL   Optional

------------------------------------------------------------------------

## 4. GLTF Instancing Strategy

### 4.1 Problem

-   GLTF meshes are shared
-   Lightmaps require **unique UV space per instance**

### 4.2 Solution

-   Lightmap data is **per instance**
-   Mesh geometry shared
-   Lightmap UVs generated per object instance

------------------------------------------------------------------------

## 5. Lightmap UV Generation

### 5.1 UV Channels

  Channel   Usage
  --------- -------------------
  UV0       Material textures
  UV1       Lightmaps

### 5.2 UV Generation Flow

1.  Clone mesh topology per instance
2.  Apply world transform
3.  Generate UV charts
4.  Pack charts into atlas space

### 5.3 Tooling

-   **xatlas** (recommended)
-   Padding: 4--16 pixels depending on resolution

------------------------------------------------------------------------

## 6. Lightmap Atlas System

### 6.1 Atlas Strategy

-   Multiple atlases allowed
-   Typical sizes: 1024², 2048²

### 6.2 Per-Object Mapping Data

``` json
{
  "lightmap": {
    "texture": "lm_0.vtex",
    "uvScale": [0.25, 0.25],
    "uvOffset": [0.5, 0.0]
  }
}
```

------------------------------------------------------------------------

## 7. LightMapBaker Tool

### 7.1 Tool Inputs

-   Scene.json
-   GLTF assets
-   Bake settings

### 7.2 Tool Outputs

-   Lightmap textures
-   Updated Scene.json

------------------------------------------------------------------------

## 8. Baking Pipeline

### 8.1 Scene Load Phase

-   Parse Scene.json
-   Resolve static objects
-   Resolve static lights

### 8.2 Geometry Build Phase

-   Expand instances
-   Generate UV1
-   Assign atlas space

### 8.3 Lighting Phase

#### Initial Version

-   Direct lighting
-   Shadow rays

#### Advanced

-   Multi-bounce GI
-   Sky lighting
-   Importance sampling

------------------------------------------------------------------------

## 9. Scene Augmentation

### 9.1 Lightmap Registry

``` json
"lightmaps": [
  {
    "id": "lm_0",
    "file": "lightmaps/lm_0.vtex",
    "resolution": 2048
  }
]
```

------------------------------------------------------------------------

## 10. Runtime Integration

### 10.1 Renderer Logic

-   If object has lightmap:
    -   Bind lightmap texture
    -   Use UV1
-   Else:
    -   Skip baked lighting

### 10.2 Shader Logic

``` glsl
vec3 baked = texture(lightmap, lmUV).rgb;
finalColor = baked + dynamicLighting;
```

------------------------------------------------------------------------

## 11. Incremental Development Roadmap

### Phase 1 -- Foundations

-   Scene export
-   Mobility flags
-   Baker scene loading

### Phase 2 -- Baking

-   UV unwrapping
-   Atlas packing
-   Direct lighting bake

### Phase 3 -- Quality

-   GI bounces
-   Denoising
-   Sky lighting

------------------------------------------------------------------------

## 12. Common Pitfalls

-   Writing lightmap UVs into GLTF ❌
-   Baking per-mesh instead of per-instance ❌
-   Missing atlas padding ❌
-   No transform baking ❌

------------------------------------------------------------------------

## 13. Future Extensions

-   Light probes
-   Volumetric lightmaps
-   Cascaded GI
-   Incremental rebaking

------------------------------------------------------------------------

**End of Planning Document**
