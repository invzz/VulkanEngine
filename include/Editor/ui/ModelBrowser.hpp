#pragma once

#include <functional>
#include <string>

#include "Engine/Graphics/Device.hpp"

#include "Editor/ui/ModelInfoPane.hpp"
#include "Editor/ui/ModelInsertionOptions.hpp"

namespace engine::ui {

    /// "Add Model" popup browser. Owns the popup state (filter, active model
    /// for the info pane) as instance state instead of file-scope statics, and
    /// delegates screenshot/readme rendering to ModelInfoPane.
    class ModelBrowser {
       public:
        explicit ModelBrowser(engine::Device& device);

        /// Open the popup (call after the "+" add button in the model section).
        void Open();

        /// Render the popup if open. `loader` enqueues a model load.
        void Draw(ModelInsertionOptions::StaticColliderImportMode& mode,
            std::function<void(const std::string&, const std::string&,
                const ModelInsertionOptions&, ModelInsertionOptions::StaticColliderImportMode)>
                loader);

       private:
        engine::Device& device_;
        ModelInfoPane   infoPane_;
        std::string     activeModelId_;
        std::string     activeLabel_;
        std::string     activeScreenshot_;
        std::string     activeReadme_;
        char            filterModel_[128] = "";
        char            customPath_[512]  = "";
    };

}  // namespace engine::ui
