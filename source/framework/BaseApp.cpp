#include "BaseApp.h"

#include "external/sokol_app.h"
#include "external/sokol_glue.h"
#include "external/sokol_time.h"


BaseApp::BaseApp() {
}

BaseApp::~BaseApp() {
}

void BaseApp::ResetCamera() {

}

bool BaseApp::OnEvent(const sapp_event* ev) {

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
      wx -= mouseSensibility * ev->mouse_dy;
      wy -= mouseSensibility * ev->mouse_dx;
    }
    break;

  case SAPP_EVENTTYPE_KEY_DOWN:
    if (ev->key_code == SAPP_KEYCODE_ESCAPE)
    {
      sapp_request_quit();
    }
    if (ev->key_code == SAPP_KEYCODE_ENTER)
    {
      ResetCamera();
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
      case SAPP_KEYCODE_W: key_forwardKey  = pressed; break;
      case SAPP_KEYCODE_S: key_backwardKey = pressed; break;
      case SAPP_KEYCODE_A: key_leftKey     = pressed; break;
      case SAPP_KEYCODE_D: key_rightKey    = pressed; break;
      }
    }
  }

  return false;
}

void BaseApp::Controls() {
  // Compute directional vectors from euler angles
  float cosX = cosf(wx),
        sinX = sinf(wx),
        cosY = cosf(wy),
        sinY = sinf(wy);
  vec3 dx(cosY, 0, sinY);
  vec3 dy(-sinX * sinY, cosX, sinX * cosY);
  vec3 dz(-cosX * sinY, -sinX, cosX * cosY);

  vec3 dir(0, 0, 0);
  if (key_leftKey)     dir -= dx;
  if (key_rightKey)    dir += dx;
  if (key_downKey)     dir -= dy;
  if (key_upKey)       dir += dy;
  if (key_backwardKey) dir -= dz;
  if (key_forwardKey)  dir += dz;

  float lenSq = dot(dir, dir);
  if (lenSq > 0) {
    dir *= 1.0f / sqrtf(lenSq);
    float speed = 1000.0f; // DT_TODO:
    camPos += dir * (frame_time * speed);
  }
}


bool BaseApp::Load() {
  return true;
}

static void init_userdata_cb(void* in_app) {
  BaseApp* app = (BaseApp*)in_app;

  //printf("Startup time %f\n", stm_ms(stm_diff(stm_now(), app->start_ticks)));

  sg_setup(sg_desc{ .context = sapp_sgcontext() });
#ifdef SOKOL_GL
  sgl_setup(sgl_desc_t{});
#endif // SOKOL_GL
  //DT_TODO: Load UI assets
  app->Load();
  app->ResetCamera();
}

static void frame_userdata_cb(void* in_app) {
  BaseApp* app = (BaseApp*)in_app;
  
  // Update delta time
  app->frame_time = (float)stm_sec(stm_laptime(&app->time_ticks));
  app->app_time   = (float)stm_sec(stm_diff(app->time_ticks, app->start_ticks));

  app->Controls();
  app->DrawFrame();

  //DT_TODO: Draw UI

  sg_commit();
}

static void cleanup_userdata_cb(void* in_app) {
  BaseApp* app = (BaseApp*)in_app;
  delete app;

#ifdef SOKOL_GL
  sgl_shutdown();
#endif // SOKOL_GL
  sg_shutdown();
}

static void event_userdata_cb(const sapp_event* ev, void* in_app){
  BaseApp* app = (BaseApp*)in_app;
  app->OnEvent(ev);
}

sapp_desc sokol_main(int argc, char* argv[]) {

  // Create App
  BaseApp* app = BaseApp::CreateApp();
  stm_setup();
  app->start_ticks = stm_now(); // DT_TODO: Move this to start and report startup time?

  return sapp_desc{
      .user_data = app,
      .init_userdata_cb = init_userdata_cb,
      .frame_userdata_cb = frame_userdata_cb,
      .cleanup_userdata_cb = cleanup_userdata_cb,
      .event_userdata_cb = event_userdata_cb,
      .width = 800,  // DT_TODO: Get params from the app
      .height = 600,
      .sample_count = 4,
      .window_title = "Portals",
  };
}

// DT_TODO: Add mem tracking
// DT_TODO: Add FPS graph
// DT_TODO: Add ini file settings

