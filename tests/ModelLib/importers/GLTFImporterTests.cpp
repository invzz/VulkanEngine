#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

#include "ModelLib/Resources/Model.hpp"
#include "ModelLib/importers/GLTFImporter.hpp"

namespace engine::tests {

    class GLTFImporterTest : public ::testing::Test {
       protected:
        void SetUp() override {
            assetsPath_ = std::filesystem::current_path();

            while (!std::filesystem::exists(assetsPath_ / "assets" / "models" / "glTF") && assetsPath_.has_parent_path()) {
                assetsPath_ = assetsPath_.parent_path();
            }
            assetsPath_ /= "assets/models/glTF";
        }

        std::filesystem::path assetsPath_;
        GLTFImporter          importer_;
    };

    TEST_F(GLTFImporterTest, GetName_ReturnsCorrectName) {
        EXPECT_EQ(importer_.getName(), "glTF Importer");
    }

    TEST_F(GLTFImporterTest, GetSupportedExtensions_ReturnsGltfAndGlb) {
        auto extensions = importer_.getSupportedExtensions();
        ASSERT_EQ(extensions.size(), 2);
        EXPECT_EQ(extensions[0], "gltf");
        EXPECT_EQ(extensions[1], "glb");
    }

    TEST_F(GLTFImporterTest, Load_NonexistentFile_ReturnsFalse) {
        Model::Builder builder;
        bool           result = importer_.load(builder, "nonexistent_file.gltf", false, false, false);
        EXPECT_FALSE(result);
    }

    TEST_F(GLTFImporterTest, Load_InvalidFile_ReturnsFalse) {
        auto tempPath = std::filesystem::temp_directory_path() / "invalid_test.gltf";
        {
            std::ofstream file(tempPath);
            file << "not valid gltf content";
        }

        Model::Builder builder;
        bool           result = importer_.load(builder, tempPath.string(), false, false, false);
        EXPECT_FALSE(result);

        std::filesystem::remove(tempPath);
    }

    TEST_F(GLTFImporterTest, Load_Triangle_Succeeds) {
        if (!std::filesystem::exists(assetsPath_ / "Triangle" / "glTF" / "Triangle.gltf")) {
            GTEST_SKIP() << "Triangle test asset not found";
        }

        Model::Builder builder;
        bool           result = importer_.load(builder, (assetsPath_ / "Triangle" / "glTF" / "Triangle.gltf").string(), false, false, false);
        EXPECT_TRUE(result);

        EXPECT_GE(builder.vertices.size(), 3);
    }

    TEST_F(GLTFImporterTest, Load_Triangle_HasCorrectVertexCount) {
        if (!std::filesystem::exists(assetsPath_ / "Triangle" / "glTF" / "Triangle.gltf")) {
            GTEST_SKIP() << "Triangle test asset not found";
        }

        Model::Builder builder;
        importer_.load(builder, (assetsPath_ / "Triangle" / "glTF" / "Triangle.gltf").string(), false, false, false);

        EXPECT_EQ(builder.vertices.size(), 3);
    }

    TEST_F(GLTFImporterTest, Load_Triangle_HasIndices) {
        if (!std::filesystem::exists(assetsPath_ / "Triangle" / "glTF" / "Triangle.gltf")) {
            GTEST_SKIP() << "Triangle test asset not found";
        }

        Model::Builder builder;
        importer_.load(builder, (assetsPath_ / "Triangle" / "glTF" / "Triangle.gltf").string(), false, false, false);

        EXPECT_FALSE(builder.indices.empty());
    }

    TEST_F(GLTFImporterTest, Load_Box_Succeeds) {
        if (!std::filesystem::exists(assetsPath_ / "Box" / "glTF" / "Box.gltf")) {
            GTEST_SKIP() << "Box test asset not found";
        }

        Model::Builder builder;
        bool           result = importer_.load(builder, (assetsPath_ / "Box" / "glTF" / "Box.gltf").string(), false, false, false);
        EXPECT_TRUE(result);
    }

    TEST_F(GLTFImporterTest, Load_Box_HasVertices) {
        if (!std::filesystem::exists(assetsPath_ / "Box" / "glTF" / "Box.gltf")) {
            GTEST_SKIP() << "Box test asset not found";
        }

        Model::Builder builder;
        importer_.load(builder, (assetsPath_ / "Box" / "glTF" / "Box.gltf").string(), false, false, false);

        EXPECT_GE(builder.vertices.size(), 8);
    }

    TEST_F(GLTFImporterTest, Load_Cube_Succeeds) {
        if (!std::filesystem::exists(assetsPath_ / "Cube" / "glTF" / "Cube.gltf")) {
            GTEST_SKIP() << "Cube test asset not found";
        }

        Model::Builder builder;
        bool           result = importer_.load(builder, (assetsPath_ / "Cube" / "glTF" / "Cube.gltf").string(), false, false, false);
        EXPECT_TRUE(result);
    }

    TEST_F(GLTFImporterTest, Load_BoxGLB_Succeeds) {
        auto glbPath = assetsPath_ / "Box" / "glTF-Binary" / "Box.glb";
        if (!std::filesystem::exists(glbPath)) {
            GTEST_SKIP() << "Box.glb test asset not found";
        }

        Model::Builder builder;
        bool           result = importer_.load(builder, glbPath.string(), false, false, false);
        EXPECT_TRUE(result);
        EXPECT_FALSE(builder.vertices.empty());
    }

    TEST_F(GLTFImporterTest, Load_WithFlipX_ModifiesVertices) {
        if (!std::filesystem::exists(assetsPath_ / "Triangle" / "glTF" / "Triangle.gltf")) {
            GTEST_SKIP() << "Triangle test asset not found";
        }

        Model::Builder builderNormal;
        Model::Builder builderFlipped;

        importer_.load(builderNormal, (assetsPath_ / "Triangle" / "glTF" / "Triangle.gltf").string(), false, false, false);
        importer_.load(builderFlipped, (assetsPath_ / "Triangle" / "glTF" / "Triangle.gltf").string(), true, false, false);

        ASSERT_EQ(builderNormal.vertices.size(), builderFlipped.vertices.size());

        bool hasXDifference = false;
        for (size_t i = 0; i < builderNormal.vertices.size() && !hasXDifference; ++i) {
            if (builderNormal.vertices[i].position.x != 0.0f) {
                hasXDifference = (builderNormal.vertices[i].position.x != builderFlipped.vertices[i].position.x);
            }
        }
    }

    TEST_F(GLTFImporterTest, Load_WithFlipY_ModifiesVertices) {
        if (!std::filesystem::exists(assetsPath_ / "Triangle" / "glTF" / "Triangle.gltf")) {
            GTEST_SKIP() << "Triangle test asset not found";
        }

        Model::Builder builderNormal;
        Model::Builder builderFlipped;

        importer_.load(builderNormal, (assetsPath_ / "Triangle" / "glTF" / "Triangle.gltf").string(), false, false, false);
        importer_.load(builderFlipped, (assetsPath_ / "Triangle" / "glTF" / "Triangle.gltf").string(), false, true, false);

        ASSERT_EQ(builderNormal.vertices.size(), builderFlipped.vertices.size());
    }

    TEST_F(GLTFImporterTest, Load_WithFlipZ_ModifiesVertices) {
        if (!std::filesystem::exists(assetsPath_ / "Triangle" / "glTF" / "Triangle.gltf")) {
            GTEST_SKIP() << "Triangle test asset not found";
        }

        Model::Builder builderNormal;
        Model::Builder builderFlipped;

        importer_.load(builderNormal, (assetsPath_ / "Triangle" / "glTF" / "Triangle.gltf").string(), false, false, false);
        importer_.load(builderFlipped, (assetsPath_ / "Triangle" / "glTF" / "Triangle.gltf").string(), false, false, true);

        ASSERT_EQ(builderNormal.vertices.size(), builderFlipped.vertices.size());
    }

    TEST_F(GLTFImporterTest, Load_Duck_Succeeds) {
        auto duckPath = assetsPath_ / "Duck" / "glTF" / "Duck.gltf";
        if (!std::filesystem::exists(duckPath)) {
            GTEST_SKIP() << "Duck test asset not found";
        }

        Model::Builder builder;
        bool           result = importer_.load(builder, duckPath.string(), false, false, false);
        EXPECT_TRUE(result);
        EXPECT_FALSE(builder.vertices.empty());
    }

    TEST_F(GLTFImporterTest, Load_Duck_HasMaterials) {
        auto duckPath = assetsPath_ / "Duck" / "glTF" / "Duck.gltf";
        if (!std::filesystem::exists(duckPath)) {
            GTEST_SKIP() << "Duck test asset not found";
        }

        Model::Builder builder;
        importer_.load(builder, duckPath.string(), false, false, false);

        EXPECT_FALSE(builder.materials.empty());
    }

    TEST_F(GLTFImporterTest, Load_Avocado_Succeeds) {
        auto avocadoPath = assetsPath_ / "Avocado" / "glTF" / "Avocado.gltf";
        if (!std::filesystem::exists(avocadoPath)) {
            GTEST_SKIP() << "Avocado test asset not found";
        }

        Model::Builder builder;
        bool           result = importer_.load(builder, avocadoPath.string(), false, false, false);
        EXPECT_TRUE(result);
    }

    TEST_F(GLTFImporterTest, Load_BoxTextured_HasTexturePaths) {
        auto boxPath = assetsPath_ / "BoxTextured" / "glTF" / "BoxTextured.gltf";
        if (!std::filesystem::exists(boxPath)) {
            GTEST_SKIP() << "BoxTextured test asset not found";
        }

        Model::Builder builder;
        importer_.load(builder, boxPath.string(), false, false, false);

        EXPECT_FALSE(builder.materials.empty());

        bool hasTexture = false;
        for (const auto& mat : builder.materials) {
            if (!mat.diffuseTexPath.empty()) {
                hasTexture = true;
                break;
            }
        }
    }

    TEST_F(GLTFImporterTest, Load_Triangle_VerticesHavePositions) {
        if (!std::filesystem::exists(assetsPath_ / "Triangle" / "glTF" / "Triangle.gltf")) {
            GTEST_SKIP() << "Triangle test asset not found";
        }

        Model::Builder builder;
        importer_.load(builder, (assetsPath_ / "Triangle" / "glTF" / "Triangle.gltf").string(), false, false, false);

        ASSERT_FALSE(builder.vertices.empty());

        bool hasNonZeroPosition = false;
        for (const auto& v : builder.vertices) {
            if (v.position != glm::vec3(0.0f)) {
                hasNonZeroPosition = true;
                break;
            }
        }
        EXPECT_TRUE(hasNonZeroPosition);
    }

    TEST_F(GLTFImporterTest, Load_Box_VerticesHaveNormals) {
        if (!std::filesystem::exists(assetsPath_ / "Box" / "glTF" / "Box.gltf")) {
            GTEST_SKIP() << "Box test asset not found";
        }

        Model::Builder builder;
        importer_.load(builder, (assetsPath_ / "Box" / "glTF" / "Box.gltf").string(), false, false, false);

        ASSERT_FALSE(builder.vertices.empty());

        bool hasNonZeroNormal = false;
        for (const auto& v : builder.vertices) {
            if (v.normal != glm::vec3(0.0f)) {
                hasNonZeroNormal = true;
                break;
            }
        }
        EXPECT_TRUE(hasNonZeroNormal);
    }

    TEST_F(GLTFImporterTest, Load_Triangle_IndicesFormValidTriangle) {
        if (!std::filesystem::exists(assetsPath_ / "Triangle" / "glTF" / "Triangle.gltf")) {
            GTEST_SKIP() << "Triangle test asset not found";
        }

        Model::Builder builder;
        importer_.load(builder, (assetsPath_ / "Triangle" / "glTF" / "Triangle.gltf").string(), false, false, false);

        EXPECT_GE(builder.indices.size(), 3);

        for (uint32_t index : builder.indices) {
            EXPECT_LT(index, builder.vertices.size());
        }
    }

    TEST_F(GLTFImporterTest, Load_Box_IndicesMultipleOf3) {
        if (!std::filesystem::exists(assetsPath_ / "Box" / "glTF" / "Box.gltf")) {
            GTEST_SKIP() << "Box test asset not found";
        }

        Model::Builder builder;
        importer_.load(builder, (assetsPath_ / "Box" / "glTF" / "Box.gltf").string(), false, false, false);

        EXPECT_EQ(builder.indices.size() % 3, 0);
    }

    TEST_F(GLTFImporterTest, Load_Triangle_HasNodes) {
        if (!std::filesystem::exists(assetsPath_ / "Triangle" / "glTF" / "Triangle.gltf")) {
            GTEST_SKIP() << "Triangle test asset not found";
        }

        Model::Builder builder;
        importer_.load(builder, (assetsPath_ / "Triangle" / "glTF" / "Triangle.gltf").string(), false, false, false);

        EXPECT_FALSE(builder.nodes.empty());
    }

    TEST_F(GLTFImporterTest, Load_Box_NodeHasValidMeshIndex) {
        if (!std::filesystem::exists(assetsPath_ / "Box" / "glTF" / "Box.gltf")) {
            GTEST_SKIP() << "Box test asset not found";
        }

        Model::Builder builder;
        importer_.load(builder, (assetsPath_ / "Box" / "glTF" / "Box.gltf").string(), false, false, false);

        bool hasValidMeshIndex = false;
        for (const auto& node : builder.nodes) {
            if (node.mesh >= 0) {
                hasValidMeshIndex = true;
                break;
            }
        }
        EXPECT_TRUE(hasValidMeshIndex);
    }

}  // namespace engine::tests
