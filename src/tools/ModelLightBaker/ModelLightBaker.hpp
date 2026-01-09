#pragma once

#include <glm/glm.hpp>
#include <string>
#include <vector>

#include "Engine/Graphics/Device.hpp"
#include "Engine/Resources/Model.hpp"

namespace ModelLightBaker {

  struct Options
  {
    enum class Mode
    {
      TEXEL,
      VERTEX,
      MESH
    };

    int       resolution   = 512;
    int       samples      = 16; // multisample for soft penumbra
    Mode      mode         = Mode::TEXEL;
    glm::vec3 sunDir       = glm::vec3(0.0f, -1.0f, 0.0f);
    float     sunIntensity = 1.0f;

    // Sampling & BVH tolerances
    // sampleEpsilonExponent is stored as exponent (e.g. -6 => 1e-6) for convenient CLI parsing
    int   sampleEpsilonExponent = -6;    // default 1e-6
    float bvhPaddingFraction    = 0.02f; // fraction of scene extent to pad BVH bounds

    // Preview options
    bool preview        = false;
    int  previewMaxSize = 512; // max width/height for preview PNG

    // GPU options
    bool gpu = false; // use GPU compute shader raycasts

    // Packaging options
    bool packToVTEX = false; // write a VTEX container from the EXR after baking

    // Automatic per-mesh chunking tile size in meters (used when running in MESH mode)
    float meshChunkSize = 8.0f; // default tile size (meters) for automatic chunking
  };

  class ModelLightBaker
  {
  public:
    ModelLightBaker(engine::Device& device, engine::Model& model, const Options& opts, const std::string& outDir);

    // Perform the bake offline (CPU-driven sampling + optional GPU acceleration)
    void bake();

    // Save bake outputs and manifest to disk
    bool saveToDisk();

  private:
    engine::Device& device_;
    engine::Model&  model_;
    Options         opts_;
    std::string     outDir_;

    // HDR image buffer (RGB float) used by the baker (width = height = opts_.resolution)
    std::vector<float> hdrPixels_; // size = w * h * 3
    int                imageWidth_  = 0;
    int                imageHeight_ = 0;

    // Preview options
    bool previewEnabled_ = false;
    int  previewMaxSize_ = 512; // max width/height for preview PNG

    struct MeshLightmapInfo
    {
      int         meshIndex;
      int         chunkX;
      int         chunkZ;
      std::string file;
      int         width;
      int         height;
    };
    std::vector<MeshLightmapInfo> meshLightmaps_; // populated when running in MESH mode

    // TODO: add BVH and bake buffers here

    // Save a small LDR preview PNG for quick validation
    bool savePreviewPNG(const std::string& outDir, const std::string& baseName) const;
  };

} // namespace ModelLightBaker
