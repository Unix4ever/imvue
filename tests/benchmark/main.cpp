#include <benchmark/benchmark.h>
#include "imvue.h"
#include "imgui.h"
#include <stdio.h>
#include <stdlib.h>
#include <sstream>
#include "imvue_generated.h"

#define WINDOW_COUNT 1000

class ImVueBenchmark : public benchmark::Fixture {
public:
  void SetUp(const ::benchmark::State& state) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    // Build atlas
    unsigned char* tex_pixels = NULL;
    int tex_w, tex_h;
    ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&tex_pixels, &tex_w, &tex_h);
  }

  void TearDown(const ::benchmark::State& state) {
    ImGui::DestroyContext();
  }
};

inline void beforeRender() {
  ImGuiIO& io = ImGui::GetIO();
  io.DisplaySize = ImVec2(320, 240);
  io.DeltaTime = 1.0f / 60.0f;
  ImGui::NewFrame();
}

inline void afterRender() {
  ImGui::Render();
}

/**
 * Render test view using 'Vanilla' ImGui
 */
inline void renderImGuiStaticView() {
  beforeRender();
  char windowName[128] = {0};
  for(int i = 0; i < WINDOW_COUNT; ++i) {
    snprintf(windowName, 127, "window%d", i);

    if(ImGui::Begin(windowName)) {
      ImGui::Button("test");
      ImGui::End();
    }
  }
  afterRender();
}

BENCHMARK_DEFINE_F(ImVueBenchmark, RenderImGuiStatic)(benchmark::State& state) {
  for (auto _ : state) {
    renderImGuiStaticView();
  }
};

BENCHMARK_DEFINE_F(ImVueBenchmark, RenderImVueStatic)(benchmark::State& state) {
  ImVue::Document document;
  std::stringstream ss;

  ss << "<template>";
  for(size_t i = 0; i < WINDOW_COUNT; ++i) {
    ss << "<window name=\"window" << i << "\"><button>test</button></window>";
  }
  ss << "</template>";

  document.parse(&ss.str()[0]);
  for (auto _ : state) {
    beforeRender();
    document.render();
    afterRender();
  }
};

#if defined(WITH_LUA)

#include "imgui_lua_bindings.h"
#include "lua/script.h"

const char* imguiLuaScript =
"names = {}\n"
"for i = 1,WINDOW_COUNT do\n"
"names[#names+1] = 'window' .. tostring(i)\n"
"end\n"
"return function()\n"
  "for _, name in ipairs(names) do\n"
    "if imgui.Begin(name) then\n"
      "imgui.Button('test')\n"
      "imgui.End()\n"
    "end\n"
  "end\n"
"end\n";

BENCHMARK_DEFINE_F(ImVueBenchmark, RenderImGuiLuaStatic)(benchmark::State& state) {
  lua_State * L = luaL_newstate();
  luaL_openlibs(L);
  LoadImguiBindings(L);
  lua_pushnumber(L, WINDOW_COUNT);
  lua_setglobal(L, "WINDOW_COUNT");

  int iStatus = luaL_dostring(L, imguiLuaScript);
  if(iStatus) {
    throw std::runtime_error(lua_tostring(L, -1));
  }

  int ref = luaL_ref(L, LUA_REGISTRYINDEX);

  for (auto _ : state) {
    beforeRender();
    lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
    iStatus = lua_pcall(L, 0, 0, 0);
    if(iStatus) {
      throw std::runtime_error(lua_tostring(L, -1));
    }
    lua_pop(L, lua_gettop(L));
    afterRender();
  }
  luaL_unref(L, LUA_REGISTRYINDEX, ref);
  lua_close(L);
};

BENCHMARK_REGISTER_F(ImVueBenchmark, RenderImGuiLuaStatic);

BENCHMARK_DEFINE_F(ImVueBenchmark, RenderImVueScripted)(benchmark::State& state) {
  lua_State * L = luaL_newstate();
  luaL_openlibs(L);
  ImVue::registerBindings(L);
  {
    ImVue::Document document(ImVue::createContext(
      ImVue::createElementFactory(),
      new ImVue::LuaScriptState(L)
    ));
    std::stringstream ss;

    ss << "<template>";
    for(size_t i = 0; i < WINDOW_COUNT; ++i) {
      ss << "<window :name=\"self.windowName .. '" << i << "'\"><button>test</button></window>";
    }
    ss << "</template>";

    ss << "<script>"
      "return ImVue.new({"
        "data = function() return { windowName = 'window' } end"
      "})"
    "</script>";

    document.parse(&ss.str()[0]);
    for (auto _ : state) {
      beforeRender();
      document.render();
      afterRender();
    }
  }
  lua_close(L);
}

BENCHMARK_REGISTER_F(ImVueBenchmark, RenderImVueScripted);
#endif

BENCHMARK_REGISTER_F(ImVueBenchmark, RenderImGuiStatic);
BENCHMARK_REGISTER_F(ImVueBenchmark, RenderImVueStatic);
// Run the benchmark
BENCHMARK_MAIN();
