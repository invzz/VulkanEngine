/**
 * @file SceneTests.cpp
 * @brief Unit tests for Scene class (ECS wrapper)
 */

#include <glm/glm.hpp>

#include <entt/entt.hpp>
#include <gtest/gtest.h>
#include <set>

#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"

namespace engine::test {

    struct TestComponent {
        int         value = 0;
        std::string name;
    };

    struct TagComponent {
        bool active = true;
    };

    class SceneTest : public ::testing::Test {
       protected:
        Scene scene;
    };

    TEST_F(SceneTest, CreateEntity_ReturnsValidEntity) {
        entt::entity entity = scene.createEntity();
        EXPECT_TRUE(scene.getRegistry().valid(entity));
    }

    TEST_F(SceneTest, CreateEntity_MultipleEntitiesAreUnique) {
        entt::entity e1 = scene.createEntity();
        entt::entity e2 = scene.createEntity();
        entt::entity e3 = scene.createEntity();

        EXPECT_TRUE(e1 != e2);
        EXPECT_TRUE(e2 != e3);
        EXPECT_TRUE(e1 != e3);
    }

    TEST_F(SceneTest, CreateEntity_ManyEntities) {
        std::set<entt::entity> entities;
        constexpr int          COUNT = 1000;

        for (int i = 0; i < COUNT; ++i) {
            entities.insert(scene.createEntity());
        }

        EXPECT_EQ(entities.size(), COUNT);
    }

    TEST_F(SceneTest, DestroyEntity_EntityNoLongerValid) {
        entt::entity entity = scene.createEntity();
        scene.destroyEntity(entity);

        EXPECT_FALSE(scene.getRegistry().valid(entity));
    }

    TEST_F(SceneTest, DestroyEntity_ComponentsRemoved) {
        entt::entity entity = scene.createEntity();
        scene.getRegistry().emplace<TestComponent>(entity, 42, "test");

        scene.destroyEntity(entity);

        EXPECT_FALSE(scene.getRegistry().valid(entity));
    }

    TEST_F(SceneTest, DestroyEntity_MultipleTimes) {
        entt::entity e1 = scene.createEntity();
        entt::entity e2 = scene.createEntity();
        entt::entity e3 = scene.createEntity();

        scene.destroyEntity(e2);

        EXPECT_TRUE(scene.getRegistry().valid(e1));
        EXPECT_FALSE(scene.getRegistry().valid(e2));
        EXPECT_TRUE(scene.getRegistry().valid(e3));
    }

    TEST_F(SceneTest, GetRegistry_ReturnsNonNull) {
        entt::registry& reg = scene.getRegistry();

        entt::entity entity = reg.create();
        EXPECT_TRUE(reg.valid(entity));
    }

    TEST_F(SceneTest, GetRegistry_ConstVersion) {
        scene.createEntity();

        const Scene&          constScene = scene;
        const entt::registry& constReg   = constScene.getRegistry();

        EXPECT_GE(constReg.storage<entt::entity>()->size(), 1u);
    }

    TEST_F(SceneTest, AddComponent_ToEntity) {
        entt::entity entity = scene.createEntity();
        auto&        comp   = scene.getRegistry().emplace<TestComponent>(entity);
        comp.value          = 100;
        comp.name           = "MyComponent";

        auto& retrieved = scene.getRegistry().get<TestComponent>(entity);
        EXPECT_EQ(retrieved.value, 100);
        EXPECT_EQ(retrieved.name, "MyComponent");
    }

    TEST_F(SceneTest, AddComponent_TransformComponent) {
        entt::entity entity    = scene.createEntity();
        auto&        transform = scene.getRegistry().emplace<TransformComponent>(entity);
        transform.translation  = glm::vec3(1.0f, 2.0f, 3.0f);
        transform.scale        = glm::vec3(2.0f);
        transform.rotation.y   = glm::radians(45.0f);

        auto& retrieved = scene.getRegistry().get<TransformComponent>(entity);
        EXPECT_EQ(retrieved.translation, glm::vec3(1.0f, 2.0f, 3.0f));
        EXPECT_EQ(retrieved.scale, glm::vec3(2.0f));
        EXPECT_FLOAT_EQ(retrieved.rotation.y, glm::radians(45.0f));
    }

    TEST_F(SceneTest, AddComponent_MultipleComponents) {
        entt::entity entity = scene.createEntity();
        scene.getRegistry().emplace<TestComponent>(entity, 42, "test");
        scene.getRegistry().emplace<TransformComponent>(entity);
        scene.getRegistry().emplace<TagComponent>(entity);

        EXPECT_TRUE(scene.getRegistry().all_of<TestComponent>(entity));
        EXPECT_TRUE(scene.getRegistry().all_of<TransformComponent>(entity));
        EXPECT_TRUE(scene.getRegistry().all_of<TagComponent>(entity));
    }

    TEST_F(SceneTest, RemoveComponent_FromEntity) {
        entt::entity entity = scene.createEntity();
        scene.getRegistry().emplace<TestComponent>(entity);
        scene.getRegistry().emplace<TagComponent>(entity);

        scene.getRegistry().remove<TestComponent>(entity);

        EXPECT_FALSE(scene.getRegistry().all_of<TestComponent>(entity));
        EXPECT_TRUE(scene.getRegistry().all_of<TagComponent>(entity));
    }

    TEST_F(SceneTest, View_SingleComponent) {
        for (int i = 0; i < 5; ++i) {
            entt::entity e = scene.createEntity();
            scene.getRegistry().emplace<TestComponent>(e, i, "entity_" + std::to_string(i));
        }

        auto view  = scene.getRegistry().view<TestComponent>();
        int  count = 0;
        for (auto entity : view) {
            auto& comp = view.get<TestComponent>(entity);
            EXPECT_GE(comp.value, 0);
            EXPECT_LT(comp.value, 5);
            ++count;
        }
        EXPECT_EQ(count, 5);
    }

    TEST_F(SceneTest, View_MultipleComponents) {
        for (int i = 0; i < 10; ++i) {
            entt::entity e = scene.createEntity();
            scene.getRegistry().emplace<TransformComponent>(e);

            if (i % 2 == 0) {
                scene.getRegistry().emplace<TestComponent>(e, i, "even");
            }
        }

        auto view  = scene.getRegistry().view<TransformComponent, TestComponent>();
        int  count = 0;
        for (auto entity : view) {
            auto& test = view.get<TestComponent>(entity);
            EXPECT_EQ(test.name, "even");
            ++count;
        }
        EXPECT_EQ(count, 5);
    }

    TEST_F(SceneTest, View_EmptyView) {
        for (int i = 0; i < 5; ++i) {
            entt::entity e = scene.createEntity();
            scene.getRegistry().emplace<TransformComponent>(e);
        }

        auto view  = scene.getRegistry().view<TestComponent>();
        int  count = 0;
        for ([[maybe_unused]] auto entity : view) {
            ++count;
        }
        EXPECT_EQ(count, 0);
    }

    TEST_F(SceneTest, EntityRecycling_DestroyedEntityCanBeReused) {
        entt::entity e1 = scene.createEntity();
        scene.destroyEntity(e1);

        entt::entity e2 = scene.createEntity();

        EXPECT_TRUE(scene.getRegistry().valid(e2));
    }

    TEST_F(SceneTest, EntityRecycling_ComponentsNotCarriedOver) {
        entt::entity e1 = scene.createEntity();
        scene.getRegistry().emplace<TestComponent>(e1, 999, "old");
        scene.destroyEntity(e1);

        entt::entity e2 = scene.createEntity();

        EXPECT_FALSE(scene.getRegistry().all_of<TestComponent>(e2));
    }

    TEST_F(SceneTest, StressTest_CreateDestroy) {
        constexpr int ITERATIONS = 100;
        constexpr int BATCH_SIZE = 100;

        for (int iter = 0; iter < ITERATIONS; ++iter) {
            std::vector<entt::entity> entities;
            entities.reserve(BATCH_SIZE);

            for (int i = 0; i < BATCH_SIZE; ++i) {
                entt::entity e = scene.createEntity();
                scene.getRegistry().emplace<TransformComponent>(e);
                scene.getRegistry().emplace<TestComponent>(e, i, "batch");
                entities.push_back(e);
            }

            for (int i = 0; i < BATCH_SIZE / 2; ++i) {
                scene.destroyEntity(entities[i]);
            }
        }

        SUCCEED();
    }

    TEST_F(SceneTest, StressTest_ManyComponents) {
        entt::entity entity = scene.createEntity();

        for (int i = 0; i < 1000; ++i) {
            entt::entity e = scene.createEntity();
            scene.getRegistry().emplace<TestComponent>(e, i, "entity");
        }

        auto view = scene.getRegistry().view<TestComponent>();
        EXPECT_EQ(view.size(), 1000u);
    }

    TEST_F(SceneTest, DefaultConstruction_RegistryIsEmpty) {
        Scene newScene;
        auto  view = newScene.getRegistry().view<entt::entity>();
        EXPECT_EQ(view.size(), 0u);
    }

    TEST_F(SceneTest, MultipleScenes_Independent) {
        Scene scene1;
        Scene scene2;

        entt::entity e1 = scene1.createEntity();
        scene1.getRegistry().emplace<TestComponent>(e1, 1, "scene1");

        entt::entity e2 = scene2.createEntity();
        scene2.getRegistry().emplace<TestComponent>(e2, 2, "scene2");

        EXPECT_EQ(scene1.getRegistry().view<TestComponent>().size(), 1u);
        EXPECT_EQ(scene2.getRegistry().view<TestComponent>().size(), 1u);

        auto& comp1 = scene1.getRegistry().get<TestComponent>(e1);
        auto& comp2 = scene2.getRegistry().get<TestComponent>(e2);

        EXPECT_EQ(comp1.value, 1);
        EXPECT_EQ(comp2.value, 2);
    }

}  // namespace engine::test
