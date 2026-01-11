// Single translation unit providing STB implementations for the whole project
// This avoids link-order issues with multiple static libraries referencing
// STB symbols (e.g., tinygltf).

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include <stb_image.h>
#include <stb_image_write.h>
