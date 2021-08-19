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

const char *IMAGE_PROCESSOR_COMPUTE_SHADER = R"(

#version 330
#extension GL_ARB_compute_shader: enable
#extension GL_ARB_shader_image_load_store: enable
#extension GL_ARB_shader_image_size: enable
layout (local_size_x = 16, local_size_y = 16) in;
layout (rgba32f) readonly uniform image2D inputImage;
layout (rgba32f) writeonly uniform image2D outputImage;
uniform float value;

void main()
{
  ivec2 size = imageSize(inputImage);
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);
  imageStore(outputImage, texel, imageLoad(inputImage, texel) * value);
}

)";

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

typedef struct ImageTexture {
  GLuint id;
  int width;
  int height;
  int channelsCount;

  int getBufferSize() { return width * height * channelsCount; }
} ImageTexture;

static void verifyShader(GLuint shader) {
  GLint status;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
  if (status == GL_TRUE) {
    return;
  }

  GLchar log[1024];
  GLsizei length = 0;
  glGetShaderInfoLog(shader, sizeof(log), &length, log);
  puts(log);
  exit(1);
}

static GLuint createImageViewerProgram() {
  GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vertexShader, 1, &IMAGE_VIEWER_VERTEX_SHADER, NULL);
  glCompileShader(vertexShader);
  verifyShader(vertexShader);

  GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragmentShader, 1, &IMAGE_VIEWER_FRAGMENT_SHADER, NULL);
  glCompileShader(fragmentShader);
  verifyShader(fragmentShader);

  GLuint program = glCreateProgram();

  glAttachShader(program, vertexShader);
  glAttachShader(program, fragmentShader);

  glBindFragDataLocation(program, 0, "fragmentColor");

  glLinkProgram(program);

  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);

  return program;
}

static GLuint createImageProcessorProgram() {
  GLuint computeShader = glCreateShader(GL_COMPUTE_SHADER);
  glShaderSource(computeShader, 1, &IMAGE_PROCESSOR_COMPUTE_SHADER, NULL);
  glCompileShader(computeShader);
  verifyShader(computeShader);

  GLuint program = glCreateProgram();

  glAttachShader(program, computeShader);

  glLinkProgram(program);

  glDeleteShader(computeShader);

  return program;
}

static ImageTexture loadImageTexture(const char *imagePath) {
  ImageTexture imageTexture;
  imageTexture.channelsCount = 4;

  stbi_set_flip_vertically_on_load(true);
  stbi_ldr_to_hdr_gamma(1.0f);

  int channelsCount;
  float *imageData =
      stbi_loadf(imagePath, &imageTexture.width, &imageTexture.height,
                 &channelsCount, STBI_rgb_alpha);

  glGenTextures(1, &imageTexture.id);
  glBindTexture(GL_TEXTURE_2D, imageTexture.id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, imageTexture.width,
               imageTexture.height, 0, GL_RGBA, GL_FLOAT, imageData);
  glBindTexture(GL_TEXTURE_2D, 0);

  stbi_image_free(imageData);

  return imageTexture;
}

static ImageTexture allocateOutputTexture(ImageTexture &inputImageTexture) {
  ImageTexture imageTexture = inputImageTexture;
  glGenTextures(1, &imageTexture.id);
  glBindTexture(GL_TEXTURE_2D, imageTexture.id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, imageTexture.width,
                 imageTexture.height);
  glBindTexture(GL_TEXTURE_2D, 0);
  return imageTexture;
}

static void setComputeUniforms(GLuint program) {
  static float value = 1.0f;
  ImGui::SliderFloat("Value", &value, 0.0f, 1.0f);
  GLint valueLocation = glGetUniformLocation(program, "value");
  glUniform1f(valueLocation, value);
}

static void computeImage(GLuint program, ImageTexture &inputImageTexture,
                         ImageTexture &outputImageTexture) {
  glUseProgram(program);

  GLuint inputImageUnit = 0;
  glBindImageTexture(inputImageUnit, inputImageTexture.id, 0, GL_FALSE, 0,
                     GL_READ_ONLY, GL_RGBA32F);
  GLint inputImageLocation = glGetUniformLocation(program, "inputImage");
  glUniform1i(inputImageLocation, inputImageUnit);

  GLuint outputImageUnit = 1;
  glBindImageTexture(outputImageUnit, outputImageTexture.id, 0, GL_FALSE, 0,
                     GL_WRITE_ONLY, GL_RGBA32F);
  GLint outputImageLocation = glGetUniformLocation(program, "outputImage");
  glUniform1i(outputImageLocation, outputImageUnit);

  setComputeUniforms(program);

  glDispatchCompute(inputImageTexture.width / 16 + 1,
                    inputImageTexture.height / 16 + 1, 1);
  glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

  glUseProgram(0);
  glBindTexture(GL_TEXTURE_2D, 0);
}

static void drawImage(GLuint program, ImageTexture &imageTexture,
                      int displayWidth, int displayHeight) {
  glUseProgram(program);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, imageTexture.id);
  GLint imageTextureLocation = glGetUniformLocation(program, "imageTexture");
  glUniform1i(imageTextureLocation, 0);

  GLuint vertexBuffer;
  glGenBuffers(1, &vertexBuffer);
  glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
  glBufferData(GL_ARRAY_BUFFER, 16 * sizeof(float), NULL, GL_STREAM_DRAW);
  float *bufferPointer = (float *)glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);

  float normalizedWidth = (float)imageTexture.width / displayWidth;
  float normalizedHeight = (float)imageTexture.height / displayHeight;

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
      glGetAttribLocation(program, "inTextureCoordinates");
  GLuint positionAttribute = glGetAttribLocation(program, "position");

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

  ImageTexture inputImageTexture = loadImageTexture(argv[1]);
  ImageTexture outputImageTexture = allocateOutputTexture(inputImageTexture);
  GLuint imageViewerProgram = createImageViewerProgram();
  GLuint imageProcessorProgram = createImageProcessorProgram();

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

    ImGui::Begin("OpenGL Image Viewer");

    computeImage(imageProcessorProgram, inputImageTexture, outputImageTexture);

    drawImage(imageViewerProgram, outputImageTexture, displayWidth,
              displayHeight);

    ImGui::End();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(window);
  }

  glDeleteBuffers(1, &inputImageTexture.id);
  glDeleteBuffers(1, &outputImageTexture.id);
  glDeleteProgram(imageViewerProgram);
  glDeleteProgram(imageProcessorProgram);

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwDestroyWindow(window);
  glfwTerminate();

  return 0;
}
