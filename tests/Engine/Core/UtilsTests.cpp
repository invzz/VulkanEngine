/**
 * @file UtilsTests.cpp
 * @brief Unit tests for utility functions (hashCombine)
 */
#include <glm/glm.hpp>

#include <gtest/gtest.h>
#include <string>
#include <unordered_set>

#include "Engine/Core/utils.hpp"
namespace engine::test {
    TEST(HashCombineTest, SingleInt) {
        std::size_t seed = 0;
        hashCombine(seed, 42);
        EXPECT_NE(seed, 0u);
    }
    TEST(HashCombineTest, SingleString) {
        std::size_t seed = 0;
        std::string str  = "hello";
        hashCombine(seed, str);
        EXPECT_NE(seed, 0u);
    }
    TEST(HashCombineTest, SingleFloat) {
        std::size_t seed = 0;
        hashCombine(seed, 3.14159f);
        EXPECT_NE(seed, 0u);
    }
    TEST(HashCombineTest, SingleDouble) {
        std::size_t seed = 0;
        hashCombine(seed, 3.14159265358979);
        EXPECT_NE(seed, 0u);
    }
    TEST(HashCombineTest, TwoInts) {
        std::size_t seed = 0;
        hashCombine(seed, 1, 2);
        std::size_t seed2 = 0;
        hashCombine(seed2, 1);
        hashCombine(seed2, 2);
        EXPECT_EQ(seed, seed2);
    }
    TEST(HashCombineTest, MultipleValues_DifferentOrder) {
        std::size_t seed1 = 0;
        hashCombine(seed1, 1, 2, 3);
        std::size_t seed2 = 0;
        hashCombine(seed2, 3, 2, 1);
        EXPECT_NE(seed1, seed2);
    }
    TEST(HashCombineTest, MultipleValues_MixedTypes) {
        std::size_t seed = 0;
        hashCombine(seed, 42, 3.14f, std::string("test"));
        EXPECT_NE(seed, 0u);
    }
    TEST(HashCombineTest, ManyValues) {
        std::size_t seed = 0;
        hashCombine(seed, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
        EXPECT_NE(seed, 0u);
    }
    TEST(HashCombineTest, Vec2) {
        std::size_t seed = 0;
        glm::vec2   v(1.0f, 2.0f);
        hashCombine(seed, v);
        EXPECT_NE(seed, 0u);
    }
    TEST(HashCombineTest, Vec3) {
        std::size_t seed = 0;
        glm::vec3   v(1.0f, 2.0f, 3.0f);
        hashCombine(seed, v);
        EXPECT_NE(seed, 0u);
    }
    TEST(HashCombineTest, Vec4) {
        std::size_t seed = 0;
        glm::vec4   v(1.0f, 2.0f, 3.0f, 4.0f);
        hashCombine(seed, v);
        EXPECT_NE(seed, 0u);
    }
    TEST(HashCombineTest, IVec2) {
        std::size_t seed = 0;
        glm::ivec2  v(1, 2);
        hashCombine(seed, v);
        EXPECT_NE(seed, 0u);
    }
    TEST(HashCombineTest, IVec3) {
        std::size_t seed = 0;
        glm::ivec3  v(1, 2, 3);
        hashCombine(seed, v);
        EXPECT_NE(seed, 0u);
    }
    TEST(HashCombineTest, IVec4) {
        std::size_t seed = 0;
        glm::ivec4  v(1, 2, 3, 4);
        hashCombine(seed, v);
        EXPECT_NE(seed, 0u);
    }
    TEST(HashCombineTest, DVec3) {
        std::size_t seed = 0;
        glm::dvec3  v(1.0, 2.0, 3.0);
        hashCombine(seed, v);
        EXPECT_NE(seed, 0u);
    }
    TEST(HashCombineTest, SameInput_SameOutput) {
        std::size_t seed1 = 0;
        hashCombine(seed1, 42, 3.14f, glm::vec3(1.0f, 2.0f, 3.0f));
        std::size_t seed2 = 0;
        hashCombine(seed2, 42, 3.14f, glm::vec3(1.0f, 2.0f, 3.0f));
        EXPECT_EQ(seed1, seed2);
    }
    TEST(HashCombineTest, DifferentInput_DifferentOutput) {
        std::size_t seed1 = 0;
        hashCombine(seed1, 1);
        std::size_t seed2 = 0;
        hashCombine(seed2, 2);
        EXPECT_NE(seed1, seed2);
    }
    TEST(HashCombineTest, Vec3_DifferentComponents_DifferentHash) {
        std::size_t seed1 = 0;
        hashCombine(seed1, glm::vec3(1.0f, 0.0f, 0.0f));
        std::size_t seed2 = 0;
        hashCombine(seed2, glm::vec3(0.0f, 1.0f, 0.0f));
        std::size_t seed3 = 0;
        hashCombine(seed3, glm::vec3(0.0f, 0.0f, 1.0f));
        EXPECT_NE(seed1, seed2);
        EXPECT_NE(seed2, seed3);
        EXPECT_NE(seed1, seed3);
    }
    TEST(HashCombineTest, Distribution_ManyInts) {
        std::unordered_set<std::size_t> hashes;
        for (int i = 0; i < 1000; ++i) {
            std::size_t seed = 0;
            hashCombine(seed, i);
            hashes.insert(seed);
        }
        EXPECT_EQ(hashes.size(), 1000u);
    }
    TEST(HashCombineTest, Distribution_Vec3) {
        std::unordered_set<std::size_t> hashes;
        for (int i = 0; i < 100; ++i) {
            for (int j = 0; j < 10; ++j) {
                std::size_t seed = 0;
                hashCombine(seed, glm::vec3(static_cast<float>(i), static_cast<float>(j), 0.0f));
                hashes.insert(seed);
            }
        }
        EXPECT_GT(hashes.size(), 990u);
    }
    TEST(HashCombineTest, Distribution_MixedValues) {
        std::unordered_set<std::size_t> hashes;
        for (int i = 0; i < 100; ++i) {
            std::size_t seed = 0;
            hashCombine(seed, i, static_cast<float>(i) * 0.1f, glm::vec3(static_cast<float>(i)));
            hashes.insert(seed);
        }
        EXPECT_EQ(hashes.size(), 100u);
    }
    TEST(HashCombineTest, ZeroValue) {
        std::size_t seed = 0;
        hashCombine(seed, 0);
        EXPECT_NE(seed, 0u);
    }
    TEST(HashCombineTest, NegativeValue) {
        std::size_t seed = 0;
        hashCombine(seed, -42);
        EXPECT_NE(seed, 0u);
    }
    TEST(HashCombineTest, VeryLargeValue) {
        std::size_t seed = 0;
        hashCombine(seed, std::numeric_limits<int>::max());
        EXPECT_NE(seed, 0u);
    }
    TEST(HashCombineTest, VerySmallFloat) {
        std::size_t seed = 0;
        hashCombine(seed, std::numeric_limits<float>::min());
        EXPECT_NE(seed, 0u);
    }
    TEST(HashCombineTest, EmptyString) {
        std::size_t seed = 0;
        hashCombine(seed, std::string(""));
        EXPECT_NE(seed, 0u);
    }
    TEST(HashCombineTest, ZeroVector) {
        std::size_t seed = 0;
        hashCombine(seed, glm::vec3(0.0f));
        EXPECT_NE(seed, 0u);
    }
    TEST(HashCombineTest, NonZeroInitialSeed) {
        std::size_t seed1 = 12345;
        hashCombine(seed1, 42);
        std::size_t seed2 = 0;
        hashCombine(seed2, 42);
        EXPECT_NE(seed1, seed2);
    }
    TEST(HashCombineTest, NonZeroInitialSeed_Chained) {
        std::size_t seed = 100;
        hashCombine(seed, 1);
        hashCombine(seed, 2);
        hashCombine(seed, 3);
        EXPECT_NE(seed, 100u);
    }
    TEST(HashCombineTest, UseCase_VertexHash) {
        struct Vertex {
            glm::vec3   position;
            glm::vec3   normal;
            glm::vec2   texCoord;
            std::size_t hash() const {
                std::size_t seed = 0;
                engine::hashCombine(seed, position, normal, texCoord);
                return seed;
            }
        };
        Vertex v1{{1.0f, 2.0f, 3.0f}, {0.0f, 1.0f, 0.0f}, {0.5f, 0.5f}};
        Vertex v2{{1.0f, 2.0f, 3.0f}, {0.0f, 1.0f, 0.0f}, {0.5f, 0.5f}};
        Vertex v3{{1.0f, 2.0f, 3.0f}, {1.0f, 0.0f, 0.0f}, {0.5f, 0.5f}};
        EXPECT_EQ(v1.hash(), v2.hash());
        EXPECT_NE(v1.hash(), v3.hash());
    }
    TEST(HashCombineTest, UseCase_KeyHash) {
        struct CompositeKey {
            int         id;
            std::string name;
            std::size_t hash() const {
                std::size_t seed = 0;
                engine::hashCombine(seed, id, name);
                return seed;
            }
        };
        CompositeKey k1{1, "test"};
        CompositeKey k2{1, "test"};
        CompositeKey k3{1, "other"};
        EXPECT_EQ(k1.hash(), k2.hash());
        EXPECT_NE(k1.hash(), k3.hash());
    }
}  // namespace engine::test
