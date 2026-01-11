#include <iostream>
#include <memory>
#include <stdexcept>

#include "Engine/Core/ansi_colors.hpp"
#include "ModelLib/Resources/Model.hpp"
#include "ModelLib/importers/GLTFImporter.hpp"
#include "ModelLib/importers/OBJImporter.hpp"

namespace engine {

  std::unique_ptr<Model> Model::createModelFromFile(Device& device, const std::string& filepath, bool flipX, bool flipY, bool flipZ)
  {
    std::cout << "[" << GREEN << "Model" << RESET << "]: Loading model from file: " << filepath << '\n';
    Builder builder;
    builder.loadModelFromFile(filepath, flipX, flipY, flipZ);
    std::cout << "[" << GREEN << "Model" << RESET << "]: " << filepath << " with " << builder.vertices.size() << " vertices " << '\n';
    return std::make_unique<Model>(device, builder);
  }

  void Model::Builder::loadModelFromFile(const std::string& filepath, bool flipX, bool flipY, bool flipZ)
  {
    this->filePath = filepath;
    OBJImporter importer;
    if (!importer.load(*this, filepath, flipX, flipY, flipZ))
    {
      throw std::runtime_error("Failed to load OBJ file: " + filepath);
    }
  }

  std::unique_ptr<Model> Model::createModelFromGLTF(Device& device, const std::string& filepath, bool flipX, bool flipY, bool flipZ)
  {
    std::cout << "[" << GREEN << "Model" << RESET << "]: Loading glTF model from file: " << filepath << '\n';
    Builder builder;
    builder.loadModelFromGLTF(filepath, flipX, flipY, flipZ);
    std::cout << "[" << GREEN << "Model" << RESET << "]: " << filepath << " with " << builder.vertices.size() << " vertices " << '\n';
    return std::make_unique<Model>(device, builder);
  }

  void Model::Builder::loadModelFromGLTF(const std::string& filepath, bool flipX, bool flipY, bool flipZ)
  {
    this->filePath = filepath;
    GLTFImporter importer;
    if (!importer.load(*this, filepath, flipX, flipY, flipZ))
    {
      throw std::runtime_error("Failed to load glTF file: " + filepath);
    }
  }

} // namespace engine
