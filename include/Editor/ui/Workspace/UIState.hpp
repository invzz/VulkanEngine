#ifndef EDITOR_WORKSPACE_UI_STATE_HPP
#define EDITOR_WORKSPACE_UI_STATE_HPP

#include <cstdint>
#include <entt/entt.hpp>
#include <functional>
#include <string>

#include "Layout.hpp"

namespace engine {

    class Scene;

    /**
 * @brief Editor-wide UI state (selection, active camera, etc.).
 *
 * This is the single source of truth for all editor UI state. Panels and
 * systems read/write through this object instead of directly manipulating
 * FrameInfo or passing pointers everywhere.
 *
 * Thread-safe for reads; writes must be done on the main thread.
 */
    class UIState {
       public:
        UIState();
        ~UIState() = default;

        // --- Selection ---

        /** Get the currently selected entity. */
        entt::entity getSelectedEntity() const {
            return selectedEntity_;
        }

        /** Set the selected entity. Emits selectionChanged signal. */
        void setSelectedEntity(entt::entity entity);

        /** Get the selected object ID (for ImGui compatibility). */
        uint32_t getSelectedObjectId() const {
            return selectedObjectId_;
        }

        /** Set the selected object ID. */
        void setSelectedObjectId(uint32_t id);

        /** Register a callback to be called when selection changes. */
        void onSelectionChanged(std::function<void(entt::entity)> callback);

        // --- Camera ---

        /** Get the active camera entity. */
        entt::entity getActiveCameraEntity() const {
            return activeCameraEntity_;
        }

        /** Set the active camera entity. */
        void setActiveCameraEntity(entt::entity entity);

        // --- Scene ---

        /** Get the current scene. */
        Scene* getScene() const {
            return scene_;
        }

        /** Set the current scene. */
        void setScene(Scene* scene);

        // --- Theme ---

        /** Get the current theme preset (0=dark, 1=light, 2=midnight). */
        int getThemePreset() const {
            return themePreset_;
        }

        /** Set the current theme preset. */
        void setThemePreset(int preset);

        // --- Layout ---

        /** Get the current layout preset. */
        LayoutPreset getLayoutPreset() const {
            return layoutPreset_;
        }

        /** Set the current layout preset. */
        void setLayoutPreset(LayoutPreset preset);

        // --- Frame timing ---

        /** Get the current frame time in milliseconds. */
        float getFrameTimeMs() const {
            return frameTimeMs_;
        }

        /** Set the current frame time. */
        void setFrameTimeMs(float ms) {
            frameTimeMs_ = ms;
        }

        /** Get the current FPS. */
        float getFPS() const {
            return fps_;
        }

        /** Update FPS counter. */
        void updateFPS(float frameTimeSec);

       private:
        entt::entity selectedEntity_     = entt::null;
        uint32_t     selectedObjectId_   = 0;
        entt::entity activeCameraEntity_ = entt::null;
        Scene*       scene_              = nullptr;
        int          themePreset_        = 0;  // default to dark
        LayoutPreset layoutPreset_       = LayoutPreset::Default;
        float        frameTimeMs_        = 0.0f;
        float        fps_                = 0.0f;

        std::function<void(entt::entity)> selectionChangedCallback_;
    };

}  // namespace engine

#endif  // EDITOR_WORKSPACE_UI_STATE_HPP
