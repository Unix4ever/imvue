#ifndef __TEST_UTILS__
#define __TEST_UTILS__

#include "imgui.h"

static void renderDocument(ImVue::Document& document, int times = 1)
{
  ImGuiIO& io = ImGui::GetIO();
  unsigned char* pixels;
  int width, height;
  io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

  for(int i = 0; i < times; ++i) {
    io.DisplaySize = ImVec2(1024, 768);
    io.DeltaTime = 1.0f / 60.0f;
    ImGui::NewFrame();
    try {
      document.render();
    } catch (...) {
      ImGui::Render();
      throw;
    }
    ImGui::Render();
  }
}

enum Modifiers {
  Ctrl  = 1 << 0,
  Alt   = 1 << 1,
  Shift = 1 << 2,
  Super = 1 << 3
};

static void simulateMouseClick(const ImVec2& pos, ImVue::Document& document, int button = 0, int modifiers = 0)
{
  ImGuiIO& io = ImGui::GetIO();
  io.MousePos = pos;
  if(modifiers & Modifiers::Ctrl) {
    io.KeyCtrl = true;
  }
  if(modifiers & Modifiers::Alt) {
    io.KeyAlt = true;
  }
  if(modifiers & Modifiers::Shift) {
    io.KeyShift = true;
  }
  if(modifiers & Modifiers::Super) {
    io.KeySuper = true;
  }

  renderDocument(document);
  io.MouseDown[button] = true;
  renderDocument(document);
  io.MouseDown[button] = false;
  io.KeyCtrl = false;
  io.KeyAlt = false;
  io.KeyShift = false;
  io.KeySuper = false;
  renderDocument(document);
}

#endif
