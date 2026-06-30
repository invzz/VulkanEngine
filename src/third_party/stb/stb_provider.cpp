// Single translation unit that defines all STB symbols so that static
// archive link-order doesn't cause unresolved stbi_* references from
// STB symbols (e.g., tinygltf).

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include <stb_image.h>
#include <stb_image_write.h>
