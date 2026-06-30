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
            // Find project root by looking for assets/models/glTF
            assetsPath_ = std::filesystem::current_path();
            while (!std::filesystem::exists(assetsPath_ / "assets" / "models" / "glTF") && assetsPath_.has_parent_path()) {
                assetsPath_ = assetsPath_.parent_path();
            }
            gltfAssetsPath_ = assetsPath_ / "assets" / "models" / "glTF";

            // Create temp directory for test files
            tempDir_ = std::filesystem::temp_directory_path() / "vulkanengine_importer_tests";
            std::filesystem::create_directories(tempDir_);
        }

        void TearDown() override {
            // Clean up temp directory
            if (std::filesystem::exists(tempDir_)) {
                std::error_code ec;
                std::filesystem::remove_all(tempDir_, ec);
            }
        }

        // Path accessors
        const std::filesystem::path& gltfAssetsPath() const {
            return gltfAssetsPath_;
        }
        const std::filesystem::path& tempDir() const {
            return tempDir_;
        }
        const std::filesystem::path& projectRoot() const {
            return assetsPath_;
        }

        // Helper to check if a glTF model exists
        bool gltfModelExists(const std::string& modelName) const {
            return std::filesystem::exists(gltfAssetsPath_ / modelName / "glTF" / (modelName + ".gltf"));
        }

        // Helper to get glTF model path
        std::filesystem::path getGltfModelPath(const std::string& modelName) const {
            return gltfAssetsPath_ / modelName / "glTF" / (modelName + ".gltf");
        }

        // Helper to get GLB model path
        std::filesystem::path getGlbModelPath(const std::string& modelName) const {
            return gltfAssetsPath_ / modelName / "glTF-Binary" / (modelName + ".glb");
        }

        // Helper to create a temp file with content
        std::filesystem::path createTempFile(const std::string& filename, const std::string& content) {
            auto          path = tempDir_ / filename;
            std::ofstream file(path);
            file << content;
            return path;
        }

        // Helper to create an invalid glTF file for error testing
        std::filesystem::path createInvalidGltfFile() {
            return createTempFile("invalid_test.gltf", "not valid gltf content");
        }

        // Helper to create a simple triangle OBJ
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

        // Helper to create a simple cube OBJ
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
                "f 1//1 2//1 3//1\n"
                "f 1//1 3//1 4//1\n"
                "f 5//2 7//2 6//2\n"
                "f 5//2 8//2 7//2\n"
                "f 4//3 3//3 7//3\n"
                "f 4//3 7//3 8//3\n"
                "f 1//4 6//4 2//4\n"
                "f 1//4 5//4 6//4\n"
                "f 2//5 6//5 7//5\n"
                "f 2//5 7//5 3//5\n"
                "f 1//6 4//6 8//6\n"
                "f 1//6 8//6 5//6\n");
        }

        // Helper to create an OBJ with MTL material
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
                "f 1//1 2//1 3//1\n");

            return {objPath, mtlPath};
        }

       private:
        std::filesystem::path assetsPath_;
        std::filesystem::path gltfAssetsPath_;
        std::filesystem::path tempDir_;
    };

}  // namespace engine::test

#endif  // VULKANENGINE_TESTS_FIXTURES_IMPORTERFIXTURE_HPP
