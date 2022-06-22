#ifndef _APP_H_
#define _APP_H_

#include "framework/BaseApp.h"
#include "framework/ParticleSystem.h"
#include "framework/Model.h"


struct Light {

  Light(vec3 in_position, float in_radius,float in_xs, float in_ys, float in_zs)
  : position(in_position)
  , radius(in_radius)
  , xs(in_xs)
  , ys(in_ys)
  , zs(in_zs)
  {
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
  }

  vec3 CalcLightOffset(float t, float j)
  {
    return vec3(xs * cosf(4.23f * t + j), ys * sinf(2.37f * t) * cosf(1.39f * t), zs * sinf(3.12f * t + j));
  }

  ParticleSystem particles;
  vec3 position = {};
  float radius = 0.0f;
  float xs = 0.0f, ys = 0.0f, zs = 0.0f;
};

struct Portal {
  inline Portal(uint32_t sect, const vec3& vc0, const vec3& vc1, const vec3& vc2) {
    sector = sect;
    v0 = vc0;
    v1 = vc1;
    v2 = vc1 + vc2 - vc0;
    v3 = vc2;
  }

  vec3 v0, v1, v2, v3;
  uint32_t sector = 0;
};

class Sector {
public:

  inline bool isInBoundingBox(vec3& pos) const {
    return (pos.x > min.x && pos.x < max.x&&
      pos.y > min.y && pos.y < max.y&&
      pos.z > min.z && pos.z < max.z);
  }

  inline bool isSphereInSector(const vec3& pos, const float radius) const {
    return (getDistanceSqr(pos) < radius * radius);
  }

  inline float getDistanceSqr(const vec3& pos) const {
    float s, d = 0;
    for (int i = 0; i < 3; i++) {
      if (pos[i] < min[i]) {
        s = pos[i] - min[i];
        d += s * s;
      }
      else if (pos[i] > max[i]) {
        s = pos[i] - max[i];
        d += s * s;
      }
    }
    return d;
  }


  Model room;
  std::vector<Portal> portals;
  std::vector<Light> lights;

  vec3 min, max;
  bool hasBeenDrawn = false;
};

class App : public BaseApp
{
public:

  void ResetCamera() override;
  bool Load() override;
  void DrawFrame() override;

protected:

  Sector sectors[5];

  sg_shader shader = {};
  sg_image base[3] = {};
  sg_image bump[3] = {};
  sg_pipeline room_pipline = {};
  sg_pipeline room_pipline_blend = {};

  sg_shader pfx_shader = {};
  sg_image pfx_particle = {};
  sg_pipeline pfx_pipline = {};

  sg_buffer pfx_index = {};
  sg_buffer pfx_vertex = {};

};


#endif // _APP_H_
