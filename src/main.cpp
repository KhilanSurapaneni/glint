#include <GLFW/glfw3.h>

#include <cstdio>

int main() {
  if (!glfwInit()) {
    std::fprintf(stderr, "failed to initialize GLFW\n");
    return 1;
  }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  GLFWwindow* window = glfwCreateWindow(1280, 720, "glint", nullptr, nullptr);
  if (!window) {
    std::fprintf(stderr, "failed to create window\n");
    glfwTerminate();
    return 1;
  }

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();
  }

  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
