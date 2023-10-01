#ifndef _IMAGE_H_
#define _IMAGE_H_
#include "external/sokol_gfx.h"
#include <vector>

sg_image create_texture(const char* filename, std::vector<uint8_t>& loadbuffer, const sg_image_desc& img_desc);

#endif // _IMAGE_H_
