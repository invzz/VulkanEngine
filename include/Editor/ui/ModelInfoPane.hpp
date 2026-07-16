#pragma once

#include <string>
#include <vector>

#include "Engine/Graphics/Device.hpp"

#include "ModelLib/Resources/Texture.hpp"

namespace engine::ui {

    /// Cached screenshot + readme for a single model, kept alive for the
    /// lifetime of the pane instance (per-frame reuse, no global state).
    struct ModelInfoCache {
        std::string                      id;
        std::shared_ptr<engine::Texture> texture;
        VkDescriptorSet                  texID{VK_NULL_HANDLE};
        std::string                      readmeText;
        bool                             readmeLoaded{false};
    };

    /// Right-hand pane of the Add-Model popup: shows the screenshot + readme
    /// of the currently active (hovered / last-clicked) model.
    class ModelInfoPane {
       public:
        void Draw(engine::Device& device,
            const std::string&    id,
            const std::string&    label,
            const std::string&    screenshotRel,  // relative to MODEL_PATH
            const std::string&    readmeRel,      // relative to MODEL_PATH
            const std::vector<std::string>& /*screenshotList*/);

       private:
        ModelInfoCache cache_;
    };

}  // namespace engine::ui
