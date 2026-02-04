/**
 * @file SceneFixture.hpp
 * @brief Shared test fixture for Scene and ECS tests
 *
 * Provides a Device with ResourceManager for scene-related tests.
 * Useful for tests involving Scene, SceneSerializer, or components that
 * need model loading capabilities.
 */

#ifndef VULKANENGINE_TESTS_FIXTURES_SCENEFIXTURE_HPP
#define VULKANENGINE_TESTS_FIXTURES_SCENEFIXTURE_HPP

#include <gtest/gtest.h>

#include <filesystem>
#include <memory>

#include "DeviceFixture.hpp"
#include "Engine/Scene/Scene.hpp"
#include "ModelLib/Resources/ResourceManager.hpp"

namespace engine::test {

  /**
   * @brief Scene fixture with Device + ResourceManager
   *
   * Provides:
   * - Shared Device (from DeviceFixture)
   * - Per-test ResourceManager instance
   * - Assets directory setup
   */
  class SceneFixture : public DeviceFixtureWithSetup
  {
  protected:
    void SetUp() override
    {
      DeviceFixtureWithSetup::SetUp();

      // Ensure test directories exist
      std::filesystem::create_directories("assets/scenes/test");

      // Create ResourceManager for this test
      resourceManager_ = std::make_unique<ResourceManager>(device());
    }

    void TearDown() override
    {
      resourceManager_.reset();
      DeviceFixtureWithSetup::TearDown();
    }

    // Accessors
    ResourceManager& resourceManager() { return *resourceManager_; }

    // Helper to create a fresh scene
    std::unique_ptr<Scene> createScene() { return std::make_unique<Scene>(); }

    // Helper to get test scene directory path
    static std::filesystem::path testSceneDir() { return "assets/scenes/test"; }

    // Helper to create a unique test file path
    std::filesystem::path createTestScenePath(const std::string& testName) { return testSceneDir() / (testName + ".json"); }

  private:
    std::unique_ptr<ResourceManager> resourceManager_;
  };

  /**
   * @brief Scene fixture that creates a Scene instance for each test
   *
   * Extends SceneFixture with an automatically created Scene.
   */
  class SceneWithInstanceFixture : public SceneFixture
  {
  protected:
    void SetUp() override
    {
      SceneFixture::SetUp();
      scene_ = createScene();
    }

    void TearDown() override
    {
      scene_.reset();
      SceneFixture::TearDown();
    }

    Scene& scene() { return *scene_; }

  private:
    std::unique_ptr<Scene> scene_;
  };

} // namespace engine::test

#endif // VULKANENGINE_TESTS_FIXTURES_SCENEFIXTURE_HPP
