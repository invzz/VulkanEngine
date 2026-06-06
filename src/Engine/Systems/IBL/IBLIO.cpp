#include <filesystem>
#include <string>
#include <system_error>

#include "Engine/Systems/IBL/BRDFLUT.hpp"
#include "Engine/Systems/IBL/IrradianceIBL.hpp"
#include "Engine/Systems/IBL/PrefilteredEnvIBL.hpp"
#include "Engine/Systems/IBL/VTexIO.hpp"
#include "Engine/Systems/IBLSystem.hpp"

namespace engine {

    bool IBLSystem::saveToDisk(const std::string& directory) const {
        namespace fs = std::filesystem;

        std::error_code ec;
        fs::create_directories(fs::path(directory), ec);
        if (ec) {
            return false;
        }

        // Note: sizes are implicit from settings for generated images.
        // Irradiance/prefilter are cubemaps with 6 layers.
        bool           ok = true;
        fs::path const dirPath(directory);
        ok = ok && ibl_detail::vtex::writeImage(device_,
                       (dirPath / "irradiance.vtex").generic_string(),
                       irradiance_->image(),
                       VK_FORMAT_R32G32B32A32_SFLOAT,
                       static_cast<uint32_t>(settings_.irradianceSize),
                       static_cast<uint32_t>(settings_.irradianceSize),
                       1,
                       6);
        ok = ok && ibl_detail::vtex::writeImage(device_,
                       (dirPath / "prefilter.vtex").generic_string(),
                       prefiltered_->image(),
                       VK_FORMAT_R16G16B16A16_SFLOAT,
                       static_cast<uint32_t>(settings_.prefilterSize),
                       static_cast<uint32_t>(settings_.prefilterSize),
                       static_cast<uint32_t>(settings_.prefilterMipLevels),
                       6);
        ok = ok && ibl_detail::vtex::writeImage(device_,
                       (dirPath / "brdf_lut.vtex").generic_string(),
                       brdfLUT_->image(),
                       VK_FORMAT_R16G16B16A16_SFLOAT,
                       static_cast<uint32_t>(settings_.brdfLUTSize),
                       static_cast<uint32_t>(settings_.brdfLUTSize),
                       1,
                       1);
        return ok;
    }

    bool IBLSystem::loadFromDisk(const std::string& directory) {
        namespace fs = std::filesystem;

        fs::path const dirPath(directory);

        ibl_detail::vtex::Header irrH{};
        ibl_detail::vtex::Header preH{};
        ibl_detail::vtex::Header brdfH{};

        // Replace existing resources (deferred destroy) and adopt the loaded ones.
        irradiance_->deferDestroyImageResources();
        prefiltered_->deferDestroyImageResources();
        brdfLUT_->deferDestroyImageResources();

        VkImage        irrImage   = VK_NULL_HANDLE;
        VkDeviceMemory irrMemory  = VK_NULL_HANDLE;
        VkImageView    irrView    = VK_NULL_HANDLE;
        VkSampler      irrSampler = VK_NULL_HANDLE;

        VkImage        preImage   = VK_NULL_HANDLE;
        VkDeviceMemory preMemory  = VK_NULL_HANDLE;
        VkImageView    preView    = VK_NULL_HANDLE;
        VkSampler      preSampler = VK_NULL_HANDLE;

        VkImage        brdfImage   = VK_NULL_HANDLE;
        VkDeviceMemory brdfMemory  = VK_NULL_HANDLE;
        VkImageView    brdfView    = VK_NULL_HANDLE;
        VkSampler      brdfSampler = VK_NULL_HANDLE;

        bool ok = true;
        ok      = ok && ibl_detail::vtex::loadImage(device_,
                            (dirPath / "irradiance.vtex").generic_string(),
                            irrImage,
                            irrMemory,
                            irrView,
                            irrSampler,
                            VK_IMAGE_VIEW_TYPE_CUBE,
                            VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
                            &irrH);
        ok      = ok && ibl_detail::vtex::loadImage(device_,
                            (dirPath / "prefilter.vtex").generic_string(),
                            preImage,
                            preMemory,
                            preView,
                            preSampler,
                            VK_IMAGE_VIEW_TYPE_CUBE,
                            VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
                            &preH);
        ok      = ok && ibl_detail::vtex::loadImage(device_, (dirPath / "brdf_lut.vtex").generic_string(), brdfImage, brdfMemory, brdfView, brdfSampler, VK_IMAGE_VIEW_TYPE_2D, 0, &brdfH);

        if (ok) {
            irradiance_->adoptLoaded(irrImage, irrMemory, irrView, irrSampler);
            prefiltered_->adoptLoaded(preImage, preMemory, preView, preSampler);
            brdfLUT_->adoptLoaded(brdfImage, brdfMemory, brdfView, brdfSampler, static_cast<int>(brdfH.width));

            generated_ = true;
            generationCounter_++;
        }

        return ok;
    }

}  // namespace engine
