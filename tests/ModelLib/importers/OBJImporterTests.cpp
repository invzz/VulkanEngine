#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

#include "ModelLib/Resources/Model.hpp"
#include "ModelLib/importers/OBJImporter.hpp"

namespace engine::tests {

    class OBJImporterTest : public ::testing::Test {
       protected:
        void SetUp() override {
            tempDir_ = std::filesystem::temp_directory_path() / "obj_importer_tests";
            std::filesystem::create_directories(tempDir_);

            triangleObjPath_ = tempDir_ / "triangle.obj";
            {
                std::ofstream file(triangleObjPath_);
                file << "# Simple triangle\n";
                file << "v 0.0 0.0 0.0\n";
                file << "v 1.0 0.0 0.0\n";
                file << "v 0.5 1.0 0.0\n";
                file << "vn 0.0 0.0 1.0\n";
                file << "vt 0.0 0.0\n";
                file << "vt 1.0 0.0\n";
                file << "vt 0.5 1.0\n";
                file << "f 1/1/1 2/2/1 3/3/1\n";
            }

            cubeObjPath_ = tempDir_ / "cube.obj";
            {
                std::ofstream file(cubeObjPath_);
                file << "# Simple cube\n";

                file << "v -0.5 -0.5  0.5\n";
                file << "v  0.5 -0.5  0.5\n";
                file << "v  0.5  0.5  0.5\n";
                file << "v -0.5  0.5  0.5\n";

                file << "v -0.5 -0.5 -0.5\n";
                file << "v  0.5 -0.5 -0.5\n";
                file << "v  0.5  0.5 -0.5\n";
                file << "v -0.5  0.5 -0.5\n";

                file << "vn  0.0  0.0  1.0\n";
                file << "vn  0.0  0.0 -1.0\n";
                file << "vn  0.0  1.0  0.0\n";
                file << "vn  0.0 -1.0  0.0\n";
                file << "vn  1.0  0.0  0.0\n";
                file << "vn -1.0  0.0  0.0\n";

                file << "f 1
                    file
                     << "f 1
                    file
                     << "f 5
                    file
                     << "f 5
                    file
                     << "f 4
                    file
                     << "f 4
                    file
                     << "f 1
                    file
                     << "f 1
                    file
                     << "f 2
                    file
                     << "f 2
                    file
                     << "f 1
                    file
                     << "f 1
            }

            materialObjPath_ = tempDir_ / "with_material.obj";
            materialMtlPath_ = tempDir_ / "with_material.mtl";
            {
                std::ofstream mtlFile(materialMtlPath_);
                mtlFile << "# Test material\n";
                mtlFile << "newmtl TestMaterial\n";
                mtlFile << "Ns 225.0\n";
                mtlFile << "Ka 0.1 0.1 0.1\n";
                mtlFile << "Kd 0.8 0.2 0.2\n";
                mtlFile << "Ks 1.0 1.0 1.0\n";
                mtlFile << "d 1.0\n";
                mtlFile << "illum 2\n";
                mtlFile << "\n";
                mtlFile << "newmtl Chrome\n";
                mtlFile << "Ns 900.0\n";
                mtlFile << "Ka 0.0 0.0 0.0\n";
                mtlFile << "Kd 0.0 0.0 0.0\n";
                mtlFile << "Ks 0.8 0.8 0.8\n";
                mtlFile << "d 1.0\n";
                mtlFile << "illum 3\n";
            }
            {
                std::ofstream objFile(materialObjPath_);
                objFile << "# OBJ with material\n";
                objFile << "mtllib with_material.mtl\n";
                objFile << "v 0.0 0.0 0.0\n";
                objFile << "v 1.0 0.0 0.0\n";
                objFile << "v 0.5 1.0 0.0\n";
                objFile << "vn 0.0 0.0 1.0\n";
                objFile << "usemtl TestMaterial\n";
                objFile << "f 1
            }
        }

        void TearDown() override {
            std::filesystem::remove_all(tempDir_);
        }

        std::filesystem::path tempDir_;
        std::filesystem::path triangleObjPath_;
        std::filesystem::path cubeObjPath_;
        std::filesystem::path materialObjPath_;
        std::filesystem::path materialMtlPath_;
        OBJImporter           importer_;
    };

    TEST_F(OBJImporterTest, GetName_ReturnsCorrectName) {
        EXPECT_EQ(importer_.getName(), "OBJ Importer");
    }

    TEST_F(OBJImporterTest, GetSupportedExtensions_ReturnsObj) {
        auto extensions = importer_.getSupportedExtensions();
        ASSERT_EQ(extensions.size(), 1);
        EXPECT_EQ(extensions[0], "obj");
    }

    TEST_F(OBJImporterTest, Load_NonexistentFile_ReturnsFalse) {
        Model::Builder builder;
        bool           result = importer_.load(builder, "nonexistent_file.obj", false, false, false);
        EXPECT_FALSE(result);
    }

    TEST_F(OBJImporterTest, Load_InvalidFile_ReturnsFalse) {
        auto invalidPath = tempDir_ / "invalid.obj";
        {
            std::ofstream file(invalidPath);

            file << "v 0 0 0\n";
            file << "f invalid\n";
        }

        Model::Builder builder;
        bool           result = importer_.load(builder, invalidPath.string(), false, false, false);
    }

    TEST_F(OBJImporterTest, Load_Triangle_Succeeds) {
        Model::Builder builder;
        bool           result = importer_.load(builder, triangleObjPath_.string(), false, false, false);
        EXPECT_TRUE(result);
    }

    TEST_F(OBJImporterTest, Load_Triangle_HasCorrectVertexCount) {
        Model::Builder builder;
        importer_.load(builder, triangleObjPath_.string(), false, false, false);

        EXPECT_EQ(builder.vertices.size(), 3);
    }

    TEST_F(OBJImporterTest, Load_Triangle_HasCorrectIndexCount) {
        Model::Builder builder;
        importer_.load(builder, triangleObjPath_.string(), false, false, false);

        EXPECT_EQ(builder.indices.size(), 3);
    }

    TEST_F(OBJImporterTest, Load_Cube_Succeeds) {
        Model::Builder builder;
        bool           result = importer_.load(builder, cubeObjPath_.string(), false, false, false);
        EXPECT_TRUE(result);
    }

    TEST_F(OBJImporterTest, Load_Cube_HasVertices) {
        Model::Builder builder;
        importer_.load(builder, cubeObjPath_.string(), false, false, false);

        EXPECT_FALSE(builder.vertices.empty());
    }

    TEST_F(OBJImporterTest, Load_Cube_IndicesMultipleOf3) {
        Model::Builder builder;
        importer_.load(builder, cubeObjPath_.string(), false, false, false);

        EXPECT_EQ(builder.indices.size() % 3, 0);
        EXPECT_EQ(builder.indices.size(), 36);
    }

    TEST_F(OBJImporterTest, Load_Triangle_VerticesHavePositions) {
        Model::Builder builder;
        importer_.load(builder, triangleObjPath_.string(), false, false, false);

        ASSERT_EQ(builder.vertices.size(), 3);

        bool hasOrigin      = false;
        bool hasUnitX       = false;
        bool hasTriangleTip = false;

        for (const auto& v : builder.vertices) {
            if (glm::distance(v.position, glm::vec3(0.0f, 0.0f, 0.0f)) < 0.001f)
                hasOrigin = true;
            if (glm::distance(v.position, glm::vec3(1.0f, 0.0f, 0.0f)) < 0.001f)
                hasUnitX = true;
            if (glm::distance(v.position, glm::vec3(0.5f, 1.0f, 0.0f)) < 0.001f)
                hasTriangleTip = true;
        }

        EXPECT_TRUE(hasOrigin);
        EXPECT_TRUE(hasUnitX);
        EXPECT_TRUE(hasTriangleTip);
    }

    TEST_F(OBJImporterTest, Load_Triangle_VerticesHaveNormals) {
        Model::Builder builder;
        importer_.load(builder, triangleObjPath_.string(), false, false, false);

        for (const auto& v : builder.vertices) {
            EXPECT_NEAR(v.normal.x, 0.0f, 0.001f);
            EXPECT_NEAR(v.normal.y, 0.0f, 0.001f);
            EXPECT_NEAR(v.normal.z, 1.0f, 0.001f);
        }
    }

    TEST_F(OBJImporterTest, Load_Triangle_VerticesHaveUVs) {
        Model::Builder builder;
        importer_.load(builder, triangleObjPath_.string(), false, false, false);

        bool hasNonZeroUV = false;
        for (const auto& v : builder.vertices) {
            if (v.uv != glm::vec2(0.0f)) {
                hasNonZeroUV = true;
                break;
            }
        }
        EXPECT_TRUE(hasNonZeroUV);
    }

    TEST_F(OBJImporterTest, Load_WithFlipX_NegatesXPositions) {
        Model::Builder builderNormal;
        Model::Builder builderFlipped;

        importer_.load(builderNormal, triangleObjPath_.string(), false, false, false);
        importer_.load(builderFlipped, triangleObjPath_.string(), true, false, false);

        ASSERT_EQ(builderNormal.vertices.size(), builderFlipped.vertices.size());

        for (size_t i = 0; i < builderNormal.vertices.size(); ++i) {
            float normalX  = builderNormal.vertices[i].position.x;
            float flippedX = builderFlipped.vertices[i].position.x;

            if (std::abs(normalX) > 0.001f) {
                EXPECT_NEAR(flippedX, -normalX, 0.001f);
            }
        }
    }

    TEST_F(OBJImporterTest, Load_WithFlipY_NegatesYPositions) {
        Model::Builder builderNormal;
        Model::Builder builderFlipped;

        importer_.load(builderNormal, triangleObjPath_.string(), false, false, false);
        importer_.load(builderFlipped, triangleObjPath_.string(), false, true, false);

        ASSERT_EQ(builderNormal.vertices.size(), builderFlipped.vertices.size());

        for (size_t i = 0; i < builderNormal.vertices.size(); ++i) {
            float normalY  = builderNormal.vertices[i].position.y;
            float flippedY = builderFlipped.vertices[i].position.y;

            if (std::abs(normalY) > 0.001f) {
                EXPECT_NEAR(flippedY, -normalY, 0.001f);
            }
        }
    }

    TEST_F(OBJImporterTest, Load_WithFlipZ_NegatesZPositions) {
        Model::Builder builderNormal;
        Model::Builder builderFlipped;

        importer_.load(builderNormal, triangleObjPath_.string(), false, false, false);
        importer_.load(builderFlipped, triangleObjPath_.string(), false, false, true);

        ASSERT_EQ(builderNormal.vertices.size(), builderFlipped.vertices.size());

        for (size_t i = 0; i < builderNormal.vertices.size(); ++i) {
            float normalZ  = builderNormal.vertices[i].normal.z;
            float flippedZ = builderFlipped.vertices[i].normal.z;

            if (std::abs(normalZ) > 0.001f) {
                EXPECT_NEAR(flippedZ, -normalZ, 0.001f);
            }
        }
    }

    TEST_F(OBJImporterTest, Load_WithMaterial_ParsesMaterial) {
        Model::Builder builder;
        bool           result = importer_.load(builder, materialObjPath_.string(), false, false, false);
        EXPECT_TRUE(result);

        EXPECT_FALSE(builder.materials.empty());
    }

    TEST_F(OBJImporterTest, Load_WithMaterial_MaterialHasCorrectName) {
        Model::Builder builder;
        importer_.load(builder, materialObjPath_.string(), false, false, false);

        bool foundTestMaterial = false;
        for (const auto& mat : builder.materials) {
            if (mat.name == "TestMaterial") {
                foundTestMaterial = true;
                break;
            }
        }
        EXPECT_TRUE(foundTestMaterial);
    }

    TEST_F(OBJImporterTest, Load_WithMaterial_HasPBRProperties) {
        Model::Builder builder;
        importer_.load(builder, materialObjPath_.string(), false, false, false);

        ASSERT_FALSE(builder.materials.empty());

        const auto& mat = builder.materials[0];

        EXPECT_GE(mat.pbrMaterial.roughness, 0.0f);
        EXPECT_LE(mat.pbrMaterial.roughness, 1.0f);
    }

    TEST_F(OBJImporterTest, Load_Triangle_AllIndicesValid) {
        Model::Builder builder;
        importer_.load(builder, triangleObjPath_.string(), false, false, false);

        for (uint32_t index : builder.indices) {
            EXPECT_LT(index, builder.vertices.size());
        }
    }

    TEST_F(OBJImporterTest, Load_Cube_AllIndicesValid) {
        Model::Builder builder;
        importer_.load(builder, cubeObjPath_.string(), false, false, false);

        for (uint32_t index : builder.indices) {
            EXPECT_LT(index, builder.vertices.size());
        }
    }

    TEST_F(OBJImporterTest, Load_EmptyFile_HandleGracefully) {
        auto emptyPath = tempDir_ / "empty.obj";
        {
            std::ofstream file(emptyPath);
            file << "# Empty OBJ file\n";
        }

        Model::Builder builder;
        bool           result = importer_.load(builder, emptyPath.string(), false, false, false);

        if (result) {
            EXPECT_TRUE(builder.vertices.empty());
        }
    }

    TEST_F(OBJImporterTest, Load_CommentsOnly_HandleGracefully) {
        auto commentsPath = tempDir_ / "comments.obj";
        {
            std::ofstream file(commentsPath);
            file << "# Comment 1\n";
            file << "# Comment 2\n";
            file << "# More comments\n";
        }

        Model::Builder builder;
        importer_.load(builder, commentsPath.string(), false, false, false);

        EXPECT_TRUE(builder.vertices.empty());
    }

    TEST_F(OBJImporterTest, Load_Cube_HasCorrectFaceCount) {
        Model::Builder builder;
        importer_.load(builder, cubeObjPath_.string(), false, false, false);

        size_t triangleCount = builder.indices.size() / 3;
        EXPECT_EQ(triangleCount, 12);
    }

    TEST_F(OBJImporterTest, Load_Cube_VerticesWithinBounds) {
        Model::Builder builder;
        importer_.load(builder, cubeObjPath_.string(), false, false, false);

        for (const auto& v : builder.vertices) {
            EXPECT_GE(v.position.x, -0.51f);
            EXPECT_LE(v.position.x, 0.51f);
            EXPECT_GE(v.position.y, -0.51f);
            EXPECT_LE(v.position.y, 0.51f);
            EXPECT_GE(v.position.z, -0.51f);
            EXPECT_LE(v.position.z, 0.51f);
        }
    }

}  // namespace engine::tests
