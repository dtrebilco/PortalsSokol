#ifndef _IMAGE_H_
#define _IMAGE_H_
#include "external/sokol_gfx.h"
#include <vector>

sg_image create_texture(const char* filename, std::vector<uint8_t>& loadbuffer, bool useMipmaps = true);

#endif // _IMAGE_H_
