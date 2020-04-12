#include "imvue.h"
#include <gtest/gtest.h>
#include "utils.h"
#include "imvue_generated.h"

#if defined(WITH_LUA)

#include "imgui_lua_bindings.h"
#include "lua/script.h"
#include <tuple>

class StackChecker
{
  public:
    void init(lua_State* s)
    {
      L = s;
      top = lua_gettop(L);
    }

    void check()
    {
      if(lua_gettop(L) != top) {
        EXPECT_EQ(lua_gettop(L), top);
        while(lua_gettop(L) > top && lua_gettop(L) > 0) {
          int lt = lua_type(L, -1);
          int current = lua_gettop(L);
          switch(lt) {
            case LUA_TTABLE:
              std::cout << "#" << current << ", table\n";
              break;
            case LUA_TSTRING:
              std::cout << "#" << current << ", string: " << lua_tostring(L, -1) << "\n";
              break;
            case LUA_TNUMBER:
              std::cout << "#" << current << ", number: " << lua_tonumber(L, -1) << "\n";
              break;
            case LUA_TBOOLEAN:
              std::cout << "#" << current << ", bool: " << lua_toboolean(L, -1) << "\n";
              break;
            case LUA_TUSERDATA:
              std::cout << "#" << current << ", userdata\n";
              break;
            case LUA_TFUNCTION:
              std::cout << "#" << current << ", function\n";
              break;
            default:
              std::cout << "#" << current << ", type\n";
              break;
          }
          lua_pop(L, 1);
        }
      }
    }

  private:
    lua_State* L;
    int top;
};

class LuaScriptStateTest : public ::testing::Test {
  protected:
    void SetUp() override {
      L = luaL_newstate();
      luaL_openlibs(L);
      ImVue::registerBindings(L);
      sc.init(L);
    }

    void TearDown() override {
      sc.check();
      lua_close(L);
    }

    lua_State* L;
    StackChecker sc;
};

void getLuaVariable(lua_State* L, const char* table, const char* key, bool& result) {
  int top = lua_gettop(L);
  lua_getglobal(L, table);
  lua_pushstring(L, key);
  lua_gettable(L, -2);
  result = lua_toboolean(L, -1);
  lua_pop(L, lua_gettop(L) - top);
}

void getLuaVariable(lua_State* L, const char* table, const char* key, int& result) {
  int top = lua_gettop(L);
  lua_getglobal(L, table);
  lua_pushstring(L, key);
  lua_gettable(L, -2);
  result = lua_tointeger(L, -1);
  lua_pop(L, lua_gettop(L) - top);
}

TEST_F(LuaScriptStateTest, TestState)
{
  ImVue::Document document(ImVue::createContext(
        ImVue::createElementFactory(),
        new ImVue::LuaScriptState(L)
        ));

  const char* data = "<template>"
    "<window name=\"static\">"
    "</window>"
    "</template>"
    "<script>"
    "return ImVue.new({"
      "data = function(self)\n"
        "if type(self) ~= 'userdata' then\n"
          "error('self type check failed')\n"
        "end\n"
        "return {}\n"
      "end"
    "})"
    "</script>";

  document.parse(data);
  renderDocument(document);
}

TEST_F(LuaScriptStateTest, TestReactivity)
{
  ImVue::Document document(ImVue::createContext(
        ImVue::createElementFactory(),
        new ImVue::LuaScriptState(L)
        ));

  const char* data = "<template>"
    "<window id=\"main\" :name=\"self.windowName\"><button>{{self.buttonName}}</button><button v-if=\"self.condition\">static</button>"
      "<selectable id=\"pill\" v-for=\"value in self.pills\">{{value}}</selectable>"
      "<text-unformatted>{{self.previous = self.access; self.count = self.count + 1; return self.access}}</text-unformatted>"
      "<text-unformatted id=\"multivar\">{{self.v1}}{{self.v2}}</text-unformatted>"
    "</window>"
    "</template>"
    "<script>"
    "state = ImVue.new({"
      "data = function() return { "
        "windowName = 'window', buttonName = 'button', condition = false,"
        "pills = {'red pill', 'blue pill'},"
        "access = 0,"
        "count = 0,"
        "v1 = 0, v2 = 0,"
      "} end"
    "})"
    "return state"
    "</script>";

  document.parse(data);
  renderDocument(document);

  int count = -1;
  getLuaVariable(L, "state", "count", count);
  ASSERT_EQ(count, 1);

  luaL_dostring(L, "state.access = 100");
  renderDocument(document);
  getLuaVariable(L, "state", "count", count);
  ASSERT_EQ(count, 2);

  ImVector<ImVue::Window*> windows = document.getChildren<ImVue::Window>("#main");
  ASSERT_EQ(windows.size(), 1);

  ImVue::Window* window = windows[0];

  ASSERT_STREQ(window->name, "window");

  ImVector<ImVue::Button*> buttons = window->getChildren<ImVue::Button>("button");
  ASSERT_EQ(buttons.size(), 2);

  ASSERT_STREQ(buttons[0]->label, "button");
  ASSERT_FALSE(buttons[1]->enabled);

  luaL_dostring(L, "state.windowName = 'changed'; state.buttonName = 'ok'; state.condition = true");
  renderDocument(document, 2);

  ASSERT_STREQ(window->name, "changed");
  ASSERT_STREQ(buttons[0]->label, "ok");
  ASSERT_TRUE(buttons[1]->enabled);

  ImVector<ImVue::Selectable*> selectables = window->getChildren<ImVue::Selectable>("#pill");
  ASSERT_EQ(selectables.size(), 2);
  luaL_dostring(L, "state.pills[#state.pills + 1] = 'laxative'");
  renderDocument(document, 2);
  selectables = window->getChildren<ImVue::Selectable>("#pill");
  ASSERT_EQ(selectables.size(), 3);

  // clear list
  luaL_dostring(L, "state.pills = {}");
  renderDocument(document, 2);
  selectables = window->getChildren<ImVue::Selectable>("#pill");
  ASSERT_EQ(selectables.size(), 0);

  luaL_dostring(L, "state.pills[#state.pills + 1] = 'natrium oxybutyricum'");
  renderDocument(document, 2);
  selectables = window->getChildren<ImVue::Selectable>("#pill");
  ASSERT_EQ(selectables.size(), 1);

  // multi value template updates
  luaL_dostring(L, "state.v1 = 1");
  renderDocument(document, 2);
  ImVector<ImVue::TextUnformatted*> multivar = window->getChildren<ImVue::TextUnformatted>("#multivar");
  EXPECT_EQ(multivar.size(), 1);
  ImVue::TextUnformatted* element = multivar[0];

  EXPECT_STREQ(element->text, "10");
  luaL_dostring(L, "state.v2 = 1");
  renderDocument(document, 2);
  EXPECT_STREQ(element->text, "11");
  luaL_dostring(L, "state.v1 = 0");
  renderDocument(document, 2);
  EXPECT_STREQ(element->text, "01");
}

TEST_F(LuaScriptStateTest, TestIfElseIf)
{
  ImVue::Document document(ImVue::createContext(
        ImVue::createElementFactory(),
        new ImVue::LuaScriptState(L)
  ));

  const char* data = "<template>"
    "<window id=\"1\" v-if=\"self.mode == 0\" :name=\"self.windowName\"><text-unformatted>window 1</text-unformatted></window>"
    "<window id=\"2\" v-else-if=\"self.mode == 1\" :name=\"self.windowName\"><text-unformatted>window 2</text-unformatted></window>"
    "<window id=\"3\" v-else=\"\" :name=\"self.windowName\"><text-unformatted>window 3</text-unformatted></window>"
    "</template>"
    "<script>"
    "state = ImVue.new({"
      "data = function() return { mode = 0, windowName = 'window' } end"
    "})"
    "return state"
    "</script>";

  document.parse(data);
  renderDocument(document);
  ImVector<ImVue::Window*> windows = document.getChildren<ImVue::Window>("window");
  ASSERT_EQ(windows.size(), 3);
  ASSERT_TRUE(windows[0]->enabled);
  ASSERT_FALSE(windows[1]->enabled);
  ASSERT_FALSE(windows[2]->enabled);

  luaL_dostring(L, "state.mode = 1");
  renderDocument(document, 3);

  windows = document.getChildren<ImVue::Window>("window");
  EXPECT_FALSE(windows[0]->enabled);
  ASSERT_TRUE(windows[1]->enabled);
  ASSERT_FALSE(windows[2]->enabled);

  luaL_dostring(L, "state.mode = 10");
  renderDocument(document, 3);

  windows = document.getChildren<ImVue::Window>("window");
  ASSERT_FALSE(windows[0]->enabled);
  ASSERT_FALSE(windows[1]->enabled);
  ASSERT_TRUE(windows[2]->enabled);

  luaL_dostring(L, "state.mode = 1");
  renderDocument(document, 3);

  windows = document.getChildren<ImVue::Window>("window");
  ASSERT_FALSE(windows[0]->enabled);
  ASSERT_TRUE(windows[1]->enabled);
  ASSERT_FALSE(windows[2]->enabled);

  luaL_dostring(L, "state.mode = 0");
  renderDocument(document, 3);

  windows = document.getChildren<ImVue::Window>("window");
  ASSERT_TRUE(windows[0]->enabled);
  ASSERT_FALSE(windows[1]->enabled);
  ASSERT_FALSE(windows[2]->enabled);
}

typedef std::tuple<const char*, bool, int, int> MouseHandlerTestParam;

class MouseHandlerTest : public ::testing::Test, public testing::WithParamInterface<MouseHandlerTestParam> {
  protected:
    void SetUp() override {
      L = luaL_newstate();
      luaL_openlibs(L);
      ImVue::registerBindings(L);
      sc.init(L);
    }

    void TearDown() override {
      sc.check();
      lua_close(L);
    }

    lua_State* L;
    StackChecker sc;
};

TEST_P(MouseHandlerTest, TestMouseHandlers) {
  ImVue::Document document(ImVue::createContext(
        ImVue::createElementFactory(),
        new ImVue::LuaScriptState(L)
  ));

  const char* handler;
  bool expectTriggered;
  int button, modifiers;
  std::tie(handler, expectTriggered, button, modifiers) = GetParam();
  std::stringstream ss;
  ss << "<template>"
    "<window id=\"main\" :name=\"self.windowName\">"
      "<button size=\"200, 100\"" << handler << "=\"self.triggered = true\">click me</button>"
    "</window>"
    "</template>"
    "<script>"
    "state = ImVue.new({"
      "data = function() return { windowName = 'window', triggered = false, right = false, middle = false } end"
    "})"
    "return state"
    "</script>";

  document.parse(&ss.str()[0]);

  renderDocument(document);
  ImGuiIO& io = ImGui::GetIO();
  ImGui::NewFrame();
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(io.DisplaySize);
  document.render();
  ImGui::SetWindowFocus("window");
  ImGui::Render();

  bool triggered = false;
  getLuaVariable(L, "state", "triggered", triggered);

  ASSERT_FALSE(triggered);

  simulateMouseClick(ImVec2(60, 60), document, button, modifiers);

  getLuaVariable(L, "state", "triggered", triggered);
  EXPECT_EQ(triggered, expectTriggered);
}

INSTANTIATE_TEST_CASE_P(
  TestMouseHandlers,
  MouseHandlerTest,
  ::testing::Values(
    std::make_tuple("v-on:click", true, 0, 0),
    std::make_tuple("v-on:click.left", true, 0, 0),
    std::make_tuple("v-on:click.right", true, 1, 0),
    std::make_tuple("v-on:click.middle", true, 2, 0),
    std::make_tuple("v-on:click.ctrl", true, 0, Modifiers::Ctrl),
    std::make_tuple("v-on:click.ctrl.exact", true, 0, Modifiers::Ctrl),
    std::make_tuple("v-on:click.right.ctrl.exact", true, 1, Modifiers::Ctrl),
    std::make_tuple("v-on:click.right.ctrl.exact", false, 1, Modifiers::Ctrl | Modifiers::Shift),
    std::make_tuple("v-on:click.right.ctrl", true, 1, Modifiers::Ctrl | Modifiers::Shift),
    std::make_tuple("v-on:click.exact", false, 0, Modifiers::Ctrl | Modifiers::Shift)
 ));

TEST_F(LuaScriptStateTest, TestHandlersInLoop) {
  ImVue::Document document(ImVue::createContext(
        ImVue::createElementFactory(),
        new ImVue::LuaScriptState(L)
  ));

  const char* data = "<template>"
    "<window id=\"main\" :name=\"self.windowName\"><template v-for=\"item in self.items\"><button v-for=\"i in item\" key=\"item\" size=\"200, 200\" v-on:click=\"self.triggered = true\">click me</button></template></window>"
    "</template>"
    "<script>"
    "state = ImVue.new({"
      "data = function() return { windowName = 'window', triggered = false, items = {{'1'}} } end"
    "})"
    "return state"
    "</script>";

  document.parse(data);

  renderDocument(document);
  ImGuiIO& io = ImGui::GetIO();
  ImGui::NewFrame();
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(io.DisplaySize);
  document.render();
  ImGui::SetWindowFocus("window");
  ImGui::Render();

  bool triggered = false;
  getLuaVariable(L, "state", "triggered", triggered);

  ASSERT_FALSE(triggered);

  simulateMouseClick(ImVec2(60, 60), document);

  getLuaVariable(L, "state", "triggered", triggered);
  ASSERT_TRUE(triggered);
}

TEST_F(LuaScriptStateTest, TestLifecycle)
{
  const char* data = "<template>"
    "<window id=\"main\" :name=\"self.windowName\"><button size=\"200, 200\" v-on:click=\"function() self.triggered = true end\">click me</button></window>"
    "</template>"
    "<script>"
    "calls = {}\n"
    "calls.increase = function(self, t)\n self[t] = (self[t] or 0) + 1\n end\n"
    "state = ImVue.new({"
      "data = function() return { windowName = 'window', triggered = false } end,\n"
      "beforeCreate = function() calls:increase('beforeCreate') end,\n"
      "created = function() calls:increase('created') end,\n"
      "beforeMount = function() calls:increase('beforeMount') end,\n"
      "mounted = function() calls:increase('mounted') end,\n"
      "beforeUpdate = function() calls:increase('beforeUpdate') end,\n"
      "updated = function() calls:increase('updated') end,\n"
      "beforeDestroy = function() calls:increase('beforeDestroy') end,\n"
      "destroyed = function() calls:increase('destroyed') end,\n"
    "})"
    "return state"
    "</script>";

  int count = 0;
  {
    ImVue::Document document(ImVue::createContext(
          ImVue::createElementFactory(),
          new ImVue::LuaScriptState(L)
    ));

    document.parse(data);
    getLuaVariable(L, "calls", "beforeCreate", count);
    ASSERT_EQ(count, 1);
    getLuaVariable(L, "calls", "created", count);
    ASSERT_EQ(count, 1);
    getLuaVariable(L, "calls", "beforeMount", count);
    ASSERT_EQ(count, 0);
    getLuaVariable(L, "calls", "mounted", count);
    ASSERT_EQ(count, 0);

    renderDocument(document);

    getLuaVariable(L, "calls", "beforeMount", count);
    ASSERT_EQ(count, 1);
    getLuaVariable(L, "calls", "mounted", count);
    ASSERT_EQ(count, 1);
    getLuaVariable(L, "calls", "beforeUpdate", count);
    renderDocument(document);
    ASSERT_EQ(count, 0);
    getLuaVariable(L, "calls", "updated", count);
    ASSERT_EQ(count, 0);
    luaL_dostring(L, "state.windowName = 'changed'");
    renderDocument(document, 2);
    getLuaVariable(L, "calls", "beforeUpdate", count);
    ASSERT_EQ(count, 1);
    getLuaVariable(L, "calls", "updated", count);
    ASSERT_EQ(count, 1);
  }
  getLuaVariable(L, "calls", "beforeDestroy", count);
  ASSERT_EQ(count, 1);
  getLuaVariable(L, "calls", "destroyed", count);
  ASSERT_EQ(count, 1);
}

TEST_F(LuaScriptStateTest, TestDocumentParse) {
  ImVue::Document document(ImVue::createContext(
        ImVue::createElementFactory(),
        new ImVue::LuaScriptState(L)
  ));

  const char* data = "<template>"
    "<window name=\"static\"></window>"
    "</template>"
    "<script>"
    "return ImVue.new({"
      "data = function() return {} end"
    "})"
    "</script>";

  document.parse(data);

  data = "<template>"
    "<window id=\"main\" :name=\"self.windowName\"><button>{{self.buttonName}}</button><button v-if=\"self.condition\">static</button></window>"
    "</template>"
    "<script>"
    "state = ImVue.new({"
      "data = function() return { windowName = 'window', buttonName = 'button', condition = false } end"
    "})"
    "return state"
    "</script>";

  document.parse(data);

  renderDocument(document);

  ImVector<ImVue::Window*> windows = document.getChildren<ImVue::Window>("#main");
  ASSERT_EQ(windows.size(), 1);

  ImVue::Window* window = windows[0];

  ASSERT_STREQ(window->name, "window");

  ImVector<ImVue::Button*> buttons = window->getChildren<ImVue::Button>("button");
  ASSERT_EQ(buttons.size(), 2);

  ASSERT_STREQ(buttons[0]->label, "button");
  ASSERT_FALSE(buttons[1]->enabled);

  luaL_dostring(L, "state.windowName = 'changed'; state.buttonName = 'ok'; state.condition = true");
  renderDocument(document, 2);

  ASSERT_STREQ(window->name, "changed");
  ASSERT_STREQ(buttons[0]->label, "ok");
  ASSERT_TRUE(buttons[1]->enabled);
}

TEST_F(LuaScriptStateTest, TestReferences) {
  ImVue::LuaScriptState* state = new ImVue::LuaScriptState(L);
  ImVue::Document document(ImVue::createContext(
        ImVue::createElementFactory(),
        state
  ));

  const char* data = "<template>"
    "<window name=\"static\">"
    "<input-text buf=\"fine\" buf-size=\"512\" ref=\"input\">console</input-text>"
    "</window>"
    "</template>"
    "<script>"
    "return ImVue.new({"
      "data = function() return {} end"
    "})\n"
    "</script>";

  document.parse(data);
  renderDocument(document);

  ImVue::Object obj = state->getObject("self._refs.input");
  EXPECT_EQ(obj.type(), ImVue::ObjectType::OBJECT);

  obj = state->getObject("self._refs.input.buf");
  EXPECT_EQ(obj.type(), ImVue::ObjectType::STRING);
  ASSERT_STREQ(obj.as<ImString>().get(), "fine");
}

TEST_F(LuaScriptStateTest, TestListModifications) {
  ImVue::LuaScriptState* state = new ImVue::LuaScriptState(L);
  ImVue::Document document(ImVue::createContext(
        ImVue::createElementFactory(),
        state
  ));

  const char* data = "<template>"
    "<window name=\"static\">"
    "<input-text :key=\"k\" v-for=\"(i, k) in self.items\">{{i}}</input>"
    "</window>"
    "</template>"
    "<script>"
    "return ImVue.new({"
      "data = function() return {"
        "items = {"
          "'first', 'second', 'last'"
        "}"
      "} end"
    "})\n"
    "</script>";

  const char* inputValue = "hello world";
  document.parse(data);
  renderDocument(document, 2);
  ImVector<ImVue::InputText*> inputs = document.getChildren<ImVue::InputText>("input-text", true);
  ASSERT_EQ(inputs.size(), 3);
  std::strcpy(&inputs[0]->buf[0], inputValue);

  // append
  state->eval("self.items[#self.items + 1] = 'test'");
  renderDocument(document, 2);
  inputs = document.getChildren<ImVue::InputText>("input-text", true);
  EXPECT_EQ(inputs.size(), 4);
  EXPECT_STREQ(inputs[0]->buf, inputValue);
  EXPECT_STREQ(inputs[0]->label, "first");
  EXPECT_STREQ(inputs[1]->label, "second");
  EXPECT_STREQ(inputs[2]->label, "last");
  EXPECT_STREQ(inputs[3]->label, "test");

  // owerwrite
  state->eval("self.items[1] = 'changed'");
  renderDocument(document, 3);
  inputs = document.getChildren<ImVue::InputText>("input-text", true);
  EXPECT_EQ(inputs.size(), 4);
  EXPECT_STREQ(inputs[0]->buf, inputValue);
  EXPECT_STREQ(inputs[0]->label, "changed");

  // remove second element
  state->eval("self.items:remove(2)");
  renderDocument(document, 3);
  inputs = document.getChildren<ImVue::InputText>("input-text", true);
  EXPECT_EQ(inputs.size(), 3);
  EXPECT_STREQ(inputs[0]->buf, inputValue);
  EXPECT_STREQ(inputs[0]->label, "changed");
  EXPECT_STREQ(inputs[1]->label, "last");

  // insert first
  state->eval("self.items:insert(1, 'new')");
  renderDocument(document, 3);
  inputs = document.getChildren<ImVue::InputText>("input-text", true);
  EXPECT_EQ(inputs.size(), 4);
  EXPECT_STREQ(inputs[0]->label, "new");
  EXPECT_STREQ(inputs[1]->label, "changed");

  // clear
  state->eval("self.items = {}");
  renderDocument(document, 3);
  inputs = document.getChildren<ImVue::InputText>("input-text", true);
  EXPECT_EQ(inputs.size(), 0);

  // add new value
  state->eval("self.items['aye'] = \"capt'n\"");
  renderDocument(document, 3);
  inputs = document.getChildren<ImVue::InputText>("input-text", true);
  EXPECT_EQ(inputs.size(), 1);
  EXPECT_STREQ(inputs[0]->label, "capt'n");
  EXPECT_STREQ(inputs[0]->key, "aye");
}

typedef std::tuple<const char*, const char*, int> VForParam;

class LuaVForTest : public ::testing::Test, public testing::WithParamInterface<VForParam> {
  protected:
    void SetUp() override {
      L = luaL_newstate();
      luaL_openlibs(L);
      ImVue::registerBindings(L);
      sc.init(L);
    }

    void TearDown() override {
      sc.check();
      lua_close(L);
    }

    lua_State* L;
    StackChecker sc;
};

TEST_P(LuaVForTest, TestVFor)
{
  ImVue::Document document(ImVue::createContext(
        ImVue::createElementFactory(),
        new ImVue::LuaScriptState(L)
        ));


  int nestedCount = 0;
  const char* vfor = {0};
  const char* list = {0};
  std::tie(vfor, list, nestedCount) = GetParam();
  std::stringstream data;
  data << "<template><window name=\"vfor test\" id=\"main\">";
  data << vfor;
  data << "</template></window>";

  data << "<script>"
    "state = ImVue.new({"
      "data = function() return { list = ";
  data << list;
  data << "} end }); return state</script>";

  document.parse(&data.str()[0]);
  renderDocument(document);

  if(nestedCount > 0) {
    ImVector<ImVue::TextUnformatted*> nested = document.getChildren<ImVue::TextUnformatted>("#nested", true);
    ASSERT_EQ(nested.size(), nestedCount);
  } else {
    ImVector<ImVue::Button*> buttons = document.getChildren<ImVue::Button>("button", true);
    ASSERT_EQ(buttons.size(), 2);
  }
}

// simple iteration, using both key and value
const char* elements[] = {
  "<button v-for=\"(value, key) in self.list\">{{key}} {{value}}</button>",
  "<button v-for=\"value in self.list\">{{value}}</button>",
  "<button v-for=\"(_, key) in self.list\">{{key}}</button>",
  "<child :str-id=\"key\" v-for=\"(value, key) in self.list\"><text-unformatted v-for=\"(v, key) in value\" id=\"nested\">{{key}}:{{v}}</text-unformatted></child>",
};

const char* data[] = {
  "{ first = 'hello', second = '1' }",
  "{ 'hello', '1' }",
  "{ l1 = {1, 2, 3}, l2 = { diddly = 'doodly' }, motto = {'omae wa mou shinde iru'} }"
};

// nested loop

INSTANTIATE_TEST_CASE_P(
  TestVFor,
  LuaVForTest,
  ::testing::Values(
    std::make_tuple(elements[0], data[0], 0),
    std::make_tuple(elements[1], data[0], 0),
    std::make_tuple(elements[2], data[0], 0),
    std::make_tuple(elements[0], data[1], 0),
    std::make_tuple(elements[1], data[1], 0),
    std::make_tuple(elements[2], data[1], 0),
    std::make_tuple(elements[3], data[2], 5)
  ));


// components test

TEST_F(LuaScriptStateTest, TestComponentReactivity)
{
  ImVue::LuaScriptState* state = new ImVue::LuaScriptState(L);
  ImVue::Context* ctx = ImVue::createContext(
    ImVue::createElementFactory(),
    state
  );
  luaL_dostring(L, "widget = 'basic'");
  ImVue::Document document(ctx);
  char* data = ctx->fs->load("components.xml");
  ASSERT_NE((size_t)data, 0);
  document.parse(data);
  ImGui::MemFree(data);
  renderDocument(document);

  ImVector<ImVue::TextUnformatted*> local = document.getChildren<ImVue::TextUnformatted>("#local", true);
  ImVector<ImVue::TextUnformatted*> parent = document.getChildren<ImVue::TextUnformatted>("#parent", true);
  EXPECT_EQ(local.size(), 1);
  EXPECT_EQ(parent.size(), 1);

  EXPECT_STREQ(local[0]->text, "works");
  EXPECT_STREQ(parent[0]->text, "1");

  // parent -> component invalidation
  state->eval("self.count = self.count + 1");
  renderDocument(document, 3);
  EXPECT_STREQ(parent[0]->text, "2");

  // component -> component invalidation
  parent[0]->getState()->eval("self.value = 'updated'");
  renderDocument(document, 3);
  EXPECT_STREQ(local[0]->text, "updated");
}

TEST_F(LuaScriptStateTest, TestComponentCreationError)
{
  ImVue::LuaScriptState* state = new ImVue::LuaScriptState(L);
  ImVue::Context* ctx = ImVue::createContext(
    ImVue::createElementFactory(),
    state
  );
  luaL_dostring(L, "widget = 'bad'");
  ImVue::Document document(ctx);
  char* data = ctx->fs->load("components.xml");
  ASSERT_NE((size_t)data, 0);
  EXPECT_THROW({
      try{
    document.parse(data);
    renderDocument(document);
    } catch(ImVue::ElementError e) {
      std::cout << e.what() << "\n";
      throw;
    }
  }, ImVue::ScriptError);
  ImGui::MemFree(data);
}

TEST_F(LuaScriptStateTest, TestComponentLifecycle)
{
  ImVue::LuaScriptState* state = new ImVue::LuaScriptState(L);
  ImVue::Context* ctx = ImVue::createContext(
    ImVue::createElementFactory(),
    state
  );
  luaL_dostring(L, "widget = 'lifecycle'");
  ImVue::Document document(ctx);
  char* data = ctx->fs->load("components.xml");
  ASSERT_NE((size_t)data, 0);
  document.parse(data);
  ImGui::MemFree(data);

  int count = 0;
  getLuaVariable(L, "calls", "beforeCreate", count);
  EXPECT_EQ(count, 1);
  getLuaVariable(L, "calls", "created", count);
  EXPECT_EQ(count, 1);
  getLuaVariable(L, "calls", "beforeMount", count);
  EXPECT_EQ(count, 0);
  getLuaVariable(L, "calls", "mounted", count);
  EXPECT_EQ(count, 0);

  renderDocument(document);

  getLuaVariable(L, "calls", "beforeMount", count);
  EXPECT_EQ(count, 1);
  getLuaVariable(L, "calls", "mounted", count);
  EXPECT_EQ(count, 1);
  getLuaVariable(L, "calls", "beforeUpdate", count);
  renderDocument(document);
  EXPECT_EQ(count, 0);
  getLuaVariable(L, "calls", "updated", count);
  EXPECT_EQ(count, 0);
  luaL_dostring(L, "state.count = 5");
  renderDocument(document, 2);
  getLuaVariable(L, "calls", "beforeUpdate", count);
  EXPECT_EQ(count, 1);
  getLuaVariable(L, "calls", "updated", count);
  EXPECT_EQ(count, 1);
}

typedef std::tuple<const char*, const char*, bool> ComponentPropsParam;

class LuaComponentPropsTest : public ::testing::Test, public testing::WithParamInterface<ComponentPropsParam> {
  protected:
    void SetUp() override {
      L = luaL_newstate();
      luaL_openlibs(L);
      ImVue::registerBindings(L);
      sc.init(L);
    }

    void TearDown() override {
      sc.check();
      lua_close(L);
    }

    lua_State* L;
    StackChecker sc;
};

const char* NO_VAL = "\0";

TEST_P(LuaComponentPropsTest, TestProps)
{
  const char* testCase;
  const char* val;
  bool shouldSucceed;
  std::tie(testCase, val, shouldSucceed) = GetParam();
  ImVue::LuaScriptState* state = new ImVue::LuaScriptState(L);
  ImVue::Context* ctx = ImVue::createContext(
    ImVue::createElementFactory(),
    state
  );
  luaL_dostring(L, "widget = 'props'");
  ImVue::Document document(ctx);
  char* data = ctx->fs->load("components.xml");
  ASSERT_NE((size_t)data, 0);

  std::stringstream ss;
  ss << "propsValidationTestCase = '" << testCase << "'";
  if(val != NO_VAL) {
    ss << "\nval = " << val;
  }
  luaL_dostring(L, ss.str().c_str());
  if(!shouldSucceed) {
    ASSERT_THROW({
      try {
        document.parse(data);
        renderDocument(document);
      } catch (...) {
        ImGui::MemFree(data);
        throw;
      }
    }, ImVue::ElementError);
  } else {
    document.parse(data);
    ImGui::MemFree(data);
  }
}


INSTANTIATE_TEST_CASE_P(
  TestProps,
  LuaComponentPropsTest,
  ::testing::Values(
    std::make_tuple("defValid", NO_VAL, true),
    std::make_tuple("defRequired", NO_VAL, true),
    std::make_tuple("defValidationSuccess", NO_VAL, true),
    std::make_tuple("defWrongType", NO_VAL, false),
    std::make_tuple("defValidationFail", NO_VAL, false),
    std::make_tuple("validateShortest", "'10'", true),
    std::make_tuple("validateString", "'10'", true),
    std::make_tuple("validateString", "10", false),
    std::make_tuple("validateRequired", "'10'", true),
    std::make_tuple("validateRequired", NO_VAL, false),
    std::make_tuple("validateTypeList", "1", true),
    std::make_tuple("validateTypeList", "'1'", true),
    std::make_tuple("validateArray", "{1,2,3}", true),
    std::make_tuple("validateArray", "{}", false),
    std::make_tuple("validateObject", "{1,2,3}", false),
    std::make_tuple("validateObject", "{}", true),
    std::make_tuple("validateBool", "true", true),
    std::make_tuple("validateBool", "false", true),
    std::make_tuple("validateBool", "0", true),
    std::make_tuple("validateBool", "1", true),
    std::make_tuple("validateCallback", "1", true),
    std::make_tuple("validateCallback", "0", false)
  ));

// TODO: component slot test

#endif
