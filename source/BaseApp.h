#ifndef _BASE_APP_H_
#define _BASE_APP_H_

#include "Vector.h"
#include <vector>
#include "external/sokol_gfx.h"

struct sapp_event;

class BaseApp
{
public:

  ///  To be implemented by derived class
  static BaseApp* CreateApp();

  BaseApp();
  virtual ~BaseApp();

  virtual void ResetCamera();
  virtual bool OnEvent(const sapp_event* ev);
  
  virtual bool Load();
  virtual void DrawFrame() = 0;

  void Controls();
 
  float app_time = 0.0f;
  float frame_time = 0.0f;
  uint64_t start_ticks = 0;
  uint64_t time_ticks = 0;

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

};


#endif // _BASE_APP_H_
