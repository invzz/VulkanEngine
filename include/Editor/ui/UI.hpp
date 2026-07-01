#ifndef EDITOR_UI_ABSTRACTION_HPP
#define EDITOR_UI_ABSTRACTION_HPP

#include <glm/glm.hpp>

#include <imgui.h>

#include <entt/entity/fwd.hpp>

#include <functional>
#include <string>
#include <vector>

#include "Engine/Scene/SceneUtils.hpp"

namespace engine {
   class WorkspaceManager;
   class Scene;
   class ResourceManager;
   struct FrameInfo;
}

namespace engine::ui {

   struct ScenePendingModelLoad {
      AsyncLoadId                                             id{0};
      std::string                                             path;
      std::string                                             name;
      engine::ModelInsertionOptions                           options;
      engine::ModelInsertionOptions::StaticColliderImportMode colliderMode{engine::ModelInsertionOptions::StaticColliderImportMode::AutoDetect};
      bool                                                    cancelled = false;
   };

   struct SceneEntityCollection {
      std::vector<entt::entity> cameras;
      std::vector<entt::entity> dirLights;
      std::vector<entt::entity> pointLights;
      std::vector<entt::entity> spotLights;
      std::vector<entt::entity> models;
   };

   /**
 * @brief Bind the active workspace context used by UI helper functions.
 */
   void SetActiveWorkspace(WorkspaceManager* wm);

    /**
 * @brief Engine-safe UI abstraction layer over ImGui.
 *
 * Provides semantic widgets that automatically apply the current theme
 * colors and consistent spacing. Panels should prefer these over raw
 * ImGui calls to reduce visual noise and ensure consistency.
 *
 * Design principles:
 * - Thin wrapper: always delegates to ImGui, never replaces it
 * - Theme-aware: reads colors from ThemeSystem automatically
 * - Noise-reducing: auto-collapsing, grouped sections, context display
 * - Non-intrusive: existing ImGui calls work alongside these widgets
 */
    class UI {
       public:
        // ======================================================================
        // Section / Grouping
        // ======================================================================

        /**
     * @brief Render a collapsible section header with consistent styling.
     * @param label Section title.
     * @param open_ptr Optional pointer to control open/close state (default: auto).
     * @return true if the section is currently open.
     */
        static bool Section(const char* label, bool* open_ptr = nullptr);

        /**
     * @brief Render a section with a colored accent bar on the left.
     * @param label Section title.
     * @param accent_color Color for the accent bar (default: theme accent).
     * @param open_ptr Optional pointer to control open/close state.
     * @return true if the section is currently open.
     */
        static bool Section(const char* label, ImVec4 accent_color, bool* open_ptr = nullptr);

        /**
     * @brief Render a separator with consistent thickness/color.
     */
        static void Separator();

        /**
     * @brief Render a thin horizontal divider with subtle color.
     */
        static void Divider();

        // ======================================================================
        // Buttons
        // ======================================================================

        /**
     * @brief Render a styled button with consistent sizing.
     * @param label Button text (must include ##unique ID for ImGui).
     * @return true if clicked.
     */
        static bool Button(const char* label);

        /**
     * @brief Render a small button (used for inline actions).
     * @param label Button text (must include ##unique ID).
     * @return true if clicked.
     */
        static bool SmallButton(const char* label);

        /**
     * @brief Render a button with an icon prefix.
     * @param icon Unicode icon character (e.g., "\u25B6" for play).
     * @param label Button text.
     * @return true if clicked.
     */
        static bool IconButton(const char* icon, const char* label);

        /**
     * @brief Render a toolbar icon button (square padding, active state).
     * @param icon Unicode icon character.
     * @param active Whether this button is currently active.
     * @param suffix Unique ID suffix (appended with ##, e.g. "gizmo_translate").
     * @return true if clicked.
     */
        static bool ToolbarIcon(const char* icon, bool active, const char* suffix = "");

        /**
     * @brief Render a button that appears disabled (grayed out).
     * @param label Button text.
     * @return true if clicked (even when disabled).
     */
        static bool DisabledButton(const char* label);

        /**
     * @brief Render a high-emphasis action button (material-style primary action).
     * @param label Button text.
     * @param size Optional custom size.
     * @return true if clicked.
     */
        static bool PrimaryButton(const char* label, ImVec2 size = ImVec2(0.0f, 0.0f));

        /**
     * @brief Render a low-emphasis filled button for secondary actions.
     * @param label Button text.
     * @param size Optional custom size.
     * @return true if clicked.
     */
        static bool TonalButton(const char* label, ImVec2 size = ImVec2(0.0f, 0.0f));

        // ======================================================================
        // Material Surfaces
        // ======================================================================

        /**
     * @brief Begin a material-style surface card.
     * @param id Unique id for internal draw calls.
     * @param title Optional heading rendered at top of surface.
     * @param subtitle Optional subtitle rendered below title.
     * @return true if rendering should continue.
     */
        static bool BeginSurface(const char* id, const char* title = nullptr, const char* subtitle = nullptr);

        /**
     * @brief End a material-style surface card.
     */
        static void EndSurface();

        /**
     * @brief Render a consistent section title used inside surfaces.
     * @param label Section title.
     * @param helper Optional helper text shown below title.
     */
        static void SectionTitle(const char* label, const char* helper = nullptr);

        /**
     * @brief Render a two-line boolean row with title/description and checkbox.
     * @param label Row title.
     * @param description Supporting text.
     * @param value Bound boolean value.
     * @return true if changed.
     */
        static bool CheckboxRow(const char* label, const char* description, bool* value);

      /**
    * @brief Render a two-line row with label/description and float drag control.
    * @param label Row title.
    * @param description Supporting text.
    * @param value Bound float value.
    * @param speed Drag speed.
    * @param min Minimum value.
    * @param max Maximum value.
    * @return true if changed.
    */
      static bool FloatRow(const char* label, const char* description, float* value,
         float speed = 0.1f, float min = -1e10f, float max = 1e10f);

      /**
    * @brief Render a two-line row with label/description and enum combo control.
    * @param label Row title.
    * @param description Supporting text.
    * @param current_index Selected enum index.
    * @param items Display names.
    * @param count Number of items.
    * @return true if changed.
    */
      static bool EnumRow(const char* label, const char* description, int* current_index,
         const char* const items[], int count);

        // ======================================================================
        // Properties (key-value display in inspector panels)
        // ======================================================================

        /**
     * @brief Render a property row (label on left, value on right).
     * @param label Property name (displayed in theme disabled color).
     * @param value Current value (displayed in theme text color).
     */
        static void Property(const char* label, const char* value);

        /**
     * @brief Render a numeric property with a color indicator dot.
     * @param label Property name.
     * @param value Current value.
     * @param dot_color Optional color for the indicator dot.
     */
        static void Property(const char* label, float value, ImVec4 dot_color = ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

        /**
     * @brief Render a boolean property with a checkbox.
     * @param label Property name.
     * @param value Current value.
     * @return true if changed.
     */
        static bool PropertyBool(const char* label, bool* value);

        // ======================================================================
        // Tree / Hierarchy
        // ======================================================================

        /**
     * @brief Render a tree node with consistent styling.
     * @param label Node label (must include ##unique ID).
     * @param flags ImGuiTreeNodeFlags to combine (default: DefaultOpen).
     * @return true if the node is currently open.
     */
        static bool TreeNode(const char* label, ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen);

        /**
     * @brief Render a tree node with an icon prefix.
     * @param icon Unicode icon character.
     * @param label Node label.
     * @param flags ImGuiTreeNodeFlags.
     * @return true if the node is currently open.
     */
        static bool TreeNode(const char* icon, const char* label, ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen);

        /**
     * @brief Render a selectable tree item (clickable row).
     * @param label Item label.
     * @param selected Whether this item is currently selected.
     * @return true if clicked.
     */
        static bool Selectable(const char* label, bool selected = false, ImGuiSelectableFlags flags = ImGuiSelectableFlags_None);

        // ======================================================================
        // Input Controls
        // ======================================================================

        /**
     * @brief Render a styled input text field with consistent padding.
     * @param label Label shown before the input (must include ##unique ID).
     * @param buffer Input buffer.
     * @param buf_size Buffer size.
     */
        static bool InputText(const char* label, char* buffer, int buf_size);

        /**
     * @brief Render a float input with a drag slider.
     * @param label Label (must include ##unique ID).
     * @param value Current value.
     * @param speed Drag speed (default: 0.1f).
     * @param min Minimum value (default: -FLT_MAX).
     * @param max Maximum value (default: FLT_MAX).
     * @return true if changed.
     */
        static bool DragFloat(const char* label, float* value, float speed = 0.1f,
            float min = -1e10f, float max = 1e10f);

        /**
     * @brief Render a float3 input (vector).
     * @param label Label (must include ##unique ID).
     * @param value Array of 3 floats.
     * @param speed Drag speed.
     * @return true if changed.
     */
        static bool DragFloat3(const char* label, float* value, float speed = 0.1f);

        /**
     * @brief Render a float3 input (vector) with min/max bounds.
     * @param label Label (must include ##unique ID).
     * @param value Array of 3 floats.
     * @param speed Drag speed.
     * @param min Minimum value.
     * @param max Maximum value.
     * @return true if changed.
     */
        static bool DragFloat3(const char* label, float* value, float speed, float min, float max);

        /**
     * @brief Render an integer slider with a drag slider.
     * @param label Label (must include ##unique ID).
     * @param value Current value.
     * @param min Minimum value.
     * @param max Maximum value.
     * @return true if changed.
     */
        static bool SliderInt(const char* label, int* value, int min, int max);

        /**
     * @brief Render an integer input field.
     * @param label Label (must include ##unique ID).
     * @param value Current value.
     * @param step Step increment (default: 1).
     * @param stepFast Step increment with Shift (default: 100).
     * @return true if changed.
     */
        static bool InputInt(const char* label, int* value, int step = 1, int stepFast = 100);

        /**
     * @brief Render a float input field with formatting.
     * @param label Label (must include ##unique ID).
     * @param value Current value.
     * @param step Float step increment.
     * @param stepFast Float step with Shift.
     * @param format printf-style format string (default: "%.3f").
     * @return true if changed.
     */
        static bool InputFloat(const char* label, float* value, float step = 0.0f, float stepFast = 0.0f, const char* format = "%.3f");

        /**
     * @brief Render a color input with a color preview square.
     * @param label Label (must include ##unique ID).
     * @param col Array of 3 floats (RGB).
     * @return true if changed.
     */
        static bool ColorEdit3(const char* label, float* col);

        /**
     * @brief Render a combo box with consistent styling.
     * @param label Label (must include ##unique ID).
     * @param current_index Current selected index.
     * @param items Null-terminated list of items.
     * @param count Number of items in the array.
     * @return true if changed.
     */
        static bool Combo(const char* label, int* current_index, const char* const items[], int count);

        /**
     * @brief Render a combo box with a callback for item count.
     * @param label Label (must include ##unique ID).
     * @param current_index Current selected index.
     * @param get_items_callback Callback to get item count and names.
     * @return true if changed.
     */
        static bool Combo(const char* label, int* current_index,
         const std::function<int(const char* const*& out_items)>& get_items_callback);

        /**
     * @brief Render a checkbox with consistent spacing.
     * @param label Label (must include ##unique ID).
     * @param value Current value.
     * @return true if changed.
     */
        static bool Checkbox(const char* label, bool* value);

        // ======================================================================
        // Display Helpers
        // ======================================================================

        /**
     * @brief Render text in the theme's disabled color.
     * @param text Text to render.
     */
        static void TextDisabled(const char* text);

        /**
     * @brief Render text in the theme's disabled color with a tooltip.
     * @param text Text to render.
     * @param tooltip Tooltip text shown on hover.
     */
        static void TextDisabledWithTooltip(const char* text, const char* tooltip);

        /**
     * @brief Render text in the theme's accent color.
     * @param text Text to render.
     */
        static void TextAccent(const char* text);

        /**
     * @brief Render text in a specific color.
     * @param text Text to render.
     * @param color ImVec4 color.
     */
        static void TextColored(const char* text, ImVec4 color);

        /**
     * @brief Render a progress bar with consistent sizing.
     * @param fraction Progress fraction (0.0 to 1.0).
     * @param width Bar width (-1 = fill available space).
     */
        static void ProgressBar(float fraction, ImVec2 size = ImVec2(-1.0f, 0.0f));

        /**
     * @brief Render a small info icon with tooltip.
     * @param tooltip Tooltip text.
     */
        static void InfoTooltip(const char* tooltip);

        /**
     * @brief Render a warning icon with tooltip.
     * @param tooltip Tooltip text.
     */
        static void WarningTooltip(const char* tooltip);

        // ======================================================================
        // Layout Helpers
        // ======================================================================

        /**
     * @brief Push the current theme style vars onto the ImGui stack.
     * Call before panel rendering.
     */
        static void PushThemeStyle();

        /**
     * @brief Pop the theme style vars from the ImGui stack.
     * Call after panel rendering.
     */
        static void PopThemeStyle();

        /**
     * @brief Get the current frame time color (green/yellow/red).
     */
        static ImVec4 GetFrameTimeColor(float frameTimeMs);

        /**
     * @brief Get the accent color for the current theme.
     */
        static ImVec4 GetAccentColor();

        /**
     * @brief Get the disabled text color for the current theme.
     */
        static ImVec4 GetDisabledColor();

        /**
     * @brief Get the border color for the current theme.
     */
        static ImVec4 GetBorderColor();

        /**
     * @brief Get the separator color for the current theme.
     */
        static ImVec4 GetSeparatorColor();

        /**
     * @brief Get the text color for the current theme.
     */
        static ImVec4 GetTextColor();

        // ======================================================================
        // Popup / Menu Helpers
        // ======================================================================

        /**
     * @brief Render a small button that opens a popup.
     * @param label Button text (must include ##unique ID).
     * @param popup_id Popup ID (must match BeginPopup call).
     * @return true if clicked.
     */
        static bool PopupButton(const char* label, const char* popup_id);

        /**
     * @brief Render a "Add" button with a + icon.
     * @param label Button text (must include ##unique ID).
     * @return true if clicked.
     */
        static bool AddButton(const char* label);

        /**
     * @brief Render a "Delete" button with an X icon.
     * @param label Button text (must include ##unique ID).
     * @return true if clicked.
     */
        static bool DeleteButton(const char* label);

        /**
     * @brief Render a reset button.
     * @param label Button text (must include ##unique ID).
     * @return true if clicked.
     */
        static bool ResetButton(const char* label);

      // ======================================================================
      // Scene Panel Facade
      // ======================================================================

      static SceneEntityCollection CollectSceneEntities(const engine::Scene& scene);

      static void EnforceSingleDirectionalLight(
         std::vector<entt::entity>& dirLights,
         std::vector<entt::entity>& toDelete);

      static void DrawSceneCameraSection(
         const std::vector<entt::entity>& cameras,
         const char* filter,
         FrameInfo& frameInfo,
         engine::Scene& scene,
         entt::registry& registry,
         std::vector<entt::entity>& toDelete);

      static void DrawSceneLightSection(
         const std::vector<entt::entity>& dirLights,
         const std::vector<entt::entity>& pointLights,
         const std::vector<entt::entity>& spotLights,
         const char* filter,
         FrameInfo& frameInfo,
         engine::Scene& scene,
         entt::registry& registry,
         std::vector<entt::entity>& toDelete);

      static void DrawSceneModelSection(
         const std::vector<entt::entity>& models,
         const char* filter,
         FrameInfo& frameInfo,
         engine::Scene& scene,
         entt::registry& registry,
         std::vector<entt::entity>& toDelete,
         ModelInsertionOptions::StaticColliderImportMode& colliderMode,
         std::function<void(
            const std::string&,
            const std::string&,
            const ModelInsertionOptions&,
            ModelInsertionOptions::StaticColliderImportMode)> enqueueModelLoad);

      static void DrawScenePendingLoadsSection(
         std::vector<ScenePendingModelLoad>& pendingLoads,
         ResourceManager* resourceManager);

      static bool ShouldCreateStaticCollider(
         const std::string& path,
         const std::string& name,
         ModelInsertionOptions::StaticColliderImportMode mode);

       private:
        // Helper to apply section header styling
        static void ApplySectionStyle(bool open);
    };

}  // namespace engine::ui

#endif  // EDITOR_UI_ABSTRACTION_HPP
