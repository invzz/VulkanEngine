#include "Editor/ui/Workspace/UIState.hpp"

#include "Engine/Scene/Scene.hpp"
namespace engine {
    UIState::UIState() = default;
    void UIState::setSelectedEntity(entt::entity entity) {
        if (selectedEntity_ != entity) {
            selectedEntity_   = entity;
            selectedObjectId_ = static_cast<uint32_t>(entity);
            if (selectionChangedCallback_) {
                selectionChangedCallback_(entity);
            }
        }
    }
    void UIState::setSelectedObjectId(uint32_t id) {
        selectedObjectId_ = id;
        selectedEntity_   = static_cast<entt::entity>(id);
        if (selectionChangedCallback_) {
            selectionChangedCallback_(selectedEntity_);
        }
    }
    void UIState::onSelectionChanged(std::function<void(entt::entity)> callback) {
        selectionChangedCallback_ = std::move(callback);
    }
    void UIState::setActiveCameraEntity(entt::entity entity) {
        activeCameraEntity_ = entity;
    }
    void UIState::setScene(Scene* scene) {
        scene_ = scene;
    }
    void UIState::setThemePreset(int preset) {
        if (preset >= 0 && preset <= 2) {
            themePreset_ = preset;
        }
    }
    void UIState::setLayoutPreset(LayoutPreset preset) {
        layoutPreset_ = preset;
    }
    void UIState::updateFPS(float frameTimeSec) {
        if (frameTimeSec > 0.0f) {
            fps_ = 1.0f / frameTimeSec;
        }
    }
}  // namespace engine
