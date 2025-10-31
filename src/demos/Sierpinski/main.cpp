#include <cstdlib>
#include <iostream>
#include <stdexcept>

#include "app.hpp"

#ifndef SHADER_PATH
#define SHADER_PATH "assets/shaders/compiled/"
#endif

int main()
{
    engine::App app;

    try
    {
        app.run();
    }
    catch (const std::exception& e)
    {
        // Handle exceptions appropriately
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}