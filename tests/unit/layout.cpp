#include "imvue.h"
#include <gtest/gtest.h>
#include "utils.h"
#include "imvue_generated.h"

#if defined(WITH_LUA)

#include "imgui_lua_bindings.h"
#include "lua/script.h"
#include <tuple>

typedef const char* LayoutsTestParam;

class TestLayouts : public ::testing::Test, public testing::WithParamInterface<LayoutsTestParam> {

  public:

    TestLayouts()
      : mDoc(0)
    {
    }

    ~TestLayouts()
    {
      if(mDoc) {
        delete mDoc;
        mDoc = 0;
      }
    }

    void SetUp() override
    {
      L = luaL_newstate();
      luaL_openlibs(L);
      ImVue::registerBindings(L);
    }

    void TearDown() override
    {
      if(mDoc) {
        delete mDoc;
        mDoc = 0;
      }
      lua_close(L);
    }

    ImVue::Document& createDoc(const char* path)
    {
      if(mDoc) {
        delete mDoc;
      }
      ImVue::ElementFactory* factory = ImVue::createElementFactory();
      ImVue::Context* ctx = ImVue::createContext(factory, new ImVue::LuaScriptState(L));
      char* data = ctx->fs->load(path);
      if(!data) {
        throw std::runtime_error("failed to load file");
      }
      try {
        mDoc = new ImVue::Document(ctx);
        mDoc->parse(data);
      } catch(...) {
        delete mDoc;
        mDoc = 0;
        ImGui::MemFree(data);
        throw;
      }
      ImGui::MemFree(data);
      return *mDoc;
    }

    ImVue::Document* mDoc;
    lua_State* L;
};

inline void getLuaVariable(lua_State* L, const char* table, const char* key, bool& result) {
  int top = lua_gettop(L);
  lua_getglobal(L, table);
  lua_pushstring(L, key);
  lua_gettable(L, -2);
  result = lua_toboolean(L, -1);
  lua_pop(L, lua_gettop(L) - top);
}

TEST_P(TestLayouts, TestFlex) {
  const char* file = GetParam();
  ImVue::Document& doc = createDoc(file);
  renderDocument(doc);
  bool success = false;
  if(luaL_dostring(L, "imvue:runTest()") != 0) {
    FAIL() << "Test failed " << lua_tostring(L, -1);
  }
  getLuaVariable(L, "result", "success", success);
  ASSERT_TRUE(success);
}

INSTANTIATE_TEST_CASE_P(
  TestFlex,
  TestLayouts,
  ::testing::Values(
    "flex_vertical.xml",
    "flex_horizontal.xml"
  ));

#endif
