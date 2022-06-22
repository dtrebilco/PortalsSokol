#ifndef _IMAGE_H_
#define _IMAGE_H_
#include "external/sokol_gfx.h"

sg_image create_texture(const char* filename, const sg_image_desc& img_desc);

#endif // _IMAGE_H_
