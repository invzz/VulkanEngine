/**
 * @file ImporterFixture.hpp
 * @brief Shared test fixture for model importer tests
 *
 * Provides common setup for finding assets paths and creating temp directories.
 * Both GLTFImporter and OBJImporter tests can inherit from this fixture.
 */

#ifndef VULKANENGINE_TESTS_FIXTURES_IMPORTERFIXTURE_HPP
#define VULKANENGINE_TESTS_FIXTURES_IMPORTERFIXTURE_HPP

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

namespace engine::test {

    /**
 * @brief Base fixture for model importer tests
 *
 * Provides:
 * - Assets path resolution (finds project root and glTF models)
 * - Temp directory management for test files
 * - Common helper methods for creating test model files
 */
    class ImporterFixture : public ::testing::Test {
       protected:
        void SetUp() override {
            assetsPath_ = std::filesystem::current_path();
            while (!std::filesystem::exists(assetsPath_ / "assets" / "models" / "glTF") && assetsPath_.has_parent_path()) {
                assetsPath_ = assetsPath_.parent_path();
            }
            gltfAssetsPath_ = assetsPath_ / "assets" / "models" / "glTF";

            tempDir_ = std::filesystem::temp_directory_path() / "vulkanengine_importer_tests";
            std::filesystem::create_directories(tempDir_);
        }

        void TearDown() override {
            if (std::filesystem::exists(tempDir_)) {
                std::error_code ec;
                std::filesystem::remove_all(tempDir_, ec);
            }
        }

        const std::filesystem::path& gltfAssetsPath() const {
            return gltfAssetsPath_;
        }
        const std::filesystem::path& tempDir() const {
            return tempDir_;
        }
        const std::filesystem::path& projectRoot() const {
            return assetsPath_;
        }

        bool gltfModelExists(const std::string& modelName) const {
            return std::filesystem::exists(gltfAssetsPath_ / modelName / "glTF" / (modelName + ".gltf"));
        }

        std::filesystem::path getGltfModelPath(const std::string& modelName) const {
            return gltfAssetsPath_ / modelName / "glTF" / (modelName + ".gltf");
        }

        std::filesystem::path getGlbModelPath(const std::string& modelName) const {
            return gltfAssetsPath_ / modelName / "glTF-Binary" / (modelName + ".glb");
        }

        std::filesystem::path createTempFile(const std::string& filename, const std::string& content) {
            auto          path = tempDir_ / filename;
            std::ofstream file(path);
            file << content;
            return path;
        }

        std::filesystem::path createInvalidGltfFile() {
            return createTempFile("invalid_test.gltf", "not valid gltf content");
        }

        std::filesystem::path createTriangleObj() {
            return createTempFile("triangle.obj",
                "# Simple triangle\n"
                "v 0.0 0.0 0.0\n"
                "v 1.0 0.0 0.0\n"
                "v 0.5 1.0 0.0\n"
                "vn 0.0 0.0 1.0\n"
                "vt 0.0 0.0\n"
                "vt 1.0 0.0\n"
                "vt 0.5 1.0\n"
                "f 1/1/1 2/2/1 3/3/1\n");
        }

        std::filesystem::path createCubeObj() {
            return createTempFile("cube.obj",
                "# Simple cube\n"
                "v -0.5 -0.5  0.5\n"
                "v  0.5 -0.5  0.5\n"
                "v  0.5  0.5  0.5\n"
                "v -0.5  0.5  0.5\n"
                "v -0.5 -0.5 -0.5\n"
                "v  0.5 -0.5 -0.5\n"
                "v  0.5  0.5 -0.5\n"
                "v -0.5  0.5 -0.5\n"
                "vn  0.0  0.0  1.0\n"
                "vn  0.0  0.0 -1.0\n"
                "vn  0.0  1.0  0.0\n"
                "vn  0.0 -1.0  0.0\n"
                "vn  1.0  0.0  0.0\n"
                "vn -1.0  0.0  0.0\n"
                "f 1
                "f 1
                "f 5
                "f 5
                "f 4
                "f 4
                "f 1
                "f 1
                "f 2
                "f 2
                "f 1
                "f 1
        }

        
        struct ObjWithMaterial {
            std::filesystem::path objPath;
            std::filesystem::path mtlPath;
        };

        ObjWithMaterial createObjWithMaterial() {
            auto mtlPath = createTempFile("with_material.mtl",
                "# Test material\n"
                "newmtl TestMaterial\n"
                "Kd 0.8 0.2 0.2\n"
                "Ks 0.5 0.5 0.5\n"
                "Ns 50.0\n"
                "d 1.0\n"
                "\n"
                "newmtl Chrome\n"
                "Kd 0.8 0.8 0.8\n"
                "Ks 1.0 1.0 1.0\n"
                "Ns 1000.0\n"
                "Pm 1.0\n"
                "Pr 0.02\n");

            auto objPath = createTempFile("with_material.obj",
                "# OBJ with material\n"
                "mtllib with_material.mtl\n"
                "v 0.0 0.0 0.0\n"
                "v 1.0 0.0 0.0\n"
                "v 0.5 1.0 0.0\n"
                "vn 0.0 0.0 1.0\n"
                "usemtl TestMaterial\n"
                "f 1

            return {objPath, mtlPath};
        }

       private:
        std::filesystem::path assetsPath_;
        std::filesystem::path gltfAssetsPath_;
        std::filesystem::path tempDir_;
    };

}  // namespace engine::test

#endif
