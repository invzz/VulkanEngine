/**
 * @file Fixtures.hpp
 * @brief Master include for all test fixtures
 *
 * Include this header to get access to all available test fixtures.
 * Individual fixtures can also be included separately for lighter builds.
 *
 * Available fixtures:
 * - DeviceFixture / DeviceFixtureWithSetup - Base GPU test fixtures
 * - DescriptorFixture / DescriptorPoolFixture - Descriptor allocation tests
 * - SceneFixture / SceneWithInstanceFixture - Scene and ECS tests
 * - FrameInfoFixture / FrameInfoWithSceneFixture - Render system tests
 * - ImporterFixture - Model importer tests
 */

#ifndef VULKANENGINE_TESTS_FIXTURES_FIXTURES_HPP
#define VULKANENGINE_TESTS_FIXTURES_FIXTURES_HPP

#include "DescriptorFixture.hpp"
#include "DeviceFixture.hpp"
#include "FrameInfoFixture.hpp"
#include "ImporterFixture.hpp"
#include "SceneFixture.hpp"

#endif // VULKANENGINE_TESTS_FIXTURES_FIXTURES_HPP
