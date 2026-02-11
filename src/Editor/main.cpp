#include <cstdlib>
#include <exception>
#include <iostream>

#include "app.hpp"

#ifndef SHADER_PATH
#define SHADER_PATH "assets/shaders/compiled/"
#endif

int main(int argc, char** argv)
{
  bool fullscreen = false;
  for (int i = 1; i < argc; ++i)
  {
    if (std::string(argv[i]) == "--fullscreen" || std::string(argv[i]) == "-f")
    {
      fullscreen = true;
      break;
    }
  }

  engine::App app(fullscreen);

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
