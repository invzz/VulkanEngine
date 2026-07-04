#include "Editor/ui/Panels/LightsPanel.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <random>

#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/components/DirectionalLightComponent.hpp"
#include "Engine/Scene/components/PointLightComponent.hpp"
#include "Engine/Scene/components/SpotLightComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"

#include "Editor/ui/UI.hpp"
#include "entt/entity/entity.hpp"
#include "glm/geometric.hpp"
namespace engine {
    LightsPanel::LightsPanel(Scene& scene) : scene_(scene) {}
    void LightsPanel::render(FrameInfo& frameInfo) {
        if (!visible_) {
            return;
        }
        if (ui::UI::Button("Spawn 200 Point Lights##lights_spawn")) {
            auto&                                 registry = scene_.getRegistry();
            std::mt19937                          rng{1337u};
            std::uniform_real_distribution<float> angleDist(0.0f, 6.28318530718f);
            std::uniform_real_distribution<float> heightDist(-2.0f, 2.0f);
            std::uniform_real_distribution<float> radiusDist(6.0f, 35.0f);
            std::uniform_real_distribution<float> colorDist(0.2f, 1.0f);
            for (int i = 0; i < 200; i++) {
                auto        entity    = registry.create();
                auto&       transform = registry.emplace<TransformComponent>(entity);
                float const a         = angleDist(rng);
                float const r         = radiusDist(rng);
                transform.translation = glm::vec3(std::cos(a) * r, heightDist(rng), std::sin(a) * r);
                auto& light           = registry.emplace<PointLightComponent>(entity);
                light.intensity       = 20.0f;
                light.radius          = 20.0f;
                light.color           = glm::vec3(colorDist(rng), colorDist(rng), colorDist(rng));
            }
        }
        if (frameInfo.selectedEntity != entt::null) {
            auto  entity   = frameInfo.selectedEntity;
            auto& registry = scene_.getRegistry();
            if (registry.all_of<PointLightComponent>(entity)) {
                auto& pointLight = registry.get<PointLightComponent>(entity);
                ui::UI::TextColored("Point Light", ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
                ui::UI::Separator();
                ui::UI::DragFloat("Intensity##point", &pointLight.intensity, 0.1f, 0.0f, 100.0f);
                ui::UI::ColorEdit3("Color##point", &pointLight.color.x);
                ui::UI::DragFloat("Radius##point", &pointLight.radius, 0.01f, 0.0f, 100.0f);
                int         pointMob     = static_cast<int>(pointLight.lightType);
                const char* pointItems[] = {"Static", "Dynamic"};
                if (ui::UI::Combo("Mobility##point", &pointMob, pointItems, 2)) {
                    pointLight.lightType = static_cast<LightMobility>(pointMob);
                }
                ImGui::Spacing();
            }
            if (registry.all_of<DirectionalLightComponent>(entity)) {
                auto& dirLight = registry.get<DirectionalLightComponent>(entity);
                ui::UI::TextColored("Directional Light", ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
                ui::UI::Separator();
                ui::UI::DragFloat("Intensity##directional", &dirLight.intensity, 0.01f, 0.0f, 10.0f);
                ui::UI::ColorEdit3("Color##directional", &dirLight.color.x);
                int         dirMob     = static_cast<int>(dirLight.lightType);
                const char* dirItems[] = {"Static", "Dynamic"};
                if (ui::UI::Combo("Mobility##directional", &dirMob, dirItems, 2)) {
                    dirLight.lightType = static_cast<LightMobility>(dirMob);
                }
                ImGui::Spacing();
                ui::UI::TextDisabled("Direction Control:");
                if (ui::UI::Checkbox("Use Target Point##directional", &dirLight.useTargetPoint)) {
                    if (dirLight.useTargetPoint) {
                        if (glm::length(dirLight.targetPoint) < 0.01f) {
                            dirLight.targetPoint = glm::vec3(0.0f, 0.0f, -5.0f);
                        }
                    }
                }
                if (dirLight.useTargetPoint) {
                    ui::UI::DragFloat3("Target Point##directional", &dirLight.targetPoint.x, 0.1f);
                }
                glm::vec3 const dir    = registry.get<TransformComponent>(entity).getForwardDir();
                std::string     dirStr = "Current Dir: (" + std::to_string(dir.x).substr(0, 5) + ", " +
                                         std::to_string(dir.y).substr(0, 5) + ", " +
                                         std::to_string(dir.z).substr(0, 5) + ")";
                ui::UI::TextDisabled(dirStr.c_str());
                ImGui::Spacing();
            }
            if (registry.all_of<SpotLightComponent>(entity)) {
                auto& spotLight = registry.get<SpotLightComponent>(entity);
                ui::UI::TextColored("Spot Light", ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
                ui::UI::Separator();
                ui::UI::DragFloat("Intensity##spot", &spotLight.intensity, 0.1f, 0.0f, 100.0f);
                ui::UI::ColorEdit3("Color##spot", &spotLight.color.x);
                int         spotMob     = static_cast<int>(spotLight.lightType);
                const char* spotItems[] = {"Static", "Dynamic"};
                if (ui::UI::Combo("Mobility##spot", &spotMob, spotItems, 2)) {
                    spotLight.lightType = static_cast<LightMobility>(spotMob);
                }
                ImGui::Spacing();
                ui::UI::TextDisabled("Direction Control:");
                if (ui::UI::Checkbox("Use Target Point##spot", &spotLight.useTargetPoint)) {
                    if (spotLight.useTargetPoint) {
                        if (glm::length(spotLight.targetPoint) < 0.01f) {
                            spotLight.targetPoint = glm::vec3(0.0f, 0.0f, -5.0f);
                        }
                    }
                }
                if (spotLight.useTargetPoint) {
                    ui::UI::DragFloat3("Target Point##spot", &spotLight.targetPoint.x, 0.1f);
                }
                glm::vec3 const dir    = registry.get<TransformComponent>(entity).getForwardDir();
                std::string     dirStr = "Current Dir: (" + std::to_string(dir.x).substr(0, 5) + ", " +
                                         std::to_string(dir.y).substr(0, 5) + ", " +
                                         std::to_string(dir.z).substr(0, 5) + ")";
                ui::UI::TextDisabled(dirStr.c_str());
                ImGui::Spacing();
                ui::UI::TextDisabled("Cone Angles:");
                ui::UI::DragFloat("Inner Cutoff (deg)##spot_inner", &spotLight.innerCutoffAngle, 0.5f, 0.0f, 90.0f);
                ui::UI::DragFloat("Outer Cutoff (deg)##spot_outer", &spotLight.outerCutoffAngle, 0.5f, 0.0f, 90.0f);
                spotLight.outerCutoffAngle = std::max(spotLight.outerCutoffAngle, spotLight.innerCutoffAngle);
                ImGui::Spacing();
                ui::UI::TextDisabled("Attenuation:");
                ui::UI::DragFloat("Constant##spot_const", &spotLight.constantAttenuation, 0.01f, 0.0f, 10.0f);
                ui::UI::DragFloat("Linear##spot_linear", &spotLight.linearAttenuation, 0.001f, 0.0f, 1.0f);
                ui::UI::DragFloat("Quadratic##spot_quad", &spotLight.quadraticAttenuation, 0.001f, 0.0f, 1.0f);
            }
            bool const hasPointLight = registry.all_of<PointLightComponent>(entity);
            bool const hasDirLight   = registry.all_of<DirectionalLightComponent>(entity);
            bool const hasSpotLight  = registry.all_of<SpotLightComponent>(entity);
            if (!hasPointLight && !hasDirLight && !hasSpotLight) {
                ui::UI::TextDisabled("No light component");
                ui::UI::TextDisabled("This object is not a light");
            }
        } else {
            ui::UI::TextDisabled("No object selected");
            ui::UI::TextDisabled("Press Y/U to select objects");
        }
    }
}  // namespace engine
