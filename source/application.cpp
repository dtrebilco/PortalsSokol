#include "external/sokol_app.h"
#include "external/sokol_gfx.h"
#include "external/sokol_glue.h"
#include "external/sokol_time.h"

#define STBI_ONLY_PNG
#define STB_IMAGE_IMPLEMENTATION
#include "external/stb_image.h"

#ifdef _MSC_VER
extern "C" __declspec(dllimport) void __stdcall OutputDebugStringA(const char* lpOutputString);
#endif

const uint32_t MAX_PFX_PARTICLES = 1200;
const uint32_t PFX_VERTEX_SIZE = (4 * 3 + 4 * 2 + 4 * 4);

#include "ParticleSystem.h"
#include "Vector.h"
#include <vector>

extern const char* vs_src2, * fs_src2;
extern const char* vs_src_pfx, * fs_src_pfx;

const int SAMPLE_COUNT = 4;

static uint64_t time = 0;

typedef struct {
    float aspect;
    float dummy[3];
} vs_params_t;

struct vs_room_params
{
  mat4 mvp;
  vec4 lightPos;
  vec4 camPos;
};

struct vs_pfx_params
{
  mat4 mvp;
};

bool is_power2(unsigned int v) {
  return v && ((v & (v - 1)) == 0);
}

bool is_mipmap_filter(sg_filter filter) {
  return (filter == SG_FILTER_NEAREST_MIPMAP_NEAREST ||
          filter == SG_FILTER_NEAREST_MIPMAP_LINEAR ||
          filter == SG_FILTER_LINEAR_MIPMAP_NEAREST ||
          filter == SG_FILTER_LINEAR_MIPMAP_LINEAR);
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

void build_mipmapRGBA8(unsigned char* dest, unsigned char* src, int width, int height) {
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

sg_image create_texture(const char *filename, const sg_image_desc& img_desc) {

  sg_image_desc local_desc = img_desc;
  int texN = 0;
  uint8_t* texData = stbi_load(filename, &local_desc.width, &local_desc.height, &texN, 4);
  if (texData == nullptr) {
    return sg_image{};
  }

  // If bumpmap->normalmap, convert here

  // Create mip maps if needed
  local_desc.data.subimage[0][0] = { .ptr = texData, .size = size_t(local_desc.width) * local_desc.height * 4 };
  if (is_mipmap_filter(local_desc.min_filter) &&
    is_power2(local_desc.width) &&
    is_power2(local_desc.height)) {

    int mip_count = get_mipmap_count(local_desc.width, local_desc.height);
    if (mip_count <= SG_MAX_MIPMAPS) {
      local_desc.num_mipmaps = mip_count;

      int w = local_desc.width;
      int h = local_desc.height;
      for (int i = 1; i < mip_count; i++) {
        int old_w = w;
        int old_h = h;

        if (w > 1) { w >>= 1; }
        if (h > 1) { h >>= 1; }

        size_t newSize = size_t(w) * h * 4;
        local_desc.data.subimage[0][i] = { .ptr = malloc(newSize), .size = newSize };

        build_mipmapRGBA8((unsigned char*)local_desc.data.subimage[0][i].ptr, 
                          (unsigned char*)local_desc.data.subimage[0][i - 1].ptr, old_w, old_h);
      }
    }
  }
  // DT_TODO: Fail if cannot create requested mips?

  sg_image tex = sg_make_image(local_desc);
  stbi_image_free(texData);

  // Free allocated mipmaps
  for (int i = 1; i < local_desc.num_mipmaps; i++)
  {
    free((void*)local_desc.data.subimage[0][i].ptr);
  }

  return tex;
}

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

struct scene_data
{
  vec3 camPos = vec3(470, 220, 210);
  float wx = 0;
  float wy = PI / 2;
  float wz = 0;

  bool key_leftKey = false;
  bool key_rightKey = false;
  bool key_downKey = false;
  bool key_upKey = false;
  bool key_backwardKey = false;
  bool key_forwardKey = false;

  Model models[5];
  ParticleSystem particles;

  sg_shader shader = {};
  sg_image base[3] = {};
  sg_image bump[3] = {};
  sg_pipeline room_pipline = {};

  sg_shader pfx_shader = {};
  sg_image pfx_particle = {};
  sg_pipeline pfx_pipline = {};

  sg_buffer pfx_index;
  sg_buffer pfx_vertex;
};
scene_data SceneData = {};

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

bool findAttribute(Batch& batch, const AttributeType attType, const unsigned int index, unsigned int* where) {
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

void init(void) {

    sg_setup(sg_desc{ .context = sapp_sgcontext() });

    {
      std::vector<uint16_t> indices;
      indices.resize(MAX_PFX_PARTICLES * 6);
      uint16_t* dest = indices.data();
      for (unsigned int i = 0; i < MAX_PFX_PARTICLES; i++) {
        *dest++ = 4 * i;
        *dest++ = 4 * i + 1;
        *dest++ = 4 * i + 3;
        *dest++ = 4 * i + 2;
        *dest++ = 4 * i + 3;
        *dest++ = 4 * i + 1;
      }
      SceneData.pfx_index = sg_make_buffer(sg_buffer_desc{
          .type = SG_BUFFERTYPE_INDEXBUFFER,
          .data = sg_range{.ptr = indices.data(), .size = (indices.size() * sizeof(uint16_t)) } ,
        });
    }
    SceneData.pfx_vertex = sg_make_buffer(sg_buffer_desc{
        .size = MAX_PFX_PARTICLES * PFX_VERTEX_SIZE * 4,
        .usage = SG_USAGE_STREAM
      });

    ParticleSystem& particles = SceneData.particles;
    particles.setSpawnRate(400);
    particles.setSpeed(70, 20);
    particles.setLife(3.0f, 0);
    particles.setDirectionalForce(vec3(0, -10, 0));
    particles.setFrictionFactor(0.95f);
    //particles.setPosition(pos);
    particles.setSize(15, 5);

    for (unsigned int i = 0; i < 6; i++) {
      particles.setColor(i, vec4(0.05f * i, 0.01f * i, 0, 0));
      particles.setColor(6 + i, vec4(0.05f * 6, 0.05f * i + 0.06f, 0.02f * i, 0));
    }

    // create an image 
    sg_image_desc imageDesc = {
      .min_filter = SG_FILTER_LINEAR_MIPMAP_NEAREST,
      .mag_filter = SG_FILTER_LINEAR,
      .wrap_u = SG_WRAP_REPEAT,
      .wrap_v = SG_WRAP_REPEAT,
    };

    {
      sg_shader_desc shaderDesc = {};
      shaderDesc.attrs[0] = { .name = "position" };
      shaderDesc.attrs[1] = { .name = "uv" };
      shaderDesc.attrs[2] = { .name = "mat0" };
      shaderDesc.attrs[3] = { .name = "mat1" };
      shaderDesc.attrs[4] = { .name = "mat2" };

      shaderDesc.vs.source = vs_src2;
      shaderDesc.vs.entry = "main";
      shaderDesc.vs.uniform_blocks[0].size = 6 * 16;
      shaderDesc.vs.uniform_blocks[0].layout = SG_UNIFORMLAYOUT_STD140;
      shaderDesc.vs.uniform_blocks[0].uniforms[0].name = "vs_params";
      shaderDesc.vs.uniform_blocks[0].uniforms[0].type = SG_UNIFORMTYPE_FLOAT4;
      shaderDesc.vs.uniform_blocks[0].uniforms[0].array_count = 6;
      
      shaderDesc.fs.source = fs_src2;
      shaderDesc.fs.entry = "main";
      shaderDesc.fs.images[0].name = "Base";
      shaderDesc.fs.images[0].image_type = SG_IMAGETYPE_2D;
      shaderDesc.fs.images[0].sampler_type = SG_SAMPLERTYPE_FLOAT;
      shaderDesc.fs.images[1].name = "Bump";
      shaderDesc.fs.images[1].image_type = SG_IMAGETYPE_2D;
      shaderDesc.fs.images[1].sampler_type = SG_SAMPLERTYPE_FLOAT;

      SceneData.shader = sg_make_shader(shaderDesc);
    }

    {
      sg_shader_desc shaderDesc = {};
      shaderDesc.attrs[0] = { .name = "position" };
      shaderDesc.attrs[1] = { .name = "in_uv" };
      shaderDesc.attrs[2] = { .name = "in_color" };

      shaderDesc.vs.source = vs_src_pfx;
      shaderDesc.vs.entry = "main";
      shaderDesc.vs.uniform_blocks[0].size = 4 * 16;
      shaderDesc.vs.uniform_blocks[0].layout = SG_UNIFORMLAYOUT_STD140;
      shaderDesc.vs.uniform_blocks[0].uniforms[0].name = "vs_params";
      shaderDesc.vs.uniform_blocks[0].uniforms[0].type = SG_UNIFORMTYPE_FLOAT4;
      shaderDesc.vs.uniform_blocks[0].uniforms[0].array_count = 4;

      shaderDesc.fs.source = fs_src_pfx;
      shaderDesc.fs.entry = "main";
      shaderDesc.fs.images[0].name = "tex0";
      shaderDesc.fs.images[0].image_type = SG_IMAGETYPE_2D;
      shaderDesc.fs.images[0].sampler_type = SG_SAMPLERTYPE_FLOAT;

      SceneData.pfx_shader = sg_make_shader(shaderDesc);
    }

    SceneData.base[0] = create_texture("data/Wood.png", imageDesc);
    SceneData.base[1] = create_texture("data/laying_rock7.png", imageDesc);
    SceneData.base[2] = create_texture("data/victoria.png", imageDesc);

    SceneData.bump[0] = create_texture("data/Wood_N.png", imageDesc);
    SceneData.bump[1] = create_texture("data/laying_rock7_N.png", imageDesc);
    SceneData.bump[2] = create_texture("data/victoria_N.png", imageDesc);

    sg_image_desc pfx_imageDesc = {
      .min_filter = SG_FILTER_LINEAR_MIPMAP_NEAREST,
      .mag_filter = SG_FILTER_LINEAR,
      .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
      .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
    };
    SceneData.pfx_particle = create_texture("data/Particle.png", pfx_imageDesc);

    sg_image tex = create_texture("data/laying_rock7Bump.png", imageDesc);

    auto load_model = [](const char* filename, Model& model, vec3 offset) {
      load_model_from_file(filename, model);

      //mat4 mat(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
      //mat.translate(offset);
      mat4 mat(
        vec4(1.0, 0.0, 0.0, 0.0),
        vec4(0.0, 1.0, 0.0, 0.0),
        vec4(0.0, 0.0, 1.0, 0.0),
        vec4(offset, 1.0));

      transform_model(model, mat);
      make_model_renderable(model);
    };

    load_model("data/room0.hmdl", SceneData.models[0], vec3(0, 256, 0));
    load_model("data/room0.hmdl", SceneData.models[1], vec3(-384, 256, 3072));
    load_model("data/room0.hmdl", SceneData.models[2], vec3(1536, 256, 2688));
    load_model("data/room0.hmdl", SceneData.models[3], vec3(-1024, -768, 2688));
    load_model("data/room0.hmdl", SceneData.models[4], vec3(-2304, 256, 2688));

    {
      sg_pipeline_desc roomPipDesc = {};
      roomPipDesc.layout.attrs[0] = { .offset = 0, .format = SG_VERTEXFORMAT_FLOAT3 }; // position
      roomPipDesc.layout.attrs[1] = { .offset = 12, .format = SG_VERTEXFORMAT_FLOAT2 }; // uv
      roomPipDesc.layout.attrs[2] = { .offset = 20, .format = SG_VERTEXFORMAT_FLOAT3 }; // mat0
      roomPipDesc.layout.attrs[3] = { .offset = 32, .format = SG_VERTEXFORMAT_FLOAT3 }; // mat1
      roomPipDesc.layout.attrs[4] = { .offset = 44, .format = SG_VERTEXFORMAT_FLOAT3 }; // mat2
      roomPipDesc.shader = SceneData.shader;
      roomPipDesc.index_type = SG_INDEXTYPE_UINT16;
      roomPipDesc.depth = {
          .compare = SG_COMPAREFUNC_LESS_EQUAL,
          .write_enabled = true,
      };
      roomPipDesc.cull_mode = SG_CULLMODE_BACK;
      SceneData.room_pipline = sg_make_pipeline(roomPipDesc);
    }

    {
      sg_pipeline_desc pipDesc = {};
      pipDesc.layout.attrs[0] = { .offset = 0, .format = SG_VERTEXFORMAT_FLOAT3 }; // position
      pipDesc.layout.attrs[1] = { .offset = 12, .format = SG_VERTEXFORMAT_FLOAT2 }; // uv
      pipDesc.layout.attrs[2] = { .offset = 20, .format = SG_VERTEXFORMAT_FLOAT4 }; // color
      pipDesc.shader = SceneData.pfx_shader;
      pipDesc.index_type = SG_INDEXTYPE_UINT16;
      pipDesc.depth = {
          .compare = SG_COMPAREFUNC_LESS_EQUAL,
          .write_enabled = false,
      };
      pipDesc.colors[0].blend = {
          .enabled = true,
          .src_factor_rgb = SG_BLENDFACTOR_ONE,
          .dst_factor_rgb = SG_BLENDFACTOR_ONE,
          .src_factor_alpha = SG_BLENDFACTOR_ONE,
          .dst_factor_alpha = SG_BLENDFACTOR_ONE,
      };
      pipDesc.cull_mode = SG_CULLMODE_BACK;
      SceneData.pfx_pipline = sg_make_pipeline(pipDesc);
    }
    
    stm_setup();

    uint64_t t0 = stm_now();
    uint64_t tdiff = stm_diff(stm_now(), t0);
    char buf[1000];
    snprintf(buf, sizeof(buf), "Initialize time: %.1fms\n", stm_ms(tdiff));
    #ifdef _MSC_VER
    OutputDebugStringA(buf);
    #else
    puts(buf);
    #endif
}

void controls(float frameTime) {
  // Compute directional vectors from euler angles
  float cosX = cosf(SceneData.wx), sinX = sinf(SceneData.wx), cosY = cosf(SceneData.wy), sinY = sinf(SceneData.wy);
  vec3 dx(cosY, 0, sinY);
  vec3 dy(-sinX * sinY, cosX, sinX * cosY);
  vec3 dz(-cosX * sinY, -sinX, cosX * cosY);

  vec3 dir(0, 0, 0);
  if (SceneData.key_leftKey)     dir -= dx;
  if (SceneData.key_rightKey)    dir += dx;
  if (SceneData.key_downKey)     dir -= dy;
  if (SceneData.key_upKey)       dir += dy;
  if (SceneData.key_backwardKey) dir -= dz;
  if (SceneData.key_forwardKey)  dir += dz;

  float lenSq = dot(dir, dir);
  if (lenSq > 0) {
    dir *= 1.0f / sqrtf(lenSq);
    float speed = 1000.0f;
    SceneData.camPos += dir * (frameTime * speed);
  }
}

void frame(void) {
    vs_room_params room_params;
    vs_pfx_params pfx_params;

    vs_params_t vs_params;
    const float w = (float) sapp_width();
    const float h = (float) sapp_height();
    vs_params.aspect = w / h;

    mat4 proj = perspectiveMatrixX(1.5f, w, h, 0.1f, 6000);
    mat4 mv = rotateXY(-SceneData.wx, -SceneData.wy) * translate(-SceneData.camPos);

    room_params.mvp = proj * mv;
    pfx_params.mvp = room_params.mvp;

    vec3 lightStartPos = vec3(0, 128, 0);
    float xs = 100;
    float ys = 100;
    float zs = 100;
    float t = stm_sec(time) - 0.1f; // DT_TODO: Bad usage of time?
    float j = 0;
    vec3 p = vec3(xs * cosf(4.23f * t + j), ys * sinf(2.37f * t) * cosf(1.39f * t), zs * sinf(3.12f * t + j));

    //room_params.lightPos = vec4(38.729336, 87.001053, 45.429482, 1.0);
    room_params.lightPos = vec4(lightStartPos + p, 1.0);
    room_params.camPos = vec4(SceneData.camPos, 1.0);

    t = stm_sec(time);
    p = vec3(xs * cosf(4.23f * t + j), ys * sinf(2.37f * t) * cosf(1.39f * t), zs * sinf(3.12f * t + j));
    SceneData.particles.setPosition(lightStartPos + p);
    SceneData.particles.update(t);

    vec3 dx(mv[0][0], mv[1][0], mv[2][0]);
    vec3 dy(mv[0][1], mv[1][1], mv[2][1]);

    uint32_t pfxCount = SceneData.particles.getParticleCount();
    if (pfxCount > MAX_PFX_PARTICLES)
    {
      pfxCount = MAX_PFX_PARTICLES;
    }
    if (pfxCount > 0)
    {
      sg_update_buffer(SceneData.pfx_vertex, sg_range{ .ptr = SceneData.particles.getVertexArray(dx, dy), .size = pfxCount * PFX_VERTEX_SIZE * 4 });
    }

    uint64_t dt = stm_laptime(&time);
    controls(stm_sec(dt));

    sg_pass_action pass_action = {};
    pass_action.colors[0] = { .action = SG_ACTION_CLEAR, .value = { 0.1f, 0.1f, 0.1f, 1.0f } };

    sg_begin_default_pass(&pass_action, (int)w, (int)h);

    sg_apply_pipeline(SceneData.room_pipline);
    sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, SG_RANGE_REF(room_params));
    for (int i = 0; i < 3; i++)
    {
      sg_bindings binding = {};
      binding.index_buffer = SceneData.models[0].batches[i].render_index;
      binding.vertex_buffers[0] = SceneData.models[0].batches[i].render_vertex;
      binding.fs_images[0] = SceneData.base[i];
      binding.fs_images[1] = SceneData.bump[i];
      sg_apply_bindings(&binding);
      sg_draw(0, SceneData.models[0].batches[i].nIndices, 1);
    }

    if (pfxCount > 0)
    {
      sg_apply_pipeline(SceneData.pfx_pipline);
      sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, SG_RANGE_REF(pfx_params));
      sg_bindings binding = {};
      binding.index_buffer = SceneData.pfx_index;
      binding.vertex_buffers[0] = SceneData.pfx_vertex;
      binding.fs_images[0] = SceneData.pfx_particle;
      sg_apply_bindings(&binding);
      sg_draw(0, 6 * pfxCount, 1);
    }

    sg_end_pass();
    sg_commit();
}

void cleanup(void) {

    sg_shutdown();
}

static void input(const sapp_event* ev) {
  switch (ev->type) {
  case SAPP_EVENTTYPE_MOUSE_DOWN:
    if (ev->mouse_button == SAPP_MOUSEBUTTON_LEFT) {
      sapp_lock_mouse(true);
    }
    break;
  case SAPP_EVENTTYPE_MOUSE_UP:
    if (ev->mouse_button == SAPP_MOUSEBUTTON_LEFT) {
      sapp_lock_mouse(false);
    }
    break;
  case SAPP_EVENTTYPE_MOUSE_SCROLL:
    //cam_zoom(cam, ev->scroll_y * 0.5f); //DT_TODO: Adjust speed here?
    break;
  case SAPP_EVENTTYPE_MOUSE_MOVE:
    if (sapp_mouse_locked()) {
      float mouseSensibility = 0.003f;
      SceneData.wx -= mouseSensibility * ev->mouse_dy;
      SceneData.wy -= mouseSensibility * ev->mouse_dx;
    }
    break;

  case SAPP_EVENTTYPE_KEY_DOWN:
    if (ev->key_code == SAPP_KEYCODE_ESCAPE)
    {
      sapp_request_quit();
    }
    break;
  default:
    break;
  }

  if (ev->type == SAPP_EVENTTYPE_KEY_DOWN ||
      ev->type == SAPP_EVENTTYPE_KEY_UP) {
    if (!ev->key_repeat) {
      bool pressed = (ev->type == SAPP_EVENTTYPE_KEY_DOWN);

      switch (ev->key_code) {
      case SAPP_KEYCODE_W: SceneData.key_forwardKey = pressed;  break;
      case SAPP_KEYCODE_S: SceneData.key_backwardKey = pressed; break;
      case SAPP_KEYCODE_A: SceneData.key_leftKey = pressed;     break;
      case SAPP_KEYCODE_D: SceneData.key_rightKey = pressed;    break;
      }
    }
  }
}

sapp_desc sokol_main(int argc, char* argv[]) {
    return sapp_desc{
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = input,
        .width = 800,
        .height = 600,
        .sample_count = SAMPLE_COUNT,
        .window_title = "Portals",
    };
}

#if defined(SOKOL_GLCORE33)

const char* vs_src2 = R"(
#version 330
out vec2 texCoord;
out vec3 lightVec;
out vec3 viewVec;

uniform vec4 vs_params[6];

layout(location = 0) in vec4 position;
layout(location = 1) in vec2 uv;
layout(location = 2) in vec3 mat0;
layout(location = 3) in vec3 mat1;
layout(location = 4) in vec3 mat2;

void main() {

  mat4 mvp = mat4(vs_params[0], vs_params[1], vs_params[2], vs_params[3]);
  gl_Position = mvp * position;

  texCoord = uv.xy;

  vec3 lightPos = vs_params[4].xyz;
  vec3 camPos = vs_params[5].xyz;

  vec3 lVec = lightPos - position.xyz;
  lightVec.x = dot(mat0.xyz, lVec);
  lightVec.y = dot(mat1.xyz, lVec);
  lightVec.z = dot(mat2.xyz, lVec);

  vec3 vVec = camPos - position.xyz;
  viewVec.x = dot(mat0.xyz, vVec);
  viewVec.y = dot(mat1.xyz, vVec);
  viewVec.z = dot(mat2.xyz, vVec);
}
)";

const char* fs_src2 = R"(
#version 330
uniform sampler2D Base;
uniform sampler2D Bump;

//uniform float invRadius;
//uniform float ambient;

float invRadius = 0.001250;
float ambient = 0.07;

in vec2 texCoord;
in vec3 lightVec;
in vec3 viewVec;

layout(location = 0) out vec4 frag_color;

void main(){
	vec4 base = texture(Base, texCoord);
	vec3 bump = texture(Bump, texCoord).xyz * 2.0 - 1.0;

	bump = normalize(bump);

	float distSqr = dot(lightVec, lightVec);
	vec3 lVec = lightVec * inversesqrt(distSqr);

	float atten = clamp(1.0 - invRadius * sqrt(distSqr), 0.0, 1.0);
	float diffuse = clamp(dot(lVec, bump), 0.0, 1.0);

	float specular = pow(clamp(dot(reflect(normalize(-viewVec), bump), lVec), 0.0, 1.0), 16.0);
	
	frag_color = ambient * base + (diffuse * base + 0.6 * specular) * atten;
}
)";


const char* vs_src_pfx = R"(
#version 330
uniform vec4 vs_params[4];
layout(location = 0) in vec4 position;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec4 in_color;
out vec2 uv;
out vec4 color;
void main() {

  // Position it
  mat4 mvp = mat4(vs_params[0], vs_params[1], vs_params[2], vs_params[3]);
  gl_Position = mvp * position;

  uv = in_uv;
  color = in_color;
}

)";

const char* fs_src_pfx = R"(
#version 330
layout(location = 0) out vec4 frag_color;
in vec2 uv;
in vec4 color;
uniform sampler2D tex0;
void main() {
  frag_color = texture(tex0, uv);
  frag_color *= color;
}
)";

#else
#error Unknown graphics plaform
#endif
