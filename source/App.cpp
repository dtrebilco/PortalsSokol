#include "App.h"

#include "framework/Image.h"
#include "framework/external/sokol_app.h" // DT_TODO: Remove

const uint32_t MAX_PFX_PARTICLES = 1200;
const uint32_t MAX_TOTAL_PARTICLES = MAX_PFX_PARTICLES * 5;
const uint32_t PFX_VERTEX_SIZE = (4 * 3 + 4 * 2 + 4 * 4);

extern const char* vs_src2, * fs_src2;
extern const char* vs_src_pfx, * fs_src_pfx;

struct PFXBuffer
{
  vec3 pos;
  vec2 tex;
  vec4 color;
};

struct vs_room_params
{
  mat4 mvp;
  vec4 lightPos;
  vec4 camPos;
};

struct fs_room_params
{
  float invlightRadius;
  float ambient;
  float dummy[2];
};

struct vs_pfx_params
{
  mat4 mvp;
};


BaseApp* BaseApp::CreateApp() {
  return new App();
}

void App::ResetCamera() {
  camPos = vec3(470, 220, 210);
  wx = 0;
  wy = PI / 2;
  wz = 0;
}

bool App::Load() {

  pfxBuffer.reserve(MAX_PFX_PARTICLES * 36 * 4);
  workingBuffer1.reserve(8);
  workingBuffer2.reserve(8);

  {
    std::vector<uint16_t> indices;
    indices.resize(MAX_TOTAL_PARTICLES * 6);
    uint16_t* dest = indices.data();
    for (unsigned int i = 0; i < MAX_TOTAL_PARTICLES; i++) {
      *dest++ = 4 * i;
      *dest++ = 4 * i + 1;
      *dest++ = 4 * i + 3;
      *dest++ = 4 * i + 2;
      *dest++ = 4 * i + 3;
      *dest++ = 4 * i + 1;
    }
    pfx_index = sg_make_buffer(sg_buffer_desc{
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .data = sg_range{.ptr = indices.data(), .size = (indices.size() * sizeof(uint16_t)) } ,
      });
  }
  pfx_vertex = sg_make_buffer(sg_buffer_desc{
      .size = MAX_TOTAL_PARTICLES * PFX_VERTEX_SIZE * 4,
      .usage = SG_USAGE_STREAM
    });

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
    shaderDesc.fs.uniform_blocks[0].size = 1 * 16;
    shaderDesc.fs.uniform_blocks[0].layout = SG_UNIFORMLAYOUT_STD140;
    shaderDesc.fs.uniform_blocks[0].uniforms[0].name = "fs_params";
    shaderDesc.fs.uniform_blocks[0].uniforms[0].type = SG_UNIFORMTYPE_FLOAT4;
    shaderDesc.fs.uniform_blocks[0].uniforms[0].array_count = 1;
    shaderDesc.fs.images[0].name = "Base";
    shaderDesc.fs.images[0].image_type = SG_IMAGETYPE_2D;
    shaderDesc.fs.images[0].sampler_type = SG_SAMPLERTYPE_FLOAT;
    shaderDesc.fs.images[1].name = "Bump";
    shaderDesc.fs.images[1].image_type = SG_IMAGETYPE_2D;
    shaderDesc.fs.images[1].sampler_type = SG_SAMPLERTYPE_FLOAT;

    shader = sg_make_shader(shaderDesc);
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

    pfx_shader = sg_make_shader(shaderDesc);
  }

  base[0] = create_texture("data/Wood.png", loadBuffer, imageDesc);
  base[1] = create_texture("data/laying_rock7.png", loadBuffer, imageDesc);
  base[2] = create_texture("data/victoria.png", loadBuffer, imageDesc);

  bump[0] = create_texture("data/Wood_N.png", loadBuffer, imageDesc);
  bump[1] = create_texture("data/laying_rock7_N.png", loadBuffer, imageDesc);
  bump[2] = create_texture("data/victoria_N.png", loadBuffer, imageDesc);

  sg_image_desc pfx_imageDesc = {
    .min_filter = SG_FILTER_LINEAR_MIPMAP_NEAREST,
    .mag_filter = SG_FILTER_LINEAR,
    .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
    .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
  };
  pfx_particle = create_texture("data/Particle.png", loadBuffer, pfx_imageDesc);

  auto load_model = [](const char* filename, Sector& sector, vec3 offset) {
    load_model_from_file(filename, sector.room);

    //mat4 mat(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
    //mat.translate(offset);
    mat4 mat(
      vec4(1.0, 0.0, 0.0, 0.0),
      vec4(0.0, 1.0, 0.0, 0.0),
      vec4(0.0, 0.0, 1.0, 0.0),
      vec4(offset, 1.0));

    transform_model(sector.room, mat);

    // Calculate min/max bounds
    get_bounding_box(sector.room, sector.min, sector.max);
    make_model_renderable(sector.room);
  };

  load_model("data/room0.hmdl", sectors[0], vec3(0, 256, 0));
  load_model("data/room1.hmdl", sectors[1], vec3(-384, 256, 3072));
  load_model("data/room2.hmdl", sectors[2], vec3(1536, 256, 2688));
  load_model("data/room3.hmdl", sectors[3], vec3(-1024, -768, 2688));
  load_model("data/room4.hmdl", sectors[4], vec3(-2304, 256, 2688));

  // Setup portals
  sectors[0].portals.push_back(Portal(1, vec3(-384, 384, 1024), vec3(-128, 384, 1024), vec3(-384, 0, 1024)));
  sectors[1].portals.push_back(Portal(0, vec3(-384, 384, 1024), vec3(-128, 384, 1024), vec3(-384, 0, 1024)));

  sectors[1].portals.push_back(Portal(2, vec3(512, 384, 2816), vec3(512, 384, 3072), vec3(512, 0, 2816)));
  sectors[2].portals.push_back(Portal(1, vec3(512, 384, 2816), vec3(512, 384, 3072), vec3(512, 0, 2816)));

  sectors[2].portals.push_back(Portal(3, vec3(512, -256, 2304), vec3(512, -256, 2560), vec3(512, -640, 2304)));
  sectors[3].portals.push_back(Portal(2, vec3(512, -256, 2304), vec3(512, -256, 2560), vec3(512, -640, 2304)));

  sectors[1].portals.push_back(Portal(4, vec3(-1280, 384, 1664), vec3(-1280, 384, 1920), vec3(-1280, 128, 1664)));
  sectors[4].portals.push_back(Portal(1, vec3(-1280, 384, 1664), vec3(-1280, 384, 1920), vec3(-1280, 128, 1664)));

  sectors[1].portals.push_back(Portal(4, vec3(-1280, 192, 3840), vec3(-1280, 192, 4096), vec3(-1280, -256, 3840)));
  sectors[4].portals.push_back(Portal(1, vec3(-1280, 192, 3840), vec3(-1280, 192, 4096), vec3(-1280, -256, 3840)));

  // Setup lights
  sectors[0].lights.push_back(Light(vec3(0, 128, 0), 800, 100, 100, 100));

  sectors[1].lights.push_back(Light(vec3(-256, 224, 1800), 650, 100, 80, 100));
  sectors[1].lights.push_back(Light(vec3(-512, 128, 3100), 900, 100, 100, 300));

  sectors[2].lights.push_back(Light(vec3(1300, 128, 2700), 800, 100, 100, 200));

  sectors[3].lights.push_back(Light(vec3(-100, -700, 2432), 600, 50, 50, 50));
  sectors[3].lights.push_back(Light(vec3(-1450, -700, 2900), 1200, 250, 80, 250));

  sectors[4].lights.push_back(Light(vec3(-2200, 256, 2300), 800, 100, 100, 100));
  sectors[4].lights.push_back(Light(vec3(-2000, 0, 4000), 800, 100, 100, 100));

  {
    sg_pipeline_desc roomPipDesc = {};
    roomPipDesc.layout.attrs[0] = { .offset = 0, .format = SG_VERTEXFORMAT_FLOAT3 }; // position
    roomPipDesc.layout.attrs[1] = { .offset = 12, .format = SG_VERTEXFORMAT_FLOAT2 }; // uv
    roomPipDesc.layout.attrs[2] = { .offset = 20, .format = SG_VERTEXFORMAT_FLOAT3 }; // mat0
    roomPipDesc.layout.attrs[3] = { .offset = 32, .format = SG_VERTEXFORMAT_FLOAT3 }; // mat1
    roomPipDesc.layout.attrs[4] = { .offset = 44, .format = SG_VERTEXFORMAT_FLOAT3 }; // mat2
    roomPipDesc.shader = shader;
    roomPipDesc.index_type = SG_INDEXTYPE_UINT16;
    roomPipDesc.depth = {
        .compare = SG_COMPAREFUNC_LESS_EQUAL,
        .write_enabled = true,
    };
    roomPipDesc.cull_mode = SG_CULLMODE_BACK;
    //roomPipDesc.face_winding = SG_FACEWINDING_CCW;
    room_pipline = sg_make_pipeline(roomPipDesc);

    roomPipDesc.colors[0].blend = {
        .enabled = true,
        .src_factor_rgb = SG_BLENDFACTOR_ONE,
        .dst_factor_rgb = SG_BLENDFACTOR_ONE,
        .src_factor_alpha = SG_BLENDFACTOR_ONE,
        .dst_factor_alpha = SG_BLENDFACTOR_ONE,
    };
    room_pipline_blend = sg_make_pipeline(roomPipDesc);
  }

  {
    sg_pipeline_desc pipDesc = {};
    pipDesc.layout.attrs[0] = { .offset = 0, .format = SG_VERTEXFORMAT_FLOAT3 }; // position
    pipDesc.layout.attrs[1] = { .offset = 12, .format = SG_VERTEXFORMAT_FLOAT2 }; // uv
    pipDesc.layout.attrs[2] = { .offset = 20, .format = SG_VERTEXFORMAT_FLOAT4 }; // color
    pipDesc.shader = pfx_shader;
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
    //pipDesc.face_winding = SG_FACEWINDING_CCW;
    pfx_pipline = sg_make_pipeline(pipDesc);
  }

  return true;
}

void App::DrawFrame() {

  vs_room_params room_params;
  vs_pfx_params pfx_params;

  const int w = sapp_width(); // DT_TODO: Move to internal state
  const int h = sapp_height();

  //mat4 proj = glm::tweakedInfinitePerspective(1.5, 1.0, 0.2);
  //mat4 proj = glm::perspectiveFovLH_NO(1.5f, float(w), float(h), 0.1f, 6000.0f); // This is the same as perspectiveMatrixX, but the FOV is in height

  mat4 proj = perspectiveMatrixX(1.5f, w, h, 0.1f, 6000);
  mat4 mv = rotateXY(-wx, -wy) * translate(-camPos);

  unsigned int currSector = 0;
  float minDist = 1e10f;

  for (uint32_t i = 0; i < 5; i++) {
    sectors[i].hasBeenDrawn = false;

    // Works for this demo since all sectors have non-intersecting bounding boxes
    // Real large-scale applications would have to implement more sophisticated
    // ways to detect which sector the camera resides in.
    //if (sectors[i]->isInBoundingBox(position)) currSector = i;
    float d = sectors[i].getDistanceSqr(camPos);
    if (d < minDist) {
      currSector = i;
      minDist = d;
    }
  }

  room_params.mvp = proj * mv;
  room_params.camPos = vec4(camPos, 1.0);
  pfx_params.mvp = room_params.mvp;

  vec3 dx(mv[0][0], mv[1][0], mv[2][0]);
  vec3 dy(mv[0][1], mv[1][1], mv[2][1]);

  sg_pass_action pass_action = {};
  pass_action.colors[0] = { .action = SG_ACTION_CLEAR, .value = { 0.1f, 0.1f, 0.1f, 1.0f } };

  sg_begin_default_pass(&pass_action, (int)w, (int)h);

  auto draw_sector = [&](uint32_t draw_index) {

    Sector& sector = sectors[draw_index];
    sector.hasBeenDrawn = true;
    for (int j = 0; j < sector.lights.size(); j++)
    {
      Light& light = sector.lights[j];
      vec3 p = light.CalcLightOffset(app_time - 0.1f, float(j));

      fs_room_params room_params_fs{};
      room_params_fs.invlightRadius = 1.0f / light.radius;
      room_params.lightPos = vec4(light.position + p, 1.0);

      if (j == 0) {
        room_params_fs.ambient = 0.07f;
        sg_apply_pipeline(room_pipline);
      }
      else {
        sg_apply_pipeline(room_pipline_blend);
      }

      sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, SG_RANGE_REF(room_params));
      sg_apply_uniforms(SG_SHADERSTAGE_FS, 0, SG_RANGE_REF(room_params_fs));
      for (int i = 0; i < 3; i++)
      {
        const Batch& batch = sector.room.batches[i];
        sg_bindings binding = {};
        binding.index_buffer = batch.render_index;
        binding.vertex_buffers[0] = batch.render_vertex;
        binding.fs_images[0] = base[i];
        binding.fs_images[1] = bump[i];
        sg_apply_bindings(&binding);
        sg_draw(0, batch.nIndices, 1);
      }
    }
  };
  draw_sector(currSector);

  // Recurse through portals- Determine if the portal bounds are visible
  // Doing simple test if portal area is in camera frustum (original demo used queries with GL_SAMPLES_PASSED)
  for (Portal& portal : sectors[currSector].portals)
  {
    // Cannot do this test if scissoring as there can be multiple portals into the sector - Perhaps disable scissoring if drawing multiple times is very slow?
    //if (!sectors[portal.sector].hasBeenDrawn) 
    {
      vec4 projPt[4];
      for (uint32_t i = 0; i < 4; i++)
      {
        projPt[i] = room_params.mvp * vec4(portal.v[i], 1.0f);
      }

      // Cull in clip space - cull against each six clip planes. Simple fast test- may still be offscreen if passing this test. (can clip corner)
      //  NOTE: Attempting to use Normalized Device Coordinates(NDC) causes issues when the portal intersects the near clip plane 
      //        (w is positive and negative on different points)
      bool cull = false;
      for (uint32_t i = 0; i < 3; i++)
      {
        if (projPt[0][i] < -projPt[0].w &&
            projPt[1][i] < -projPt[1].w &&
            projPt[2][i] < -projPt[2].w &&
            projPt[3][i] < -projPt[3].w)
        {
          cull = true;
          break;
        }
        if (projPt[0][i] > projPt[0].w &&
            projPt[1][i] > projPt[1].w &&
            projPt[2][i] > projPt[2].w &&
            projPt[3][i] > projPt[3].w)
        {
          cull = true;
          break;
        }
      }

      if (!cull)
      {
        workingBuffer1.resize(0);
        workingBuffer2.resize(0);

        workingBuffer1.push_back(projPt[0]);
        workingBuffer1.push_back(projPt[1]);
        workingBuffer1.push_back(projPt[2]);
        workingBuffer1.push_back(projPt[3]);

        uint32_t startX;
        uint32_t startY;
        uint32_t width;
        uint32_t height;

        if (!getPolyScreenArea(workingBuffer1, workingBuffer2, w, h, false, startX, startY, width, height))
        {
          cull = true;
        }
        else
        {
          // Scissor drawing area (minor optimization)
          sg_apply_scissor_rect(startX, startY, width, height, true);

#if 0
//#ifdef SOKOL_GL
          // Debug draw scissor bounds
          sgl_matrix_mode_modelview();
          sgl_load_identity();
          sgl_matrix_mode_projection();
          sgl_load_identity();
          sgl_ortho(0.0f, (float)w, 0.0f, (float)h, -1.0f, 1.0f);

          sgl_scissor_rect(startX, startY, width, height, true);

          sgl_begin_quads();
          sgl_v3f_c3f(0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
          sgl_v3f_c3f((float)w, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
          sgl_v3f_c3f((float)w, (float)h, 0.0f, 0.0f, 1.0f, 0.0f);
          sgl_v3f_c3f(0.0f, (float)h, 0.0f, 0.0f, 1.0f, 0.0f);
          sgl_end();

          sgl_scissor_rect(0, 0, w, h, true);

          // Debug draw portal bounds
          sgl_matrix_mode_modelview();
          sgl_load_matrix(value_ptr(mv));
          sgl_matrix_mode_projection();
          sgl_load_matrix(value_ptr(proj));

          sgl_begin_quads();
          for (uint32_t i = 0; i < 4; i++)
          {
            sgl_v3f_c3f(portal.v[i].x, portal.v[i].y, portal.v[i].z, 1.0f, 0.0f, 0.0f);
          }
          sgl_end();
#endif //SOKOL_GL

        }
      }

      if (!cull)
      {
        draw_sector(portal.sector);
      }
    }
  }

  // Reset scissor from portal geometry drawing
  sg_apply_scissor_rect(0, 0, w, h, true);

  uint32_t particleCount = 0;
  for (Sector& sector : sectors)
  {
    if (sector.hasBeenDrawn)
    {
      for (int j = 0; j < sector.lights.size(); j++)
      {
        Light& light = sector.lights[j];
        vec3 p = light.CalcLightOffset(app_time, float(j));

        ParticleSystem& particles = light.particles;
        particles.setPosition(light.position + p);
        particles.update(app_time);

        uint32_t pfxCount = particles.getParticleCount();
        if (pfxCount > MAX_PFX_PARTICLES)
        {
          pfxCount = MAX_PFX_PARTICLES;
        }
        if ((particleCount + pfxCount) > MAX_TOTAL_PARTICLES)
        {
          pfxCount = MAX_TOTAL_PARTICLES - particleCount;
        }

        // Have an append buffer + render once
        if (pfxCount > 0)
        {
          particles.getVertexArray(pfxBuffer, dx, dy);
          sg_append_buffer(pfx_vertex, sg_range{ .ptr = pfxBuffer.data(), .size = pfxCount * PFX_VERTEX_SIZE * 4});
          particleCount += pfxCount;
        }
      }
    }
    else
    {
       for (unsigned int j = 0; j < sector.lights.size(); j++) {
         sector.lights[j].particles.updateTime(app_time);
       }
    }
  }

  if (particleCount > 0)
  {
    sg_apply_pipeline(pfx_pipline);
    sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, SG_RANGE_REF(pfx_params));
    sg_bindings binding = {};
    binding.index_buffer = pfx_index;
    binding.vertex_buffers[0] = pfx_vertex;
    binding.fs_images[0] = pfx_particle;
    sg_apply_bindings(&binding);
    sg_draw(0, 6 * particleCount, 1);
  }

#ifdef SOKOL_GL
  sgl_draw();
#endif //SOKOL_GL

  sg_end_pass();
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

uniform vec4 fs_params[1];

in vec2 texCoord;
in vec3 lightVec;
in vec3 viewVec;

layout(location = 0) out vec4 frag_color;

void main(){

  float invRadius = fs_params[0].x;
  float ambient = fs_params[0].y;

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
