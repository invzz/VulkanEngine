#pragma once

#include <string>

#include "Editor/ui/ModelInsertionOptions.hpp"

namespace engine {

    /// Pure business rules for deciding whether a loaded model should get a
    /// static collider. Kept out of the UI layer so the logic is unit-testable
    /// and the editor does not own gameplay rules.
    class StaticColliderRules {
       public:
        /// Returns whether a static collider should be created for a model at
        /// `path` named `name`, given the import `mode`.
        static bool ShouldCreate(const std::string& path, const std::string& name,
            ModelInsertionOptions::StaticColliderImportMode mode);

        /// Human-readable list of the auto-detection tokens, for tooltips.
        static std::string Tooltip();

       private:
        static bool AutoDetect(const std::string& path, const std::string& name);
    };

}  // namespace engine
