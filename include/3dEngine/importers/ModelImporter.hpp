#pragma once

#include <string>
#include <vector>

#include "3dEngine/Model.hpp"

namespace engine {

  /**
   * @brief Abstract base class for model importers
   *
   * Different file formats (OBJ, glTF, FBX, etc.) can implement this interface
   * to provide a unified way to load 3D models into our Model::Builder structure.
   */
  class ModelImporter
  {
  public:
    virtual ~ModelImporter() = default;

    /**
     * @brief Load a model from a file into the provided builder
     *
     * @param builder The Model::Builder to populate with geometry and materials
     * @param filepath Path to the model file
     * @param flipX Flip X-axis coordinates
     * @param flipY Flip Y-axis coordinates
     * @param flipZ Flip Z-axis coordinates
     * @return true if loading succeeded, false otherwise
     */
    virtual bool load(Model::Builder& builder, const std::string& filepath, bool flipX, bool flipY, bool flipZ) = 0;

    /**
     * @brief Get supported file extensions for this importer
     * @return Vector of file extensions (e.g., {"obj", "mtl"})
     */
    virtual std::vector<std::string> getSupportedExtensions() const = 0;

    /**
     * @brief Get the name of this importer
     * @return Name string (e.g., "OBJ Importer", "glTF Importer")
     */
    virtual std::string getName() const = 0;
  };

} // namespace engine
