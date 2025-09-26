cmake_minimum_required(VERSION 3.10)
project(MyGLFWApp)

set(CMAKE_CXX_STANDARD 11)

add_executable(main src/main.cpp)

find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
target_link_libraries(main PRIVATE glfw OpenGL::GL)