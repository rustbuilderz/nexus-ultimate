#pragma once
#include <GLFW/glfw3.h>

struct NexusUiColors;

void Nexus_GetThemeUiColors(NexusUiColors* out);

void RenderNexusUltimate(GLFWwindow* window, bool* p_open);
