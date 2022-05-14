#include "external/sokol_app.h"
#include "external/sokol_gfx.h"
#include "external/sokol_glue.h"
#include "external/sokol_time.h"

#define STB_IMAGE_IMPLEMENTATION
#include "external/stb_image.h"

#ifdef _MSC_VER
extern "C" __declspec(dllimport) void __stdcall OutputDebugStringA(const char* lpOutputString);
#endif

#include "game.h"

extern const char *vs_src, *fs_src;

struct scene_data
{
  sg_shader shader = {};
  sg_image base[3] = {};
  sg_image bump[3] = {};
  sg_image particle = {};
};
scene_data SceneData = {};

const int SAMPLE_COUNT = 4;
sg_bindings draw_state;
sg_pipeline pip;

static sprite_data_t* sprite_data;
static uint64_t time;

typedef struct {
    float aspect;
    float dummy[3];
} vs_params_t;

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

void init(void) {

    sg_setup(sg_desc{ .context = sapp_sgcontext() });

    // empty, dynamic instance-data vertex buffer
    sg_buffer instancebuf = sg_make_buffer(sg_buffer_desc{
        .size = kMaxSpriteCount * sizeof(sprite_data_t),
        .usage = SG_USAGE_STREAM
    });

    // create an index buffer for a quad 
    uint16_t indices[] = {
        0, 1, 2,  2, 1, 3,
    };
    sg_buffer ibuf = sg_make_buffer(sg_buffer_desc{
        .size = sizeof(indices),
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .data = SG_RANGE(indices),
    });

    // create shader
    sg_shader_desc shaderDesc = {};
    shaderDesc.vs.uniform_blocks[0] = {
            .size = sizeof(vs_params_t)
    };
    shaderDesc.attrs[0] = { .name = "posScale" };
    shaderDesc.attrs[1] = { .name = "colorIndex" };
    shaderDesc.fs.images[0].name = "tex0";
    shaderDesc.fs.images[0].image_type = SG_IMAGETYPE_2D;
    shaderDesc.fs.images[0].sampler_type = SG_SAMPLERTYPE_FLOAT;
    shaderDesc.vs.uniform_blocks[0].size = 16;
    shaderDesc.vs.uniform_blocks[0].layout = SG_UNIFORMLAYOUT_STD140;
    shaderDesc.vs.uniform_blocks[0].uniforms[0].name = "vs_params";
    shaderDesc.vs.uniform_blocks[0].uniforms[0].type = SG_UNIFORMTYPE_FLOAT4;
    shaderDesc.vs.uniform_blocks[0].uniforms[0].array_count = 1;
    shaderDesc.vs.source = vs_src;
    shaderDesc.vs.entry = "main"; 
    shaderDesc.fs.source = fs_src;
    shaderDesc.fs.entry = "main";
    sg_shader shd = sg_make_shader(shaderDesc);
    
    // create an image 
    sg_image_desc imageDesc = {
      .min_filter = SG_FILTER_LINEAR_MIPMAP_NEAREST,
      .mag_filter = SG_FILTER_LINEAR,
      .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
      .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
    };
    //sg_image tex = create_texture("data/sprites.png", imageDesc);

    sg_image tex = create_texture("data/laying_rock7Bump.png", imageDesc);

    // create pipeline object
    sg_pipeline_desc pipDesc = {};
    pipDesc.layout.buffers[0].step_func = SG_VERTEXSTEP_PER_INSTANCE;
    pipDesc.layout.attrs[0] = { .offset = 0, .format = SG_VERTEXFORMAT_FLOAT3 }; // instance pos + scale
    pipDesc.layout.attrs[1] = { .offset = 12, .format = SG_VERTEXFORMAT_FLOAT4 }; // instance color

    pipDesc.shader = shd;
    pipDesc.index_type = SG_INDEXTYPE_UINT16;
    pipDesc.depth = {
        .compare = SG_COMPAREFUNC_LESS_EQUAL,
        .write_enabled = true,
    };
    pipDesc.cull_mode = SG_CULLMODE_NONE;
    pipDesc.sample_count = SAMPLE_COUNT;
    pipDesc.colors[0].blend = {
        .enabled = true,
        .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
        .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
        .src_factor_alpha = SG_BLENDFACTOR_SRC_ALPHA,
        .dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
    };

    pip = sg_make_pipeline(pipDesc);

    // draw state struct with resource bindings
    draw_state = sg_bindings{};
    draw_state.vertex_buffers[0] = instancebuf;
    draw_state.index_buffer = ibuf;
    draw_state.fs_images[0] = tex;
    
    stm_setup();
    sprite_data = (sprite_data_t*)malloc(kMaxSpriteCount * sizeof(sprite_data_t));

    uint64_t t0 = stm_now();
    game_initialize();
    uint64_t tdiff = stm_diff(stm_now(), t0);
    char buf[1000];
    snprintf(buf, sizeof(buf), "Initialize time: %.1fms\n", stm_ms(tdiff));
    #ifdef _MSC_VER
    OutputDebugStringA(buf);
    #else
    puts(buf);
    #endif
}

void frame(void) {
    vs_params_t vs_params;
    const float w = (float) sapp_width();
    const float h = (float) sapp_height();
    vs_params.aspect = w / h;

    uint64_t dt = stm_laptime(&time);
    
    uint64_t t0 = stm_now();
    int sprite_count = game_update(sprite_data, stm_sec(time), (float)stm_sec(dt));
    uint64_t tdiff = stm_diff(stm_now(), t0);
    // print times that game update took (print only on frames that are powers of two, to not
    // spam the output)
    static int frameCount = 0;
    static int totalFrameCount = 0;
    static uint64_t frameTimes = 0;
    frameTimes += tdiff;
    frameCount++;
    totalFrameCount++;
    if ((totalFrameCount & (totalFrameCount - 1)) == 0 && totalFrameCount > 4)
    {
        char buf[1000];
        snprintf(buf, 1000, "Update time: %.1fms (%i sprites)\n", stm_ms(frameTimes) / frameCount, sprite_count);
        #ifdef _MSC_VER
        OutputDebugStringA(buf);
        #else
        puts(buf);
        #endif
        frameTimes = 0;
        frameCount = 0;
    }

    assert(sprite_count >= 0 && sprite_count <= kMaxSpriteCount);
    sg_update_buffer(draw_state.vertex_buffers[0], sg_range{ .ptr = sprite_data, .size = sprite_count * sizeof(sprite_data[0]) });

    sg_pass_action pass_action = {};
    pass_action.colors[0] = { .action = SG_ACTION_CLEAR, .value = { 0.1f, 0.1f, 0.1f, 1.0f } };

    sg_begin_default_pass(&pass_action, (int)w, (int)h);
    sg_apply_pipeline(pip);
    sg_apply_bindings(&draw_state);
    sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, SG_RANGE_REF(vs_params));
    if (sprite_count > 0)
        sg_draw(0, 6, sprite_count);
    sg_end_pass();
    sg_commit();
}

void cleanup(void) {
    game_destroy();
    sg_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
    return sapp_desc{
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .width = 800,
        .height = 600,
        .sample_count = SAMPLE_COUNT,
        .window_title = "dod playground",
    };
}

#if defined(SOKOL_METAL)
const char* vs_src =
    "#include <metal_stdlib>\n"
    "using namespace metal;\n"
    "struct params_t {\n"
    "  float aspect;\n"
    "};\n"
    "struct vs_in {\n"
    "  float3 posScale [[attribute(0)]];\n"
    "  float4 colorIndex [[attribute(1)]];\n"
    "};\n"
    "struct v2f {\n"
    "  float3 color;\n"
    "  float2 uv;\n"
    "  float4 pos [[position]];\n"
    "};\n"
    "vertex v2f _main(vs_in in [[stage_in]], ushort vid [[vertex_id]], constant params_t& params [[buffer(0)]]) {\n"
    "  v2f out;\n"
    "  float x = vid / 2;\n"
    "  float y = vid & 1;\n"
    "  out.pos.x = in.posScale.x + (x-0.5f) * in.posScale.z;\n"
    "  out.pos.y = in.posScale.y + (y-0.5f) * in.posScale.z * params.aspect;\n"
    "  out.pos.z = 0.0f;\n"
    "  out.pos.w = 1.0f;\n"
    "  out.uv = float2((x + in.colorIndex.w)/8,1-y);\n"
    "  out.color = in.colorIndex.rgb;\n"
    "  return out;\n"
    "}\n";
const char* fs_src =
    "#include <metal_stdlib>\n"
    "using namespace metal;\n"
    "struct v2f {\n"
    "  float3 color;\n"
    "  float2 uv;\n"
    "  float4 pos [[position]];\n"
    "};\n"
    "fragment float4 _main(v2f in [[stage_in]], texture2d<float> tex0 [[texture(0)]], sampler smp0 [[sampler(0)]]) {\n"
    "  float4 diffuse = tex0.sample(smp0, in.uv);"
    "  float lum = dot(diffuse.rgb, float3(0.333));\n"
    "  diffuse.rgb = mix(diffuse.rgb, float3(lum), 0.8);\n"
    "  diffuse.rgb *= in.color.rgb;\n"
    "  return diffuse;\n"
    "}\n";
#elif defined(SOKOL_D3D11)
const char* vs_src =
    "cbuffer params : register(b0) {\n"
    "  float aspect;\n"
    "};\n"
    "struct vs_in {\n"
    "  float4 posScale : POSSCALE;\n"
    "  float4 colorIndex : COLORSPRITE;\n"
    "  uint vid : SV_VertexID;\n"
    "};\n"
    "struct v2f {\n"
    "  float3 color : COLOR0;\n"
    "  float2 uv : TEXCOORD0;\n"
    "  float4 pos : SV_Position;\n"
    "};\n"
    "v2f main(vs_in inp) {\n"
    "  v2f outp;\n"
    "  float x = inp.vid / 2;\n"
    "  float y = inp.vid & 1;\n"
    "  outp.pos.x = inp.posScale.x + (x-0.5f) * inp.posScale.z;\n"
    "  outp.pos.y = inp.posScale.y + (y-0.5f) * inp.posScale.z * aspect;\n"
    "  outp.pos.z = 0.0f;\n"
    "  outp.pos.w = 1.0f;\n"
    "  outp.uv = float2((x + inp.colorIndex.w)/8,1-y);\n"
    "  outp.color = inp.colorIndex.rgb;\n"
    "  return outp;\n"
    "};\n";
const char* fs_src =
    "struct v2f {\n"
    "  float3 color: COLOR0;\n"
    "  float2 uv: TEXCOORD0;\n"
    "  float4 pos: SV_Position;\n"
    "};\n"
    "Texture2D tex0 : register(t0);\n"
    "SamplerState smp0 : register(s0);\n"
    "float4 main(v2f inp) : SV_Target0 {\n"
    "  float4 diffuse = tex0.Sample(smp0, inp.uv);"
    "  float lum = dot(diffuse.rgb, 0.333);\n"
    "  diffuse.rgb = lerp(diffuse.rgb, lum.xxx, 0.8);\n"
    "  diffuse.rgb *= inp.color.rgb;\n"
    "  return diffuse;\n"
    "}\n";
#elif defined(SOKOL_GLCORE33)
const char* vs_src =
"#version 330\n"
"//uniform params {\n"
"//  float aspect;\n"
"//};\n"
"uniform vec4 vs_params[1];\n"
"layout(location = 0) in vec4 posScale;\n"
"layout(location = 1) in vec4 colorIndex;\n"
"out vec2 uv;\n"
"out vec3 color;\n"
"void main() {\n"
"  float x = gl_VertexID / 2;\n"
"  float y = gl_VertexID & 1;\n"
"  gl_Position.x = posScale.x + (x-0.5f) * posScale.z;\n"
"  gl_Position.y = posScale.y + (y-0.5f) * posScale.z * vs_params[0].x;\n"
"  gl_Position.z = 0.0f;\n"
"  gl_Position.w = 1.0f;\n"
"  uv = vec2((x + colorIndex.w)/8,1-y);\n"
"  uv = vec2(x,1-y);\n"
"  color = colorIndex.rgb;\n"
"};\n";
const char* fs_src =
"#version 330\n"
"layout(location = 0) out vec4 frag_color;\n"
"in vec2 uv;\n"
"in vec3 color;\n"
"uniform sampler2D tex0;\n"
"void main() {\n"
"  frag_color = texture(tex0, uv);\n"
"  float lum = dot(frag_color.rgb, vec3(0.333));\n"
"  frag_color.rgb = mix(frag_color.rgb, vec3(lum), 0.8);\n"
"  frag_color.rgb *= color.rgb;\n"
"}\n";

#else
#error Unknown graphics plaform
#endif
