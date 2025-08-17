cmake_minimum_required(VERSION 3.12)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE Debug)

project(gl_trackball)

add_subdirectory(./3rdparty/glad)
add_subdirectory(./3rdparty/imgui)

find_package(glm REQUIRED)
find_package(assimp REQUIRED)

add_executable(${PROJECT_NAME} main.cpp app.cpp model.cpp core.cpp shader.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE imgui glad glm::glm assimp::assimp)
target_include_directories(${PROJECT_NAME} PRIVATE ./3rdparty)
