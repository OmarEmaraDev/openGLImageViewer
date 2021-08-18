cmake_minimum_required(VERSION 3.20)
cmake_policy(SET CMP0003 NEW)

project("OpenGL Image Viewer")

add_library(imgui
  extern/imgui/imgui.cpp
  extern/imgui/imgui_demo.cpp
  extern/imgui/imgui_draw.cpp
  extern/imgui/imgui_tables.cpp
  extern/imgui/imgui_widgets.cpp
)

target_include_directories(imgui
  PUBLIC extern/imgui
)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

add_subdirectory(extern/glfw)

add_library(imguiGLFW
  extern/imgui/backends/imgui_impl_glfw.cpp
)

target_include_directories(imguiGLFW
  PUBLIC extern/imgui/backends
)

target_link_libraries(imguiGLFW
  PUBLIC imgui
  PUBLIC glfw
)

set(OpenGL_GL_PREFERENCE LEGACY)

include(FindOpenGL)

set(GLAD_PROFILE "core" CACHE STRING "" FORCE)
set(GLAD_API "gl=3.3" CACHE STRING "" FORCE)
set(GLAD_ALL_EXTENSIONS ON CACHE BOOL "" FORCE)

add_subdirectory(extern/glad)

add_compile_definitions(IMGUI_IMPL_OPENGL_LOADER_GLAD)

add_library(imguiOpenGL
  extern/imgui/backends/imgui_impl_opengl3.cpp
)

target_include_directories(imguiOpenGL
  PUBLIC extern/imgui/backends
)

target_link_libraries(imguiOpenGL
  PUBLIC imgui
  PUBLIC OpenGL::GL
  PUBLIC glad
)

add_executable(openGLImageViewer
  src/main.cpp
)

target_include_directories(openGLImageViewer
  PUBLIC extern/stb
)

target_link_libraries(openGLImageViewer
  PRIVATE imguiGLFW
  PRIVATE imguiOpenGL
)

if (MSVC)
  add_compile_options(openGLImageViewer /W4 /WX)
else()
  target_compile_options(openGLImageViewer
    PRIVATE -Wall
    PRIVATE -Wextra
    PRIVATE -pedantic
    PRIVATE -Werror
  )
endif()
