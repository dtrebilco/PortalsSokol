#ifndef _MODEL_H_
#define _MODEL_H_

#include "external/sokol_gfx.h"
#include "Vector.h"
#include <vector>

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

bool load_model_from_file(const char* fileName, Model& ret_model);
bool make_model_renderable(Model& ret_model);

bool get_bounding_box(const Model& model, vec3& min, vec3& max);
bool transform_model(Model& ret_model, const mat4& mat);

#endif // _MODEL_H_
