#include <cstdlib>
#include <exception>
#include <iostream>

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
    std::cerr << "Error: " << e.what() << '\n';
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}