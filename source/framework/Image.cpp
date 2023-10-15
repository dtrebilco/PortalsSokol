#include "Image.h"

#define STBI_ONLY_PNG
#define STB_IMAGE_IMPLEMENTATION
#include "external/stb_image.h"

bool is_power2(unsigned int v) {
  return v && ((v & (v - 1)) == 0);
}

int get_mipmap_count(int width, int height) {
  int max = (width > height) ? width : height;
  int i = 0;

  while (max > 0) {
    max >>= 1;
    i++;
  }

  return i;
}

void build_mipmapRGBA8(uint8_t* dest, uint8_t* src, int width, int height) {
  int xOff = (width < 2) ? 0 : 4;
  int yOff = (height < 2) ? 0 : width * 4;

  for (int y = 0; y < height; y += 2) {
    for (int x = 0; x < width; x += 2) {
      for (int i = 0; i < 4; i++) {
        *dest = ((src[0] + src[xOff] + src[yOff] + src[yOff + xOff]) + 2) >> 2;
        dest++;
        src++;
      }
      src += xOff;
    }
    src += yOff;
  }
}

sg_image create_texture(const char* filename, std::vector<uint8_t>& loadbuffer, bool useMipmaps) {

  sg_image_desc local_desc = {};
  int texN = 0;
  uint8_t* texData = stbi_load(filename, &local_desc.width, &local_desc.height, &texN, 4);
  if (texData == nullptr) {
    return sg_image{};
  }

  // If bumpmap->normalmap, convert here

  // Create mip maps if needed
  local_desc.data.subimage[0][0] = { .ptr = texData, .size = size_t(local_desc.width) * local_desc.height * 4 };
  if (useMipmaps &&
    is_power2(local_desc.width) &&
    is_power2(local_desc.height)) {

    int mip_count = get_mipmap_count(local_desc.width, local_desc.height);
    if (mip_count <= SG_MAX_MIPMAPS) {
      local_desc.num_mipmaps = mip_count;

      // Calcuulate the required total size 
      size_t totalSize = 0;
      int w = local_desc.width;
      int h = local_desc.height;
      for (int i = 1; i < mip_count; i++) {
        if (w > 1) { w >>= 1; }
        if (h > 1) { h >>= 1; }
        totalSize += size_t(w) * h * 4;
      }

      loadbuffer.resize(totalSize);
      uint8_t* load_ptr = loadbuffer.data();

      // Build mip-maps
      w = local_desc.width;
      h = local_desc.height;
      for (int i = 1; i < mip_count; i++) {
        int old_w = w;
        int old_h = h;

        if (w > 1) { w >>= 1; }
        if (h > 1) { h >>= 1; }

        size_t newSize = size_t(w) * h * 4;
        local_desc.data.subimage[0][i] = { .ptr = load_ptr, .size = newSize };
        load_ptr += newSize;

        build_mipmapRGBA8((uint8_t*)local_desc.data.subimage[0][i].ptr,
                          (uint8_t*)local_desc.data.subimage[0][i - 1].ptr, old_w, old_h);
      }
    }
  }
  // DT_TODO: Fail if cannot create requested mips?

  sg_image tex = sg_make_image(local_desc);
  stbi_image_free(texData);

  return tex;
}
