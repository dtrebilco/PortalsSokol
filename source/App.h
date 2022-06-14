#ifndef _APP_H_
#define _APP_H_

#include "BaseApp.h"
#include "ParticleSystem.h"

enum PrimitiveType : uint32_t {
  PRIM_TRIANGLES = 0,
  PRIM_QUADS = 1,
  PRIM_TRIANGLE_STRIP = 2,
  PRIM_LINES = 3,
};

enum AttributeType : uint32_t {
  ATT_VERTEX = 0,
  ATT_NORMAL = 1,
  ATT_TEXCOORD = 2,
  ATT_COLOR = 3,
};

enum AttributeFormat : uint32_t {
  ATT_FLOAT = 0,
  ATT_UNSIGNED_BYTE = 1,
};

struct Format {
  AttributeType attType;
  AttributeFormat attFormat;
  uint32_t size;
  uint32_t offset;
  uint32_t index;
};

struct Batch
{
  char* vertices;
  char* indices;

  uint32_t nVertices;
  uint32_t nIndices;
  uint32_t vertexSize;
  uint32_t indexSize;

  std::vector<Format> formats;
  PrimitiveType primitiveType;

  sg_buffer render_index;
  sg_buffer render_vertex;
};

struct Model
{
  std::vector<Batch> batches;
};


class App : public BaseApp
{
public:

  void ResetCamera() override;
  bool Load() override;
  void DrawFrame() override;

protected:

  Model models[5];
  ParticleSystem particles;

  sg_shader shader = {};
  sg_image base[3] = {};
  sg_image bump[3] = {};
  sg_pipeline room_pipline = {};

  sg_shader pfx_shader = {};
  sg_image pfx_particle = {};
  sg_pipeline pfx_pipline = {};

  sg_buffer pfx_index = {};
  sg_buffer pfx_vertex = {};

};


#endif // _APP_H_
