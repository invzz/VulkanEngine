/**
 * @file DeviceFixture.hpp
 * @brief Shared test fixture that provides a single Window/Device instance
 *
 * This fixture creates Window and Device once per test suite (not per test),
 * significantly speeding up GPU-based tests.
 *
 * Usage:
 *   class MyTest : public DeviceFixture { ... };
 *   TEST_F(MyTest, MyTestCase) { device()... }
 *
 * Or for test suites that need per-test setup:
 *   class MyTest : public DeviceFixtureWithSetup {
 *     void SetUp() override {
 *       DeviceFixtureWithSetup::SetUp();
 *       // your per-test setup
 *     }
 *   };
 */

#ifndef VULKANENGINE_TESTS_FIXTURES_DEVICEFIXTURE_HPP
#define VULKANENGINE_TESTS_FIXTURES_DEVICEFIXTURE_HPP

#include <gtest/gtest.h>
#include <memory>

#include "Engine/Core/Window.hpp"
#include "Engine/Graphics/Device.hpp"

namespace engine::test {

    /**
 * @brief Shared Device fixture - creates Window/Device once per test suite
 *
 * Uses SetUpTestSuite/TearDownTestSuite for one-time initialization.
 * All tests in the suite share the same Device instance.
 */
    class DeviceFixture : public ::testing::Test {
       public:
        static void SetUpTestSuite() {
            if (!sharedWindow) {
                sharedWindow = std::make_unique<Window>(16, 16, "Shared Test Device");
                sharedDevice = std::make_unique<Device>(*sharedWindow);
            }
            ++suiteRefCount;
        }

        static void TearDownTestSuite() {
            --suiteRefCount;
            if (suiteRefCount == 0) {
                if (sharedDevice) {
                    sharedDevice->WaitIdle();
                }
                sharedDevice.reset();
                sharedWindow.reset();
            }
        }

       protected:
        // Accessors for derived test classes
        static Window& window() {
            return *sharedWindow;
        }
        static Device& device() {
            return *sharedDevice;
        }

        // Raw pointer accessors (for APIs that need pointers)
        static Window* windowPtr() {
            return sharedWindow.get();
        }
        static Device* devicePtr() {
            return sharedDevice.get();
        }

       private:
        static inline std::unique_ptr<Window> sharedWindow;
        static inline std::unique_ptr<Device> sharedDevice;
        static inline int                     suiteRefCount = 0;
    };

    /**
 * @brief Device fixture with per-test SetUp/TearDown hooks
 *
 * Inherits shared Device but allows per-test setup.
 * Override SetUp()/TearDown() for test-specific initialization.
 * Always call base class methods when overriding.
 */
    class DeviceFixtureWithSetup : public DeviceFixture {
       protected:
        void SetUp() override {
            // Ensure device is idle before each test
            device().WaitIdle();
        }

        void TearDown() override {
            // Ensure all GPU work completes after each test
            device().WaitIdle();
        }
    };

}  // namespace engine::test

#endif  // VULKANENGINE_TESTS_FIXTURES_DEVICEFIXTURE_HPP
