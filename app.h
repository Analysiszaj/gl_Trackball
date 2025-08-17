#ifndef __APP_H
#define __APP_H

#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "core.h"

constexpr int width = 1280;
constexpr int height = 800;
constexpr const char *title = "TrackBall";

class App
{
private:
  float main_scale_ = 1.0f;
  bool show_demo_window_ = false;
  ImVec4 clear_color_ = ImColor(23, 20, 25).Value;

  GLFWwindow *window_;
  Core *core_;

private:
  App();
  App(const App &) = delete;
  App &operator=(const App &) = delete;

public:
  ~App();
  static App &get_instance()
  {
    static App instance;
    return instance;
  }
  void init();
  void before_render();
  void render();
  void clean();
  void app_run();
  void app_exit();
};
#endif