#include "App.h"

BaseApp* BaseApp::CreateApp() {
  return new App();
}

void App::ResetCamera() {
}

bool App::Load() {
  return true;
}

void App::DrawFrame() {
}

