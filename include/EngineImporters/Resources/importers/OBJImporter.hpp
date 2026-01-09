#ifndef VULKANENGINE_INCLUDE_ENGINE_RESOURCES_IMPORTERS_OBJIMPORTER_HPP
#define VULKANENGINE_INCLUDE_ENGINE_RESOURCES_IMPORTERS_OBJIMPORTER_HPP

#include "ModelImporter.hpp"

namespace engine {

  /**
   * @brief Importer for Wavefront OBJ files with MTL material support
   */
  class OBJImporter : public ModelImporter
  {
  public:
    bool load(Model::Builder& builder, const std::string& filepath, bool flipX, bool flipY, bool flipZ) override;

    [[nodiscard]] std::vector<std::string> getSupportedExtensions() const override { return {"obj"}; }

    [[nodiscard]] std::string getName() const override { return "OBJ Importer"; }
  };

} // namespace engine

#endif // VULKANENGINE_INCLUDE_ENGINE_RESOURCES_IMPORTERS_OBJIMPORTER_HPP
