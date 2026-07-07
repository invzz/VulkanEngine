#include <cstdlib>
#include <exception>
#include <sstream>
#include <string>

#include "Engine/Core/Logger.hpp"

#include "Editor/app.hpp"
#ifndef SHADER_PATH
#define SHADER_PATH "assets/shaders/compiled/"
#endif
int main(int argc, char** argv) {
    bool        fullscreen            = false;
    bool        validationOverrideSet = false;
    bool        validationEnabled     = false;
    std::string modelPath;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--fullscreen" || arg == "-f") {
            fullscreen = true;
            continue;
        }
        if (arg == "--validation" || arg == "--validation=on" || arg == "--validation=1") {
            validationOverrideSet = true;
            validationEnabled     = true;
            continue;
        }
        if (arg == "--no-validation" || arg == "--validation=off" || arg == "--validation=0") {
            validationOverrideSet = true;
            validationEnabled     = false;
            continue;
        }
        // First positional argument that doesn't start with '-' is treated as
        // a model file path to load instead of scene.json.
        if (!arg.empty() && arg[0] != '-' && modelPath.empty()) {
            modelPath = arg;
            continue;
        }
    }
    if (validationOverrideSet) {
        engine::Device::setValidationLayersEnabledOverride(validationEnabled);
    } else {
        engine::Device::clearValidationLayersEnabledOverride();
    }
    engine::App app(fullscreen, std::move(modelPath));
    try {
        app.run();
    } catch (const std::exception& e) {
        engine::Logger::error(engine::LogChannel::General, "Error: ", e.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}