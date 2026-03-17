cmake_minimum_required(VERSION 3.15)
project(Helium)

set(CMAKE_CXX_STANDARD 17)

find_package(GLEW REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)

add_executable(helium_2d src/helium_2d.cpp)
target_link_libraries(helium_2d PRIVATE GLEW::GLEW glfw glm::glm opengl32)

add_executable(helium_3d src/helium_3d.cpp)
target_link_libraries(helium_3d PRIVATE GLEW::GLEW glfw glm::glm opengl32)