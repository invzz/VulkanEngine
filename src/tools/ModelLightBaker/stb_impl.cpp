// Include stb_image header but do NOT define STB_IMAGE_IMPLEMENTATION here.
// The implementation is provided by Engine's `Texture.cpp` (which defines STB_IMAGE_IMPLEMENTATION)
// Defining it here would cause duplicate symbol errors when tools link against libEngine.a.
#include <stb_image.h>
