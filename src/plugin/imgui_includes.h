#pragma once

// Dear ImGui (third_party/imgui, v1.91.5) and its OpenGL2 backend, included with their own warnings
// silenced so our code can build clean at warning level 4. Only the plugin sees these headers.

#pragma warning(push, 0)
#include "imgui.h"
#include "imgui_internal.h"  // ImGui::ClearActiveID, used when X-Plane takes the keyboard away
#include "backends/imgui_impl_opengl2.h"
#pragma warning(pop)
