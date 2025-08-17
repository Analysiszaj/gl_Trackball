#include "app.h"

// 全局Core指针，用于滚轮回调函数
static Core *g_core = nullptr;

static void glfw_error_callback(int error, const char *descroption)
{
  std::cout << "GLFW Error " << error << ":" << descroption << "\n"
            << std::endl;
}

// 只处理滚轮事件的回调
static void scroll_callback(GLFWwindow *window, double xoffset, double yoffset)
{
  // 先让ImGui处理滚轮事件
  ImGuiIO &io = ImGui::GetIO();
  if (io.WantCaptureMouse)
  {
    return; // ImGui正在使用鼠标，不处理轨迹球
  }

  if (g_core)
  {
    g_core->onMouseScroll(xoffset, yoffset);
  }
}

App::App()
{
  glfwSetErrorCallback(glfw_error_callback);
  if (!glfwInit())
  {
    exit(EXIT_FAILURE);
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

  main_scale_ = ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor());
  window_ = glfwCreateWindow((int)(width * main_scale_), (int)(height * main_scale_), title, nullptr, nullptr);

  if (!window_)
  {
    std::cout << "Fail create GLFW Window" << std::endl;
    glfwTerminate();
    exit(EXIT_FAILURE);
  }

  glfwMakeContextCurrent(window_);
  glfwSwapInterval(1);

  // 只设置滚轮回调，其他鼠标事件用ImGui处理
  glfwSetScrollCallback(window_, scroll_callback);

  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
  {
    std::cout << "Fail initalize GLAD" << std::endl;
    exit(EXIT_FAILURE);
  }
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE); // 启用背面剔除
  glCullFace(GL_BACK);
  glFrontFace(GL_CCW);
  init();
}

App::~App()
{
}

void App::init()
{
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls

  ImGui::StyleColorsDark();

  ImGuiStyle &style = ImGui::GetStyle();
  style.ScaleAllSizes(main_scale_);
  style.FontScaleDpi = 0.70f;
  style.FrameRounding = 2.0f;

  style.Alpha = 1.0f;

  ImGui_ImplGlfw_InitForOpenGL(window_, true);
  ImGui_ImplOpenGL3_Init("#version 330 core");

  style.FontSizeBase = 16.0f;
  // io.Fonts->AddFontDefault();
  io.Fonts->AddFontFromFileTTF("/Users/mds/my/spatial_plane_simulation/assets/AlimamaFangYuanTiVF-Thin-2.ttf");

  core_ = new Core();
  core_->init();

  // 设置全局Core指针，供滚轮回调函数使用
  g_core = core_;
}

void App::before_render()
{
  core_->before_render();
}

void App::render()
{
  if (show_demo_window_)
    ImGui::ShowDemoWindow();

  core_->imgui_render();
}

void App::clean()
{
  core_->clean();
}

void App::app_run()
{
  while (!glfwWindowShouldClose(window_))
  {
    glfwPollEvents();
    if (glfwGetWindowAttrib(window_, GLFW_ICONIFIED) != 0)
    {
      ImGui_ImplGlfw_Sleep(10);
      continue;
    }
    before_render();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    render();
    ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(window_, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(clear_color_.x * clear_color_.w, clear_color_.y * clear_color_.w, clear_color_.z * clear_color_.w, clear_color_.w);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    core_->render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(window_);
  }
}

void App::app_exit()
{
  clean();
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
  glfwDestroyWindow(window_);
  glfwTerminate();

  exit(EXIT_SUCCESS);
}
