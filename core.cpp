#include "core.h"
#include "imgui.h"
#include "imgui_stdlib.h"
#include "GLFW/glfw3.h"
#include <cmath>

void Core::init()
{
  shader_ = std::make_unique<Shader>("/Users/mds/my/gl_Trackball/glsl/vertex.glsl",
                                     "/Users/mds/my/gl_Trackball/glsl/fragment.glsl");

  // 初始化轨迹球相关变量
  cameraDistance = glm::length(camera.Position - camTarget);
  updateCameraFromTrackball();
}

void Core::imgui_render()
{
  render_tool_panel();
}

void Core::render()
{
  // 处理鼠标输入
  handleMouseInput();

  float currentFrame = static_cast<float>(glfwGetTime());
  deltaTime = currentFrame - lastFrame;
  lastFrame = currentFrame;

  shader_->use();

  // view/projection transformations
  glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)1280 / (float)800, 0.1f, 100.0f);
  glm::mat4 view = camera.GetViewMatrix();
  shader_->setMat4("projection", projection);
  shader_->setMat4("view", view);

  if (model_)
  {
    // render the loaded model
    glm::mat4 model = glm::mat4(1.0f);

    model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f)); // translate it down so it's at the center of the scene

    // 计算当前旋转角度（基于校准角度）
    float currentRotation = (float)glfwGetTime() * 30.0f;
    float finalRotation = isCalibrated ? (currentRotation - calibratedModelRotation) : currentRotation;
    model = glm::rotate(model, glm::radians(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    // 不需要缩放，模型已经标准化了

    shader_->setMat4("model", model);

    glm::mat3 normalMatrix = glm::mat3(glm::transpose(glm::inverse(model)));
    shader_->setMat3("normalMatrix", normalMatrix);

    shader_->setVec3("objectColor", glm::vec3(1.0f, 0.5f, 0.31f));
    shader_->setVec3("lightColor", glm::vec3(1.0f, 1.0f, 1.0f));
    shader_->setVec3("lightPos", glm::vec3(1.2f, 1.0f, 2.0f));
    shader_->setVec3("viewPos", camera.Position);

    model_->draw(shader_.get());

    // 单独绘制世界坐标轴（使用单位矩阵，不受模型变换影响）
    glm::mat4 worldAxisModel = glm::mat4(1.0f); // 单位矩阵
    shader_->setMat4("model", worldAxisModel);
    glm::mat3 worldAxisNormalMatrix = glm::mat3(1.0f);
    shader_->setMat3("normalMatrix", worldAxisNormalMatrix);
    model_->drawWorldAxis(shader_.get());
  }
}

void Core::clean()
{
  model_.reset();
}

// 渲染前处理事件
void Core::before_render()
{
  while (!operation_list_.empty())
  {
    auto &callback = operation_list_.front();
    callback();
    operation_list_.pop_front();
  }
}

// 渲染调试面板
void Core::render_tool_panel()
{
  ImGui::SetNextWindowBgAlpha(0.0f);
  ImGui::Begin("Debugger", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar);
  ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
              1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
  static std::string model_path = "/Users/mds/my/gl_Trackball/backpack/backpack.obj";
  ImGui::InputText("模型路径", &model_path);
  ImGui::SameLine();
  if (ImGui::Button("加载模型"))
  {
    operation_list_.emplace_back([this]()
                                 {
                                   this->model_.reset();
                              
                                   model_ = std::make_unique<Model>(model_path.c_str(), false, true, false); });
  }

  ImGui::Text("模型状态: %s", model_ ? "已加载" : "未加载");
  if (model_)
  {
    ImGui::Text("网格数量: %zu", model_->meshes.size());
    ImGui::Text("模型缩放: %.4f", model_->getModelScaleFactor());

    // 坐标轴控制
    ImGui::Separator();
    ImGui::Text("坐标轴控制:");

    bool modelAxisVisible = model_->isModelAxisVisible();
    if (ImGui::Checkbox("显示模型坐标轴", &modelAxisVisible))
    {
      model_->setModelAxisVisible(modelAxisVisible);
    }

    bool worldAxisVisible = model_->isWorldAxisVisible();
    if (ImGui::Checkbox("显示世界坐标轴", &worldAxisVisible))
    {
      model_->setWorldAxisVisible(worldAxisVisible);
    }

    if (ImGui::Button("切换模型坐标轴"))
    {
      model_->toggleModelAxis();
    }
    ImGui::SameLine();
    if (ImGui::Button("切换世界坐标轴"))
    {
      model_->toggleWorldAxis();
    }

    ImGui::Separator();
    glm::vec3 pos = camera.Position;
    ImGui::Text("相机位置: (%.2f, %.2f, %.2f)", pos.x, pos.y, pos.z);
    ImGui::Text("相机距离: %.2f", cameraDistance);
    ImGui::Text("是否拖拽: %s", isDragging ? "是" : "否");
    ImGui::Text("球面点: (%.3f, %.3f, %.3f)", lastSpherePoint.x, lastSpherePoint.y, lastSpherePoint.z);
    ImGui::Text("相机Right: (%.3f, %.3f, %.3f)", camera.Right.x, camera.Right.y, camera.Right.z);
    ImGui::Text("相机Up: (%.3f, %.3f, %.3f)", camera.Up.x, camera.Up.y, camera.Up.z);
    ImGui::Text("相机Front: (%.3f, %.3f, %.3f)", camera.Front.x, camera.Front.y, camera.Front.z);
    ImGui::Text("鼠标滚轮: %.2f", ImGui::GetIO().MouseWheel);
    ImGui::Text("ImGui捕获鼠标: %s", ImGui::GetIO().WantCaptureMouse ? "是" : "否");
    ImGui::Text("已校准: %s", isCalibrated ? "是" : "否");
    if (isCalibrated)
    {
      ImGui::Text("校准模型角度: %.2f°", calibratedModelRotation);
    }

    // 相机校准控制
    ImGui::Separator();
    ImGui::Text("相机校准:");

    if (ImGui::Button("校准当前角度"))
    {
      calibrateCamera();
    }
    ImGui::SameLine();
    if (ImGui::Button("重置到校准角度") && isCalibrated)
    {
      resetToCalibrated();
    }
    if (ImGui::Button("重置到默认角度"))
    {
      resetToDefault();
    }

    // 轨迹球控制模式选择
    ImGui::Separator();
    ImGui::Text("轨迹球模式:");

    static bool reverseControl = false;
    if (ImGui::Checkbox("反向控制（物体跟随鼠标）", &reverseControl))
    {
      // 这个标志会在鼠标处理中使用
      this->reverseTrackball = reverseControl;
    }
    ImGui::Text("当前模式: %s", reverseControl ? "物体跟随鼠标" : "相机跟随鼠标");

    // 滚转控制
    ImGui::Separator();
    ImGui::Text("滚转控制:");
    if (ImGui::Button("向左滚转"))
    {
      // 绕前方向轴旋转（滚转）
      glm::mat4 rollRotation = glm::rotate(glm::mat4(1.0f), glm::radians(5.0f), camera.Front);
      glm::vec4 newRightVec4 = rollRotation * glm::vec4(camera.Right, 0.0f);
      glm::vec4 newUpVec4 = rollRotation * glm::vec4(camera.Up, 0.0f);
      camera.Right = glm::normalize(glm::vec3(newRightVec4));
      camera.Up = glm::normalize(glm::vec3(newUpVec4));
    }
    ImGui::SameLine();
    if (ImGui::Button("向右滚转"))
    {
      // 绕前方向轴旋转（滚转）
      glm::mat4 rollRotation = glm::rotate(glm::mat4(1.0f), glm::radians(-5.0f), camera.Front);
      glm::vec4 newRightVec4 = rollRotation * glm::vec4(camera.Right, 0.0f);
      glm::vec4 newUpVec4 = rollRotation * glm::vec4(camera.Up, 0.0f);
      camera.Right = glm::normalize(glm::vec3(newRightVec4));
      camera.Up = glm::normalize(glm::vec3(newUpVec4));
    }

    float position[3] = {pos.x, pos.y, pos.z};
    if (ImGui::SliderFloat3("相机位置", position, -10.0f, 10.0f))
    {
      camera.Position = glm::vec3(position[0], position[1], position[2]);
    }
  }
  ImGui::End();
}

// 使用ImGui直接处理鼠标输入
void Core::handleMouseInput()
{
  ImGuiIO &io = ImGui::GetIO();

  // 如果ImGui正在使用鼠标，不处理轨迹球
  if (io.WantCaptureMouse)
  {
    isDragging = false;
    return;
  }

  glm::vec2 currentMousePos = glm::vec2(io.MousePos.x, io.MousePos.y);

  // 处理鼠标按钮
  if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
  {
    isDragging = true;

    // 计算初始球面点
    glm::vec2 trackballCoord = getTrackballCoord(currentMousePos);
    lastSpherePoint = mapToSphere(trackballCoord, trackballRadius);
  }

  if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
  {
    isDragging = false;
  }

  // 处理鼠标拖拽 - 真正的虚拟轨迹球算法（保留所有旋转自由度）
  if (isDragging && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
  {
    // 获取当前鼠标位置的球面坐标
    glm::vec2 trackballCoord = getTrackballCoord(currentMousePos);
    currentSpherePoint = mapToSphere(trackballCoord, trackballRadius);

    // 计算两个球面点的差异
    float dotProduct = glm::dot(lastSpherePoint, currentSpherePoint);
    dotProduct = glm::clamp(dotProduct, -1.0f, 1.0f);

    if (dotProduct < 0.9999f) // 只有当移动足够大时才旋转
    {
      // 关键：计算屏幕坐标系中的旋转轴
      glm::vec3 screenRotationAxis = glm::cross(lastSpherePoint, currentSpherePoint);

      if (glm::length(screenRotationAxis) > 0.0001f)
      {
        screenRotationAxis = glm::normalize(screenRotationAxis);

        // 如果启用反向控制，翻转旋转轴
        if (reverseTrackball)
        {
          screenRotationAxis = -screenRotationAxis;
        }

        // 计算旋转角度
        float rotationAngle = acos(dotProduct);

        // 关键步骤：将屏幕坐标系的旋转轴转换到相机坐标系
        // 屏幕坐标系: X向右, Y向上, Z向外（朝向观察者）
        // 相机坐标系: Right向右, Up向上, Front向前（背离观察者）
        glm::vec3 cameraRotationAxis = screenRotationAxis.x * camera.Right +
                                       screenRotationAxis.y * camera.Up +
                                       screenRotationAxis.z * (-camera.Front); // Z轴方向相反

        cameraRotationAxis = glm::normalize(cameraRotationAxis);

        // 使用四元数创建旋转（绕相机坐标系中的轴旋转）
        glm::quat deltaRotation = glm::angleAxis(rotationAngle, cameraRotationAxis);

        // 将旋转应用到相机的方向向量
        glm::vec3 currentDir = glm::normalize(camera.Position - camTarget);
        glm::vec3 newDir = deltaRotation * currentDir;

        // 将旋转应用到相机的右向量和上向量（保持所有轴）
        glm::vec3 newRight = deltaRotation * camera.Right;
        glm::vec3 newUp = deltaRotation * camera.Up;

        // 标准化所有向量
        newDir = glm::normalize(newDir);
        newRight = glm::normalize(newRight);
        newUp = glm::normalize(newUp);

        // 更新相机位置和姿态
        camera.Position = camTarget + newDir * cameraDistance;
        camera.Right = newRight;
        camera.Up = newUp;
        camera.Front = glm::normalize(camTarget - camera.Position);

        // 确保正交性（但不重新计算，保持旋转的连续性）
        // 只在必要时微调正交性
        float rightLength = glm::length(camera.Right);
        float upLength = glm::length(camera.Up);
        if (rightLength < 0.9f || rightLength > 1.1f || upLength < 0.9f || upLength > 1.1f)
        {
          camera.Right = glm::normalize(glm::cross(camera.Front, camera.Up));
          camera.Up = glm::normalize(glm::cross(camera.Right, camera.Front));
        }
      }
    }

    // 更新球面点
    lastSpherePoint = currentSpherePoint;
  }
}

// GLFW滚轮回调处理
void Core::onMouseScroll(double xoffset, double yoffset)
{
  // 缩放功能
  cameraDistance -= yoffset * 0.5f;
  cameraDistance = glm::clamp(cameraDistance, 0.5f, 20.0f);

  // 更新相机位置
  glm::vec3 direction = glm::normalize(camera.Position - camTarget);
  camera.Position = camTarget + direction * cameraDistance;

  // 更新相机向量
  updateCameraFromTrackball();
}

void Core::updateCameraFromTrackball()
{
  // 只在初始化或特殊情况下重新计算相机向量
  // 在拖拽过程中，相机向量已经在拖拽处理中直接设置了
  camera.Front = glm::normalize(camTarget - camera.Position);

  // 如果相机向量出现异常（比如初始化时），重新计算
  if (glm::length(camera.Right) < 0.1f || glm::length(camera.Up) < 0.1f)
  {
    camera.Right = glm::normalize(glm::cross(camera.Front, glm::vec3(0.0f, 1.0f, 0.0f)));
    camera.Up = glm::normalize(glm::cross(camera.Right, camera.Front));
  }
}

// 校准当前相机角度为基础角度
void Core::calibrateCamera()
{
  calibratedCameraPosition = camera.Position;
  calibratedFront = camera.Front;
  calibratedRight = camera.Right;
  calibratedUp = camera.Up;

  // 记住当前的模型旋转角度
  calibratedModelRotation = (float)glfwGetTime() * 30.0f;

  isCalibrated = true;
}

// 重置到校准后的基础角度
void Core::resetToCalibrated()
{
  if (isCalibrated)
  {
    camera.Position = calibratedCameraPosition;
    camera.Front = calibratedFront;
    camera.Right = calibratedRight;
    camera.Up = calibratedUp;
    cameraDistance = glm::length(camera.Position - camTarget);
  }
}

// 重置到默认角度
void Core::resetToDefault()
{
  camera.Position = glm::vec3(0.0f, 0.0f, 3.0f);
  camTarget = glm::vec3(0.0f, 0.0f, 0.0f);
  cameraDistance = 3.0f;
  updateCameraFromTrackball();
  isCalibrated = false;
  calibratedModelRotation = 0.0f;
}

glm::vec2 Core::getTrackballCoord(const glm::vec2 &mousePos)
{
  ImGuiIO &io = ImGui::GetIO();
  float windowWidth = io.DisplaySize.x;
  float windowHeight = io.DisplaySize.y;

  // 标准轨迹球坐标转换
  float x = (2.0f * mousePos.x - windowWidth) / windowWidth;
  float y = (windowHeight - 2.0f * mousePos.y) / windowHeight;

  return glm::vec2(x, y);
}

glm::vec3 Core::mapToSphere(const glm::vec2 &coord, float radius)
{
  glm::vec3 spherePoint;
  spherePoint.x = coord.x;
  spherePoint.y = coord.y;

  float d = spherePoint.x * spherePoint.x + spherePoint.y * spherePoint.y;

  if (d < radius * radius * 0.5f)
  {
    // 在球面内部，计算z坐标
    spherePoint.z = sqrt(radius * radius - d);
  }
  else
  {
    // 在球面外部，使用双曲面
    spherePoint.z = (radius * radius * 0.5f) / sqrt(d);
  }

  return glm::normalize(spherePoint);
}
