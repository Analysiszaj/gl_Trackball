#define STB_IMAGE_IMPLEMENTATION
#include <stdexcept>
#include <stb/stb_image.h>
#include "model.h"

Model::Model(const char *path, bool gamma, bool createModelAxis, bool createWorldAxis)
    : gammaCorection(gamma), showModelAxis(false), showWorldAxis(false),
      modelAxisLength(1.0f), worldAxisLength(5.0f)
{
  load_model(path);

  // 根据参数决定是否创建坐标轴
  if (createModelAxis)
  {
    this->createModelAxis();
    showModelAxis = true;
  }

  if (createWorldAxis)
  {
    this->createWorldAxis();
    showWorldAxis = true;
  }
}

void Model::draw(Shader *shader)
{
  // 绘制主模型的网格
  for (unsigned int i = 0; i < meshes.size(); i++)
    meshes[i].draw(shader);

  // 绘制模型坐标轴（如果启用）
  if (showModelAxis)
  {
    for (unsigned int i = 0; i < modelAxisMeshes.size(); i++)
      modelAxisMeshes[i].draw(shader);
  }
}

void Model::drawWorldAxis(Shader *shader)
{
  // 绘制世界坐标轴（如果启用）
  if (showWorldAxis)
  {
    for (unsigned int i = 0; i < worldAxisMeshes.size(); i++)
      worldAxisMeshes[i].draw(shader);
  }
}

void Model::load_model(std::string const path)
{
  Assimp::Importer importer;
  const aiScene *scene = importer.ReadFile(path,
                                           aiProcess_Triangulate |
                                               aiProcess_FlipUVs |
                                               aiProcess_CalcTangentSpace |
                                               aiProcess_GenNormals |          // 生成法线
                                               aiProcess_GenSmoothNormals |    // 生成平滑法线
                                               aiProcess_JoinIdenticalVertices // 合并相同顶点
  );

  if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
  {
    throw std::runtime_error(std::string("Failed to load model: ") + importer.GetErrorString());
  }
  directory = path.substr(0, path.find_last_of('/'));

  // 第一步：计算整个模型的边界
  calculate_model_bounds(scene);

  // 第二步：处理所有节点
  process_node(scene->mRootNode, scene);
}

void Model::process_node(aiNode *node, const aiScene *scene)
{
  for (unsigned int i = 0; i < node->mNumMeshes; i++)
  {
    aiMesh *mesh = scene->mMeshes[node->mMeshes[i]];
    meshes.push_back(process_mesh(mesh, scene));
  }

  for (unsigned int i = 0; i < node->mNumChildren; i++)
  {
    process_node(node->mChildren[i], scene);
  }
}

Mesh Model::process_mesh(aiMesh *mesh, const aiScene *scene)
{
  std::vector<Vertex> vertices;
  std::vector<unsigned int> indices;
  std::vector<Texture> textures;

  // 处理顶点，应用中心偏移和统一缩放
  for (unsigned int i = 0; i < mesh->mNumVertices; i++)
  {
    Vertex vertex;
    glm::vec3 vector; // we declare a placeholder vector since assimp uses its own vector class that doesn't directly convert to glm's vec3 class so we transfer the data to this placeholder glm::vec3 first.
    // positions - 应用整个模型的中心偏移和统一缩放
    vector.x = (mesh->mVertices[i].x - model_center.x) * model_scale_factor;
    vector.y = (mesh->mVertices[i].y - model_center.y) * model_scale_factor;
    vector.z = (mesh->mVertices[i].z - model_center.z) * model_scale_factor;
    vertex.Position = vector;
    // normals
    if (mesh->HasNormals())
    {
      vector.x = mesh->mNormals[i].x;
      vector.y = mesh->mNormals[i].y;
      vector.z = mesh->mNormals[i].z;
      vertex.Normal = vector;
    }
    // texture coordinates
    if (mesh->mTextureCoords[0]) // does the mesh contain texture coordinates?
    {
      glm::vec2 vec;
      // a vertex can contain up to 8 different texture coordinates. We thus make the assumption that we won't
      // use models where a vertex can have multiple texture coordinates so we always take the first set (0).
      vec.x = mesh->mTextureCoords[0][i].x;
      vec.y = mesh->mTextureCoords[0][i].y;
      vertex.TexCoords = vec;

      // 添加空指针检查
      if (mesh->mTangents)
      {
        vector.x = mesh->mTangents[i].x;
        vector.y = mesh->mTangents[i].y;
        vector.z = mesh->mTangents[i].z;
        vertex.Tangent = vector;
      }
      else
      {
        vertex.Tangent = glm::vec3(1.0f, 0.0f, 0.0f);
      }

      if (mesh->mBitangents)
      {
        vector.x = mesh->mBitangents[i].x;
        vector.y = mesh->mBitangents[i].y;
        vector.z = mesh->mBitangents[i].z;
        vertex.Bitangent = vector;
      }
      else
      {
        vertex.Bitangent = glm::vec3(0.0f, 0.0f, 1.0f);
      }
    }
    else
    {
      vertex.TexCoords = glm::vec2(0.5f, 0.5f); // 使用中性纹理坐标，避免与坐标轴标识冲突
      vertex.Tangent = glm::vec3(1.0f, 0.0f, 0.0f);
      vertex.Bitangent = glm::vec3(0.0f, 0.0f, 1.0f);
    }

    vertices.push_back(vertex);
  }

  // 输出调试信息
  std::cout << "网格处理完成，顶点数: " << vertices.size() << std::endl;
  // now wak through each of the mesh's faces (a face is a mesh its triangle) and retrieve the corresponding vertex indices.
  for (unsigned int i = 0; i < mesh->mNumFaces; i++)
  {
    aiFace face = mesh->mFaces[i];
    // retrieve all indices of the face and store them in the indices vector
    for (unsigned int j = 0; j < face.mNumIndices; j++)
      indices.push_back(face.mIndices[j]);
  }
  // process materials
  aiMaterial *material = scene->mMaterials[mesh->mMaterialIndex];
  // we assume a convention for sampler names in the shaders. Each diffuse texture should be named
  // as 'texture_diffuseN' where N is a sequential number ranging from 1 to MAX_SAMPLER_NUMBER.
  // Same applies to other texture as the following list summarizes:
  // diffuse: texture_diffuseN
  // specular: texture_specularN
  // normal: texture_normalN

  // 1. diffuse maps
  std::vector<Texture> diffuseMaps = loadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_diffuse");
  textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());
  // 2. specular maps
  std::vector<Texture> specularMaps = loadMaterialTextures(material, aiTextureType_SPECULAR, "texture_specular");
  textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());
  // 3. normal maps - 修复纹理类型
  std::vector<Texture> normalMaps = loadMaterialTextures(material, aiTextureType_NORMALS, "texture_normal");
  textures.insert(textures.end(), normalMaps.begin(), normalMaps.end());
  // 4. height maps
  std::vector<Texture> heightMaps = loadMaterialTextures(material, aiTextureType_AMBIENT, "texture_height");
  textures.insert(textures.end(), heightMaps.begin(), heightMaps.end());

  return Mesh(vertices, indices, textures);
}

std::vector<Texture> Model::loadMaterialTextures(aiMaterial *mat, aiTextureType type, std::string typeName)
{
  std::vector<Texture> textures;
  for (unsigned int i = 0; i < mat->GetTextureCount(type); i++)
  {
    aiString str;
    mat->GetTexture(type, i, &str);
    // check if texture was loaded before and if so, continue to next iteration: skip loading a new texture
    bool skip = false;
    for (unsigned int j = 0; j < textures_loaded.size(); j++)
    {
      if (std::strcmp(textures_loaded[j].path.data(), str.C_Str()) == 0)
      {
        textures.push_back(textures_loaded[j]);
        skip = true; // a texture with the same filepath has already been loaded, continue to next one. (optimization)
        break;
      }
    }
    if (!skip)
    { // if texture hasn't been loaded already, load it
      Texture texture;
      texture.id = TextureFromFile(str.C_Str(), this->directory);
      texture.type = typeName;
      texture.path = str.C_Str();
      textures.push_back(texture);
      textures_loaded.push_back(texture); // store it as texture loaded for entire model, to ensure we won't unnecessary load duplicate textures.
    }
  }
  return textures;
}

unsigned int Model::TextureFromFile(const char *path, const std::string &directory, bool gamma)
{
  std::string filename = std::string(path);
  filename = directory + '/' + filename;

  unsigned int textureID;
  glGenTextures(1, &textureID);

  int width, height, nrComponents;
  stbi_set_flip_vertically_on_load(true);
  unsigned char *data = stbi_load(filename.c_str(), &width, &height, &nrComponents, 0);
  if (data)
  {
    GLenum format;
    if (nrComponents == 1)
      format = GL_RED;
    else if (nrComponents == 3)
      format = GL_RGB;
    else if (nrComponents == 4)
      format = GL_RGBA;

    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);
  }
  else
  {
    std::cout << "Texture failed to load at path: " << path << std::endl;
    stbi_image_free(data);
  }

  return textureID;
}

void Model::calculate_model_bounds(const aiScene *scene)
{
  glm::vec3 scene_min(FLT_MAX);
  glm::vec3 scene_max(-FLT_MAX);

  // 遍历场景中的所有网格，计算整个模型的边界
  for (unsigned int i = 0; i < scene->mNumMeshes; i++)
  {
    aiMesh *mesh = scene->mMeshes[i];
    for (unsigned int j = 0; j < mesh->mNumVertices; j++)
    {
      glm::vec3 pos(mesh->mVertices[j].x, mesh->mVertices[j].y, mesh->mVertices[j].z);
      scene_min = glm::min(scene_min, pos);
      scene_max = glm::max(scene_max, pos);
    }
  }

  // 计算整个模型的中心
  model_center = (scene_min + scene_max) * 0.5f;

  // 计算模型的尺寸和缩放因子
  glm::vec3 modelSize = scene_max - scene_min;
  float maxDimension = glm::max(modelSize.x, glm::max(modelSize.y, modelSize.z));

  // 计算统一缩放因子，让模型最大尺寸标准化为2.0单位
  float targetSize = 2.0f;
  model_scale_factor = targetSize / maxDimension;

  // 模型坐标轴恢复原来的逻辑：根据标准化后的模型大小自适应
  modelAxisLength = targetSize * 1.25f;

  std::cout << "整个模型边界计算:" << std::endl;
  std::cout << "  最小点: (" << scene_min.x << ", " << scene_min.y << ", " << scene_min.z << ")" << std::endl;
  std::cout << "  最大点: (" << scene_max.x << ", " << scene_max.y << ", " << scene_max.z << ")" << std::endl;
  std::cout << "  模型中心: (" << model_center.x << ", " << model_center.y << ", " << model_center.z << ")" << std::endl;
  std::cout << "  模型尺寸: (" << modelSize.x << ", " << modelSize.y << ", " << modelSize.z << ")" << std::endl;
  std::cout << "  最大尺寸: " << maxDimension << std::endl;
  std::cout << "  统一缩放因子: " << model_scale_factor << std::endl;
  std::cout << "  坐标轴长度: " << modelAxisLength << " (根据标准化模型自适应)" << std::endl;
}

void Model::addModelAxis(float length)
{
  createModelAxis(length);
  setModelAxisVisible(true);
}

void Model::addWorldAxis(float length)
{
  createWorldAxis(length);
  setWorldAxisVisible(true);
}

Mesh Model::createAxis(glm::vec3 direction, float length, float cylinderRadius, float coneRadius, glm::vec3 color)
{
  std::vector<Vertex> vertices;
  std::vector<unsigned int> indices;
  std::vector<Texture> textures;

  // 创建圆柱体 (轴身，长度的90%)
  float cylinderLength = length * 0.9f;
  Mesh cylinder = createCylinder(glm::vec3(0.0f), direction * cylinderLength, cylinderRadius, color);

  // 创建圆锥体 (箭头，长度的10%)
  Mesh cone = createCone(direction * cylinderLength, direction * length, coneRadius, color);

  // 合并圆柱和圆锥的顶点和索引
  vertices.insert(vertices.end(), cylinder.vertices.begin(), cylinder.vertices.end());
  vertices.insert(vertices.end(), cone.vertices.begin(), cone.vertices.end());

  // 调整圆锥的索引偏移
  unsigned int cylinderVertexCount = cylinder.vertices.size();
  for (unsigned int index : cylinder.indices)
  {
    indices.push_back(index);
  }
  for (unsigned int index : cone.indices)
  {
    indices.push_back(index + cylinderVertexCount);
  }

  return Mesh(vertices, indices, textures);
}

Mesh Model::createCylinder(glm::vec3 start, glm::vec3 end, float radius, glm::vec3 color, int segments)
{
  std::vector<Vertex> vertices;
  std::vector<unsigned int> indices;
  std::vector<Texture> textures;

  glm::vec3 direction = glm::normalize(end - start);
  float height = glm::length(end - start);

  // 构建局部坐标系
  glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
  if (abs(glm::dot(direction, up)) > 0.9f)
  {
    up = glm::vec3(1.0f, 0.0f, 0.0f);
  }
  glm::vec3 right = glm::normalize(glm::cross(up, direction));
  up = glm::normalize(glm::cross(direction, right));

  // 创建圆柱体顶点
  for (int i = 0; i <= segments; i++)
  {
    float angle = 2.0f * M_PI * i / segments;
    float x = cos(angle) * radius;
    float y = sin(angle) * radius;

    glm::vec3 offset = right * x + up * y;

    // 底面顶点
    Vertex bottomVertex;
    bottomVertex.Position = start + offset;
    bottomVertex.Normal = glm::normalize(offset);
    // 为坐标轴设置特殊的纹理坐标来标识颜色
    if (color.r == 1.0f && color.g == 0.0f && color.b == 0.0f)
    { // 红色
      bottomVertex.TexCoords = glm::vec2(1.0f, 0.0f);
    }
    else if (color.r == 0.0f && color.g == 1.0f && color.b == 0.0f)
    { // 绿色
      bottomVertex.TexCoords = glm::vec2(0.0f, 1.0f);
    }
    else if (color.r == 0.0f && color.g == 0.0f && color.b == 1.0f)
    { // 蓝色
      bottomVertex.TexCoords = glm::vec2(0.0f, 0.0f);
    }
    else
    {
      bottomVertex.TexCoords = glm::vec2(color.r, color.g);
    }
    bottomVertex.Tangent = glm::vec3(1.0f, 0.0f, 0.0f);
    bottomVertex.Bitangent = glm::vec3(0.0f, 0.0f, 1.0f);
    vertices.push_back(bottomVertex);

    // 顶面顶点
    Vertex topVertex;
    topVertex.Position = end + offset;
    topVertex.Normal = glm::normalize(offset);
    // 为坐标轴设置特殊的纹理坐标来标识颜色
    if (color.r == 1.0f && color.g == 0.0f && color.b == 0.0f)
    { // 红色
      topVertex.TexCoords = glm::vec2(1.0f, 0.0f);
    }
    else if (color.r == 0.0f && color.g == 1.0f && color.b == 0.0f)
    { // 绿色
      topVertex.TexCoords = glm::vec2(0.0f, 1.0f);
    }
    else if (color.r == 0.0f && color.g == 0.0f && color.b == 1.0f)
    { // 蓝色
      topVertex.TexCoords = glm::vec2(0.0f, 0.0f);
    }
    else
    {
      topVertex.TexCoords = glm::vec2(color.r, color.g);
    }
    topVertex.Tangent = glm::vec3(1.0f, 0.0f, 0.0f);
    topVertex.Bitangent = glm::vec3(0.0f, 0.0f, 1.0f);
    vertices.push_back(topVertex);
  }

  // 创建圆柱体索引
  for (int i = 0; i < segments; i++)
  {
    int bottom1 = i * 2;
    int top1 = i * 2 + 1;
    int bottom2 = ((i + 1) % segments) * 2;
    int top2 = ((i + 1) % segments) * 2 + 1;

    // 侧面的两个三角形
    indices.push_back(bottom1);
    indices.push_back(top1);
    indices.push_back(bottom2);

    indices.push_back(top1);
    indices.push_back(top2);
    indices.push_back(bottom2);
  }

  return Mesh(vertices, indices, textures);
}

Mesh Model::createCone(glm::vec3 base, glm::vec3 tip, float radius, glm::vec3 color, int segments)
{
  std::vector<Vertex> vertices;
  std::vector<unsigned int> indices;
  std::vector<Texture> textures;

  glm::vec3 direction = glm::normalize(tip - base);

  // 构建局部坐标系
  glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
  if (abs(glm::dot(direction, up)) > 0.9f)
  {
    up = glm::vec3(1.0f, 0.0f, 0.0f);
  }
  glm::vec3 right = glm::normalize(glm::cross(up, direction));
  up = glm::normalize(glm::cross(direction, right));

  // 顶点 (圆锥尖端)
  Vertex tipVertex;
  tipVertex.Position = tip;
  tipVertex.Normal = direction;
  // 为坐标轴设置特殊的纹理坐标来标识颜色
  if (color.r == 1.0f && color.g == 0.0f && color.b == 0.0f)
  { // 红色
    tipVertex.TexCoords = glm::vec2(1.0f, 0.0f);
  }
  else if (color.r == 0.0f && color.g == 1.0f && color.b == 0.0f)
  { // 绿色
    tipVertex.TexCoords = glm::vec2(0.0f, 1.0f);
  }
  else if (color.r == 0.0f && color.g == 0.0f && color.b == 1.0f)
  { // 蓝色
    tipVertex.TexCoords = glm::vec2(0.0f, 0.0f);
  }
  else
  {
    tipVertex.TexCoords = glm::vec2(color.r, color.g);
  }
  tipVertex.Tangent = glm::vec3(1.0f, 0.0f, 0.0f);
  tipVertex.Bitangent = glm::vec3(0.0f, 0.0f, 1.0f);
  vertices.push_back(tipVertex);

  // 底面圆周顶点
  for (int i = 0; i < segments; i++)
  {
    float angle = 2.0f * M_PI * i / segments;
    float x = cos(angle) * radius;
    float y = sin(angle) * radius;

    glm::vec3 offset = right * x + up * y;
    glm::vec3 sideNormal = glm::normalize(direction + glm::normalize(offset) * 0.5f);

    Vertex baseVertex;
    baseVertex.Position = base + offset;
    baseVertex.Normal = sideNormal;
    // 为坐标轴设置特殊的纹理坐标来标识颜色
    if (color.r == 1.0f && color.g == 0.0f && color.b == 0.0f)
    { // 红色
      baseVertex.TexCoords = glm::vec2(1.0f, 0.0f);
    }
    else if (color.r == 0.0f && color.g == 1.0f && color.b == 0.0f)
    { // 绿色
      baseVertex.TexCoords = glm::vec2(0.0f, 1.0f);
    }
    else if (color.r == 0.0f && color.g == 0.0f && color.b == 1.0f)
    { // 蓝色
      baseVertex.TexCoords = glm::vec2(0.0f, 0.0f);
    }
    else
    {
      baseVertex.TexCoords = glm::vec2(color.r, color.g);
    }
    baseVertex.Tangent = glm::vec3(1.0f, 0.0f, 0.0f);
    baseVertex.Bitangent = glm::vec3(0.0f, 0.0f, 1.0f);
    vertices.push_back(baseVertex);
  }

  // 创建圆锥索引
  for (int i = 0; i < segments; i++)
  {
    int base1 = i + 1;
    int base2 = (i + 1) % segments + 1;

    // 侧面三角形
    indices.push_back(0);     // 顶点
    indices.push_back(base1); // 底面顶点1
    indices.push_back(base2); // 底面顶点2
  }

  return Mesh(vertices, indices, textures);
}

// 坐标轴控制方法实现
void Model::setModelAxisVisible(bool visible)
{
  showModelAxis = visible;
  if (visible && modelAxisMeshes.empty())
  {
    createModelAxis(modelAxisLength);
  }
}

void Model::setWorldAxisVisible(bool visible)
{
  showWorldAxis = visible;
  if (visible && worldAxisMeshes.empty())
  {
    createWorldAxis(worldAxisLength);
  }
}

void Model::toggleModelAxis()
{
  setModelAxisVisible(!showModelAxis);
}

void Model::toggleWorldAxis()
{
  setWorldAxisVisible(!showWorldAxis);
}

bool Model::isModelAxisVisible() const
{
  return showModelAxis;
}

bool Model::isWorldAxisVisible() const
{
  return showWorldAxis;
}

float Model::getModelScaleFactor() const
{
  return model_scale_factor;
}

void Model::createModelAxis(float length)
{
  // 使用计算出的模型轴长度，但缩小到合适的比例
  float axisLength = modelAxisLength * 0.3f; // 模型轴是计算长度的0.3倍，更合适的大小
  modelAxisMeshes.clear();                   // 清除现有的坐标轴网格

  float cylinderRadius = 0.01f; // 固定宽度，调小一些
  float coneRadius = 0.03f;     // 固定宽度，圆锥也调小一些

  // 创建十字形坐标轴（正负两个方向，只有正方向有箭头）
  // X轴 - 红色 (正负方向)
  modelAxisMeshes.push_back(createBidirectionalAxis(glm::vec3(1.0f, 0.0f, 0.0f), axisLength, cylinderRadius, coneRadius, glm::vec3(1.0f, 0.0f, 0.0f)));

  // Y轴 - 绿色 (正负方向)
  modelAxisMeshes.push_back(createBidirectionalAxis(glm::vec3(0.0f, 1.0f, 0.0f), axisLength, cylinderRadius, coneRadius, glm::vec3(0.0f, 1.0f, 0.0f)));

  // Z轴 - 蓝色 (正负方向)
  modelAxisMeshes.push_back(createBidirectionalAxis(glm::vec3(0.0f, 0.0f, 1.0f), axisLength, cylinderRadius, coneRadius, glm::vec3(0.0f, 0.0f, 1.0f)));

  std::cout << "已创建模型坐标轴（十字形），长度: " << axisLength << std::endl;
}

void Model::createWorldAxis(float length)
{
  worldAxisLength = length;
  worldAxisMeshes.clear(); // 清除现有的坐标轴网格

  float cylinderRadius = 0.01f; // 固定宽度，调小到合适大小
  float coneRadius = 0.03f;     // 固定宽度，调小到合适大小
  float axisLength = 2.5f;      // 世界坐标轴使用固定长度1.0单位

  // 世界坐标轴不受模型中心偏移影响，从原点开始
  // X轴 - 红色 (正负方向)
  worldAxisMeshes.push_back(createBidirectionalAxis(glm::vec3(1.0f, 0.0f, 0.0f), axisLength, cylinderRadius, coneRadius, glm::vec3(1.0f, 0.0f, 0.0f)));

  // Y轴 - 绿色 (正负方向)
  worldAxisMeshes.push_back(createBidirectionalAxis(glm::vec3(0.0f, 1.0f, 0.0f), axisLength, cylinderRadius, coneRadius, glm::vec3(0.0f, 1.0f, 0.0f)));

  // Z轴 - 蓝色 (正负方向)
  worldAxisMeshes.push_back(createBidirectionalAxis(glm::vec3(0.0f, 0.0f, 1.0f), axisLength, cylinderRadius, coneRadius, glm::vec3(0.0f, 0.0f, 1.0f)));

  std::cout << "已创建世界坐标轴（十字形），长度: " << axisLength << std::endl;
}

// 创建双向坐标轴（十字形，只有正方向有箭头）
Mesh Model::createBidirectionalAxis(glm::vec3 direction, float length, float cylinderRadius, float coneRadius, glm::vec3 color)
{
  std::vector<Vertex> vertices;
  std::vector<unsigned int> indices;
  std::vector<Texture> textures;

  // 创建中心圆柱体（从负方向到正方向箭头底部）
  float cylinderLength = length * 0.9f;               // 圆柱占90%长度，留10%给箭头
  glm::vec3 cylinderStart = -direction * length;      // 从负方向开始
  glm::vec3 cylinderEnd = direction * cylinderLength; // 到正方向箭头底部

  Mesh cylinder = createCylinder(cylinderStart, cylinderEnd, cylinderRadius, color);

  // 创建正方向箭头（只有正方向有箭头）
  glm::vec3 positiveConeBase = direction * cylinderLength;
  glm::vec3 positiveConeTip = direction * length;
  Mesh positiveCone = createCone(positiveConeBase, positiveConeTip, coneRadius, color);

  // 合并顶点
  vertices.insert(vertices.end(), cylinder.vertices.begin(), cylinder.vertices.end());
  vertices.insert(vertices.end(), positiveCone.vertices.begin(), positiveCone.vertices.end());

  // 调整索引
  unsigned int cylinderVertexCount = cylinder.vertices.size();

  // 圆柱体索引
  for (unsigned int index : cylinder.indices)
  {
    indices.push_back(index);
  }

  // 正方向圆锥索引
  for (unsigned int index : positiveCone.indices)
  {
    indices.push_back(index + cylinderVertexCount);
  }

  return Mesh(vertices, indices, textures);
}