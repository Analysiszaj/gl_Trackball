#ifndef __CORE_H
#define __CORE_H
#include "model.h"
#include "camera.h"
#include <list>
#include <functional>

#include <memory>
class Core
{
private:
  std::unique_ptr<Model> model_;
  std::unique_ptr<Shader> shader_;
  std::list<std::function<void()>> operation_list_;

  Camera camera = Camera(glm::vec3(0.0f, 0.0f, 3.0f));
  float deltaTime = 0.0f;
  float lastFrame = 0.0f;

  // 轨迹球相关变量
  bool isDragging = false;
  glm::vec3 camTarget = glm::vec3(0.0f, 0.0f, 0.0f); // 观察目标点
  float cameraDistance = 3.0f;                       // 相机到目标的距离
  glm::vec3 lastSpherePoint;                         // 上一次在球面上的点
  glm::vec3 currentSpherePoint;                      // 当前在球面上的点
  float trackballRadius = 0.8f;                      // 虚拟轨迹球半径
  bool reverseTrackball = false;                     // 是否反向控制（物体跟随鼠标）

  // 校准相关变量
  glm::vec3 calibratedCameraPosition;   // 校准后的相机基础位置
  glm::vec3 calibratedFront;            // 校准后的前方向
  glm::vec3 calibratedRight;            // 校准后的右方向
  glm::vec3 calibratedUp;               // 校准后的上方向
  float calibratedModelRotation = 0.0f; // 校准时的模型旋转角度
  bool isCalibrated = false;            // 是否已校准

public:
  Core() = default;
  ~Core() = default;

  void init();
  void imgui_render();
  void render();
  void clean();
  void before_render();
  void render_tool_panel();

  // 轨迹球旋转相关方法
  void handleMouseInput();                            // 处理ImGui鼠标输入
  void onMouseScroll(double xoffset, double yoffset); // 处理GLFW滚轮回调
  void updateCameraFromTrackball();

  // 虚拟轨迹球相关方法
  glm::vec3 mapToSphere(const glm::vec2 &coord, float radius); // 将屏幕坐标映射到球面
  glm::vec2 getTrackballCoord(const glm::vec2 &mousePos);      // 获取标准化的轨迹球坐标

  // 校准相关方法
  void calibrateCamera();   // 校准当前相机角度为基础角度
  void resetToCalibrated(); // 重置到校准后的基础角度
  void resetToDefault();    // 重置到默认角度
};

#endif