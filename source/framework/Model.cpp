#include "Model.h"

float getValue(const char* src, const unsigned int index, const AttributeFormat attFormat) {
  switch (attFormat) {
  case ATT_FLOAT:         return *(((float*)src) + index);
  case ATT_UNSIGNED_BYTE: return *(((unsigned char*)src) + index) * (1.0f / 255.0f);
  default:
    return 0;
  }
}

void setValue(const char* dest, const unsigned int index, const AttributeFormat attFormat, float value) {
  switch (attFormat) {
  case ATT_FLOAT:
    *(((float*)dest) + index) = value;
    break;
  case ATT_UNSIGNED_BYTE:
    *(((unsigned char*)dest) + index) = (unsigned char)(value * 255.0f);
    break;
  }
}

bool findAttribute(const Batch& batch, const AttributeType attType, const unsigned int index, unsigned int* where) {
  for (unsigned int i = 0; i < batch.formats.size(); i++) {
    if (batch.formats[i].attType == attType && batch.formats[i].index == index) {
      if (where != NULL) *where = i;
      return true;
    }
  }
  return false;
}

bool transform_batch(Batch& batch, const mat4& mat, const AttributeType attType, const unsigned int index) {
  AttributeFormat format;
  unsigned int i, j, offset, size;
  if (!findAttribute(batch, attType, index, &offset)) return false;
  size = batch.formats[offset].size;
  format = batch.formats[offset].attFormat;
  offset = batch.formats[offset].offset;

  for (i = 0; i < batch.nVertices; i++) {
    char* src = batch.vertices + i * batch.vertexSize + offset;

    vec4 vec(0, 0, 0, 1);
    for (j = 0; j < size; j++) {
      vec.operator [](j) = getValue(src, j, format);
    }
    vec = mat * vec;
    for (j = 0; j < size; j++) {
      setValue(src, j, format, vec.operator [](j));
    }
  }

  return true;
}

bool transform_model(Model& ret_model, const mat4& mat) {
  for (Batch& batch : ret_model.batches) {
    if (!transform_batch(batch, mat, ATT_VERTEX, 0)) {
      return false;
    }
  }
  return true;
}

bool get_bounding_box(const Batch& batch, vec3& min, vec3& max) {

  unsigned int attribIndex = 0;
  if (!findAttribute(batch, ATT_VERTEX, 0, &attribIndex)) return false;
  uint32_t size          = batch.formats[attribIndex].size;
  AttributeFormat format = batch.formats[attribIndex].attFormat;
  uint32_t offset        = batch.formats[attribIndex].offset;

  if (size != 3) {
    return false;
  }

  for (uint32_t i = 0; i < batch.nVertices; i++) {
    char* src = batch.vertices + i * batch.vertexSize + offset;

    for (uint32_t j = 0; j < size; j++) {
      float val = getValue(src, j, format);
      if (val > max[j]) max[j] = val;
      if (val < min[j]) min[j] = val;
    }
  }

  return true;
}

bool get_bounding_box(const Model& model, vec3& min, vec3& max) {

  min = vec3(FLT_MAX);
  max = vec3(-FLT_MAX);

  for (const Batch& batch : model.batches) {
    if (!get_bounding_box(batch, min, max)) {
      return false;
    }
  }
  return true;
}

void read_batch_from_file(FILE* file, Batch& batch) {
  fread(&batch.nVertices, sizeof(batch.nVertices), 1, file);
  fread(&batch.nIndices, sizeof(batch.nIndices), 1, file);
  fread(&batch.vertexSize, sizeof(batch.vertexSize), 1, file);
  fread(&batch.indexSize, sizeof(batch.indexSize), 1, file);

  fread(&batch.primitiveType, sizeof(batch.primitiveType), 1, file);

  unsigned int nFormats;
  fread(&nFormats, sizeof(nFormats), 1, file);
  batch.formats.resize(nFormats);
  fread(batch.formats.data(), nFormats * sizeof(Format), 1, file);

  batch.vertices = new char[batch.nVertices * batch.vertexSize];
  fread(batch.vertices, batch.nVertices * batch.vertexSize, 1, file);

  if (batch.nIndices > 0) {
    batch.indices = new char[batch.nIndices * batch.indexSize];
    fread(batch.indices, batch.nIndices * batch.indexSize, 1, file);
  }
  else batch.indices = NULL;
}

bool make_model_renderable(Model& ret_model) {

  for (Batch& batch : ret_model.batches) {
    sg_range index_range = sg_range{ .ptr = batch.indices, .size = (batch.nIndices * batch.indexSize) };
    batch.render_index = sg_make_buffer(sg_buffer_desc{
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .data = index_range,
      });

    sg_range vertex_range = sg_range{ .ptr = batch.vertices, .size = (batch.nVertices * batch.vertexSize) };
    batch.render_vertex = sg_make_buffer(sg_buffer_desc{
        .data = vertex_range,
      });
    if (batch.render_index.id == SG_INVALID_ID ||
      batch.render_vertex.id == SG_INVALID_ID) {
      return false;
    }
  }
  return true;
}

bool load_model_from_file(const char* fileName, Model& ret_model) {
  FILE* file = fopen(fileName, "rb");
  if (file == NULL) return false;

  uint32_t version;
  fread(&version, sizeof(version), 1, file);
  uint32_t nBatches;
  fread(&nBatches, sizeof(nBatches), 1, file);

  for (unsigned int i = 0; i < nBatches; i++) {
    Batch batch = {};
    read_batch_from_file(file, batch);
    ret_model.batches.push_back(batch);
  }

  fclose(file);

  return true;
}
