#ifndef VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_IBL_IBLSETTINGS_HPP
#define VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_IBL_IBLSETTINGS_HPP

namespace engine::ibl {

    struct Settings {
        int   irradianceSize        = 64;
        int   prefilterSize         = 256;
        int   prefilterMipLevels    = 8;
        int   brdfLUTSize           = 256;
        int   prefilterSampleCount  = 1024;
        float irradianceSampleDelta = 0.025f;
    };

}  // namespace engine::ibl

#endif  // VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_IBL_IBLSETTINGS_HPP
