#include <stdio.h>
#include <stdlib.h>

#include <cassert>
#include <memory>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <glad/glad.h>

#include <GLFW/glfw3.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "stb_image.h"

const char *IMAGE_VIEWER_VERTEX_SHADER = R"(

#version 330 core
in vec2 inTextureCoordinates;
in vec2 position;
out vec2 textureCoordinates;

void main() {
  gl_Position = vec4(position, 0.0, 1.0);
  textureCoordinates = inTextureCoordinates;
}

)";

const char *IMAGE_VIEWER_FRAGMENT_SHADER = R"(

#version 330 core
uniform sampler2D imageTexture;
in vec2 textureCoordinates;
out vec4 fragmentColor;

void main()
{
   fragmentColor = texture(imageTexture, textureCoordinates);
}

)";

static GLuint createImageViewerProgram() {
  GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vertexShader, 1, &IMAGE_VIEWER_VERTEX_SHADER, NULL);
  glCompileShader(vertexShader);

  GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragmentShader, 1, &IMAGE_VIEWER_FRAGMENT_SHADER, NULL);
  glCompileShader(fragmentShader);

  GLuint program = glCreateProgram();

  glAttachShader(program, vertexShader);
  glAttachShader(program, fragmentShader);

  glBindFragDataLocation(program, 0, "fragmentColor");

  glLinkProgram(program);

  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);

  return program;
}

static void bindImageProgram(GLuint imageViewerProgram) {
  glUseProgram(imageViewerProgram);
  GLint imageTextureLocation =
      glGetUniformLocation(imageViewerProgram, "imageTexture");
  glUniform1i(imageTextureLocation, 0);
}

typedef struct ImageBuffer {
  GLuint buffer;
  int width;
  int height;
  int channelsCount;

  int getBufferSize() { return width * height * channelsCount; }
} ImageBuffer;

static ImageBuffer loadImageBuffer(const char *imagePath) {
  ImageBuffer imageBuffer;
  imageBuffer.channelsCount = 4;

  stbi_set_flip_vertically_on_load(true);
  stbi_ldr_to_hdr_gamma(1.0f);

  int channelsCount;
  float *imageData =
      stbi_loadf(imagePath, &imageBuffer.width, &imageBuffer.height,
                 &channelsCount, STBI_rgb_alpha);

  glGenBuffers(1, &imageBuffer.buffer);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, imageBuffer.buffer);
  glBufferData(GL_SHADER_STORAGE_BUFFER,
               imageBuffer.getBufferSize() * sizeof(float), imageData,
               GL_STREAM_COPY);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

  stbi_image_free(imageData);

  return imageBuffer;
}

static void drawImage(GLuint imageViewerProgram, ImageBuffer imageBuffer,
                      int displayWidth, int displayHeight) {
  bindImageProgram(imageViewerProgram);
  GLuint texture;
  glGenTextures(1, &texture);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture);
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, imageBuffer.buffer);
  glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, imageBuffer.width,
                 imageBuffer.height);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, imageBuffer.width, imageBuffer.height,
                  GL_RGBA, GL_FLOAT, 0);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

  GLuint vertexBuffer;
  glGenBuffers(1, &vertexBuffer);
  glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
  glBufferData(GL_ARRAY_BUFFER, 16 * sizeof(float), NULL, GL_STREAM_DRAW);
  float *bufferPointer = (float *)glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);

  float normalizedWidth = (float)imageBuffer.width / displayWidth;
  float normalizedHeight = (float)imageBuffer.height / displayHeight;

  bufferPointer[0] = 0.0f;
  bufferPointer[1] = 0.0f;
  bufferPointer[2] = -normalizedWidth;
  bufferPointer[3] = -normalizedHeight;

  bufferPointer[4] = 1.0f;
  bufferPointer[5] = 0.0f;
  bufferPointer[6] = normalizedWidth;
  bufferPointer[7] = -normalizedHeight;

  bufferPointer[8] = 1.0f;
  bufferPointer[9] = 1.0f;
  bufferPointer[10] = normalizedWidth;
  bufferPointer[11] = normalizedHeight;

  bufferPointer[12] = 0.0f;
  bufferPointer[13] = 1.0f;
  bufferPointer[14] = -normalizedWidth;
  bufferPointer[15] = normalizedHeight;

  glUnmapBuffer(GL_ARRAY_BUFFER);

  GLuint vertexArrayObject;
  glGenVertexArrays(1, &vertexArrayObject);
  glBindVertexArray(vertexArrayObject);

  GLuint textureCoordinatesAttribute =
      glGetAttribLocation(imageViewerProgram, "inTextureCoordinates");
  GLuint positionAttribute =
      glGetAttribLocation(imageViewerProgram, "position");

  glEnableVertexAttribArray(textureCoordinatesAttribute);
  glEnableVertexAttribArray(positionAttribute);

  glVertexAttribPointer(textureCoordinatesAttribute, 2, GL_FLOAT, GL_FALSE,
                        4 * sizeof(float), (const GLvoid *)0);
  glVertexAttribPointer(positionAttribute, 2, GL_FLOAT, GL_FALSE,
                        4 * sizeof(float), (const GLvoid *)(sizeof(float) * 2));

  glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

  glBindBuffer(GL_ARRAY_BUFFER, 0);

  glUseProgram(0);

  glDeleteVertexArrays(1, &vertexArrayObject);
  glDeleteBuffers(1, &vertexBuffer);
  glBindTexture(GL_TEXTURE_2D, 0);
}

int main(int argc, char **argv) {
  assert(argc == 2);

  glfwInit();

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  GLFWwindow *window =
      glfwCreateWindow(1920, 1080, "OpenGL Image Viewer", NULL, NULL);
  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);
  gladLoadGL();

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  ImGui::StyleColorsDark();

  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init("#version 330 core");

  ImageBuffer inputImageBuffer = loadImageBuffer(argv[1]);
  GLuint imageViewerProgram = createImageViewerProgram();

  while (!glfwWindowShouldClose(window)) {
    glfwWaitEvents();

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    int displayWidth, displayHeight;
    glfwGetFramebufferSize(window, &displayWidth, &displayHeight);
    glViewport(0, 0, displayWidth, displayHeight);

    glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    drawImage(imageViewerProgram, inputImageBuffer, displayWidth,
              displayHeight);

    static float value = 0.0f;

    ImGui::Begin("OpenGL Image Viewer");

    ImGui::SliderFloat("Value", &value, 0.0f, 1.0f);

    ImGui::End();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(window);
  }

  glDeleteBuffers(1, &inputImageBuffer.buffer);
  glDeleteProgram(imageViewerProgram);

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwDestroyWindow(window);
  glfwTerminate();

  return 0;
}
