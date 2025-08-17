#ifndef __MODEL_H
#define __MODEL_H
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "glad/glad.h"
#include "mesh.h"

class Model
{
public:
  std::vector<Texture> textures_loaded;
  std::vector<Mesh> meshes;
  std::string directory;
  bool gammaCorection;

  glm::vec3 model_center;   // 整个模型的中心（用于顶点偏移）
  float model_scale_factor; // 模型统一缩放因子

private:
  // 坐标轴相关成员变量
  std::vector<Mesh> modelAxisMeshes; // 模型坐标轴网格
  std::vector<Mesh> worldAxisMeshes; // 世界坐标轴网格
  bool showModelAxis;                // 是否显示模型坐标轴
  bool showWorldAxis;                // 是否显示世界坐标轴
  float modelAxisLength;             // 模型坐标轴长度
  float worldAxisLength;             // 世界坐标轴长度

public:
  Model(const char *path, bool gamma = false, bool createModelAxis = true, bool createWorldAxis = false);
  ~Model() = default;
  void draw(Shader *shader);
  void drawWorldAxis(Shader *shader); // 单独绘制世界坐标轴

  // 坐标轴控制方法
  void setModelAxisVisible(bool visible); // 设置模型坐标轴显示/隐藏
  void setWorldAxisVisible(bool visible); // 设置世界坐标轴显示/隐藏
  void toggleModelAxis();                 // 切换模型坐标轴显示状态
  void toggleWorldAxis();                 // 切换世界坐标轴显示状态
  bool isModelAxisVisible() const;        // 获取模型坐标轴显示状态
  bool isWorldAxisVisible() const;        // 获取世界坐标轴显示状态
  float getModelScaleFactor() const;      // 获取模型统一缩放因子

  // 坐标轴创建方法
  void createModelAxis(float length = 1.0f); // 创建模型坐标轴
  void createWorldAxis(float length = 5.0f); // 创建世界坐标轴
  void addModelAxis(float length = 1.0f);    // 添加模型坐标轴（向后兼容）
  void addWorldAxis(float length = 5.0f);    // 添加世界坐标轴（向后兼容）

  unsigned int TextureFromFile(const char *path, const std::string &directory, bool gamma = false);

private:
  void load_model(std::string const path);
  void process_node(aiNode *node, const aiScene *scene);
  Mesh process_mesh(aiMesh *mesh, const aiScene *scene);
  std::vector<Texture> loadMaterialTextures(aiMaterial *mat, aiTextureType type, std::string typeName);
  void calculate_model_bounds(const aiScene *scene); // 新增：计算整个模型边界

  // 坐标轴构建辅助函数
  Mesh createCylinder(glm::vec3 start, glm::vec3 end, float radius, glm::vec3 color, int segments = 12);
  Mesh createCone(glm::vec3 base, glm::vec3 tip, float radius, glm::vec3 color, int segments = 12);
  Mesh createAxis(glm::vec3 direction, float length, float cylinderRadius, float coneRadius, glm::vec3 color);
  Mesh createBidirectionalAxis(glm::vec3 direction, float length, float cylinderRadius, float coneRadius, glm::vec3 color); // 创建双向坐标轴
};

#endif