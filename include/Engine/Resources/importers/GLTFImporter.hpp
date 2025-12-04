#pragma once

#include "ModelImporter.hpp"

namespace engine {

  /**
   * @brief Importer for glTF 2.0 files (.gltf and .glb binary format)
   */
  class GLTFImporter : public ModelImporter
  {
  public:
    bool load(Model::Builder& builder, const std::string& filepath, bool flipX, bool flipY, bool flipZ) override;

    std::vector<std::string> getSupportedExtensions() const override { return {"gltf", "glb"}; }

    std::string getName() const override { return "glTF Importer"; }
  };

} // namespace engine
