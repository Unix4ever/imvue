
#include <gtest/gtest.h>
#include "imvue.h"
#include "imvue_generated.h"
#include "imvue_errors.h"
#include "utils.h"
#include "extras/xhtml.h"

#if defined(WITH_LUA)
#include "lua/script.h"
extern "C" {
  #include <lua.h>
  #include <lualib.h>
  #include <lauxlib.h>
}
#endif

class TestElement : public ImVue::ContainerElement {

  public:
    TestElement()
      : mForceState(0)
      , color(0)
    {
    }

    ImVec2 screenPos;
    ImU32 color;

    void lockState(char* id)
    {
      if(ImStricmp(id, "hover") == 0) {
        mForceState = ImVue::Element::HOVERED;
      } else if(ImStricmp(id, "active") == 0) {
        mForceState = ImVue::Element::ACTIVE;
      } else if(ImStricmp(id, "disabled") == 0) {
        mForceState = ImVue::Element::DISABLED;
      } else if(ImStricmp(id, "checked") == 0) {
        mForceState = ImVue::Element::CHECKED;
      } else if(ImStricmp(id, "link") == 0) {
        mForceState = ImVue::Element::LINK;
      } else if(ImStricmp(id, "focus") == 0) {
        mForceState = ImVue::Element::FOCUSED;
      } else if(ImStricmp(id, "visited") == 0) {
        mForceState = ImVue::Element::VISITED;
      }
      mState = mForceState;
      ImGui::MemFree(id);
    }

    void renderBody()
    {
      screenPos = ImGui::GetCursorScreenPos();
      ImVec2 pos = ImGui::GetCursorPos();
      ImVec2 size(20.0f + ImMax(0.0f, padding[0] + padding[2]), 20.0f + ImMax(0.0f, padding[1] + padding[3]));

      ImRect bb(pos, pos + size);
      ImGui::ItemSize(bb.GetSize(), 0);
      ImGui::ItemAdd(bb, index);
      ContainerElement::renderBody();

      color = ImGui::GetColorU32(ImGuiCol_Text);
    }

  private:

    unsigned int mForceState;

};

class TestStyles : public ::testing::Test {

  public:

    TestStyles()
      : mDoc(0)
    {
    }

    ~TestStyles()
    {
      if(mDoc) {
        delete mDoc;
        mDoc = 0;
      }
    }

    void TearDown() override
    {
      if(mDoc) {
        delete mDoc;
        mDoc = 0;
      }
    }

    ImVue::Document& createDoc(const char* data)
    {
      if(mDoc) {
        delete mDoc;
      }
      ImVue::ElementFactory* factory = ImVue::createElementFactory();
      factory->element<TestElement>("test");
      ImVue::Context* ctx = ImVue::createContext(factory);
      mDoc = new ImVue::Document(ctx);
      mDoc->parse(data);
      return *mDoc;
    }

  private:
    ImVue::Document* mDoc;
};

/**
 * Test various styles settings
 */
TEST_F(TestStyles, Padding)
{
  {
    ImVec2 pbefore = ImGui::GetStyle().WindowPadding;

    const char* doc = "<style>test { padding: 5px; }</style><template><test class='padded'/></template>";

    ImVue::Document& d = createDoc(doc);
    renderDocument(d);

    ImVec2 pafter = ImGui::GetStyle().WindowPadding;
    EXPECT_EQ(pbefore.x, pafter.x);
    EXPECT_EQ(pbefore.y, pafter.y);

    ImVector<TestElement*> els = d.getChildren<TestElement>(".padded");
    ASSERT_EQ(els.size(), 1);

    TestElement* element = els[0];
    EXPECT_FLOAT_EQ(element->padding[0], 5.0f);
    EXPECT_FLOAT_EQ(element->padding[1], 5.0f);
    EXPECT_FLOAT_EQ(element->padding[2], 5.0f);
    EXPECT_FLOAT_EQ(element->padding[3], 5.0f);
  }

  {
    const char* doc = "<style>test { padding: 5px 10px 20px 30px; }</style><template><test class='padded'/></template>";

    ImVue::Document& d = createDoc(doc);
    renderDocument(d);

    ImVector<TestElement*> els = d.getChildren<TestElement>(".padded");
    ASSERT_EQ(els.size(), 1);

    TestElement* element = els[0];
    EXPECT_FLOAT_EQ(element->padding[1], 5.0f);
    EXPECT_FLOAT_EQ(element->padding[2], 10.0f);
    EXPECT_FLOAT_EQ(element->padding[3], 20.0f);
    EXPECT_FLOAT_EQ(element->padding[0], 30.0f);
  }

  {
    const char* doc = "<style>test { padding: 5px 10px 20px 30%; }</style><template><test class='padded'/></template>";

    ImVue::Document& d = createDoc(doc);
    renderDocument(d);

    ImVector<TestElement*> els = d.getChildren<TestElement>(".padded");
    ASSERT_EQ(els.size(), 1);

    TestElement* element = els[0];
    EXPECT_FLOAT_EQ(element->padding[1], 5.0f);
    EXPECT_FLOAT_EQ(element->padding[2], 10.0f);
    EXPECT_FLOAT_EQ(element->padding[3], 20.0f);
    EXPECT_FLOAT_EQ(element->padding[0], ImGui::GetIO().DisplaySize.x / 100 * 30);
  }
}

TEST_F(TestStyles, Display)
{
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

  // display block
  {
    const char* doc = "<style>test { display: block; }\nwindow { padding: 0; position: absolute; left: 0; right: 0; top: 0; bottom: 0; }</style><template>"
      "<window name='test' flags='1'>"
        "<test class='display-test'/><test class='display-test'/>"
      "</window>"
    "</template>";

    ImVue::Document& d = createDoc(doc);
    renderDocument(d);

    ImVector<TestElement*> els = d.getChildren<TestElement>(".display-test", true);
    ASSERT_EQ(els.size(), 2);

    EXPECT_FLOAT_EQ(els[0]->pos.x, 0.0f);
    EXPECT_FLOAT_EQ(els[1]->pos.x, 0.0f);
    EXPECT_FLOAT_EQ(els[0]->pos.x, 0.0f);
    EXPECT_FLOAT_EQ(els[1]->pos.y, els[0]->computedSize.y);
  }

  // display inline
  {
    const char* doc = "<style>test { display: inline; width: 300px; padding: 50px; }\nwindow { padding: 0; position: absolute; left: 0; right: 0; top: 0; bottom: 0; }</style><template>"
      "<window name='test' flags='1'>"
        "<test class='display-test'/><test class='display-test'/>"
      "</window>"
    "</template>";

    ImVue::Document& d = createDoc(doc);
    renderDocument(d);

    ImVector<TestElement*> els = d.getChildren<TestElement>(".display-test", true);
    ASSERT_EQ(els.size(), 2);

    EXPECT_FLOAT_EQ(els[0]->pos.x, 0.0f);
    EXPECT_FLOAT_EQ(els[0]->pos.x, 0.0f);
    EXPECT_FLOAT_EQ(els[1]->pos.x, els[0]->computedSize.x);
    EXPECT_FLOAT_EQ(els[1]->pos.y, 0.0f);
  }

  // display inline-block
  {
    const char* doc = "<style>test { display: inline-block; padding: 2px; }\nwindow { padding: 0; position: absolute; left: 0; right: 0; top: 0; bottom: 0; }</style><template>"
      "<window name='test' flags='1'>"
        "<test class='display-test'/><test class='display-test'/>"
      "</window>"
    "</template>";

    ImVue::Document& d = createDoc(doc);
    renderDocument(d);

    ImVector<TestElement*> els = d.getChildren<TestElement>(".display-test", true);
    ASSERT_EQ(els.size(), 2);

    EXPECT_FLOAT_EQ(els[0]->pos.x, 0.0f);
    EXPECT_FLOAT_EQ(els[0]->pos.x, 0.0f);
    EXPECT_FLOAT_EQ(els[1]->pos.x, els[0]->computedSize.x);
    EXPECT_FLOAT_EQ(els[1]->pos.y, 0.0f);

    EXPECT_FLOAT_EQ(els[0]->computedSize.x, 24.0f);
  }

  ImGui::PopStyleVar(3);
}

TEST_F(TestStyles, Dimensions)
{
  const char* doc = "<style>"
      "test.full-size { width: 100%; height: 100%; }"
      "test.fixed-size { width: 40px; height: 45px; }"
      "window { width: 320px; height: 240px; }"
      "window.third { position: absolute; left: 0px; top: 0px; width: 320px; height: 240px; }"
      ".padded { padding: 5px; }"
    "</style>"
    "<template>"
      "<window class='first' name='test1' flags='1'>"
        "<test class='full-size'/>"
      "</window>"
      "<window class='second' name='test2' flags='1'>"
        "<test class='fixed-size'/>"
      "</window>"
      "<window class='third padded' name='test3' flags='1'>"
        "<test class='full-size'/>"
      "</window>"
    "</template>";

  ImVue::Document& d = createDoc(doc);
  renderDocument(d);

  ImVector<ImVue::Window*> windows = d.getChildren<ImVue::Window>("window");
  ASSERT_EQ(windows.size(), 3);

  {
    ImVue::Window* window = windows[0];
    ImVector<TestElement*> els = window->getChildren<TestElement>(".full-size");
    ASSERT_EQ(els.size(), 1);

    EXPECT_EQ(window->size.x, 320.0f);
    EXPECT_EQ(window->size.y, 240.0f);

    EXPECT_EQ(els[0]->size.x, window->size.x);
    EXPECT_EQ(els[0]->size.y, window->size.y);
  }

  {
    ImVue::Window* window = windows[1];
    ImVector<TestElement*> els = window->getChildren<TestElement>(".fixed-size");
    ASSERT_EQ(els.size(), 1);

    EXPECT_EQ(window->size.x, 320.0f);
    EXPECT_EQ(window->size.y, 240.0f);

    EXPECT_EQ(els[0]->size.x, 40.0f);
    EXPECT_EQ(els[0]->size.y, 45.0f);
  }

  {
    ImVue::Window* window = windows[2];
    ImVector<TestElement*> els = window->getChildren<TestElement>(".full-size");
    ASSERT_EQ(els.size(), 1);

    EXPECT_EQ(window->size.x, 320.0f);
    EXPECT_EQ(window->size.y, 240.0f);

    EXPECT_EQ(els[0]->screenPos.x, 5.0f);
    EXPECT_EQ(els[0]->screenPos.y, 5.0f);

    EXPECT_EQ(els[0]->size.x, window->size.x - 10.0f);
    EXPECT_EQ(els[0]->size.y, window->size.y - 10.0f);
  }
}

TEST_F(TestStyles, Position)
{
  const char* doc = "<style>"
      "window { width: 320px; height: 240px; }"
      "window.first { position: absolute; top: 20px; left: 30px; padding: 0; }"
      "test.inline { display: inline; }"
      "test.top-left { position: absolute; top: 5px; left: 5px; width: 30px; height: 20px; }"
      "test.top-right { position: absolute; top: 5px; right: 5px; width: 30px; height: 20px; }"
      "test.bottom-left { position: absolute; bottom: 5px; left: 5px; width: 30px; height: 20px; }"
      "test.bottom-right { position: absolute; bottom: 5px; right: 5px; width: 30px; height: 20px; }"

      "test.full-width-top { position: absolute; top: 5px; left: 5px; right: 5px; width: 30px; height: 20px; }"
      "test.full-width-bottom { position: absolute; bottom: 5px; left: 5px; right: 5px; width: 30px; height: 20px; }"
      "test.full-height-left { position: absolute; top: 5px; bottom: 5px; left: 5px; width: 30px; height: 20px; }"
      "test.full-height-right { position: absolute; bottom: 5px; top: 5px; right: 5px; width: 30px; height: 20px; }"
      "test.full-size { position: absolute; top: 5px; bottom: 5px; left: 5px; right: 5px; width: 30px; height: 20px; }"

      ".relative-parent { position: relative; left: 10px; }"
      ".relative-parent > test { position: absolute; top: 5px; left: 5px; width: 30px; height: 20px; }"
    "</style>"
    "<template>"
      "<window class='first' name='test1' flags='1'>"
        "<test class='absolute top-left'/>"
        "<test class='inline'/>"
        "<test class='absolute top-right'/>"
        "<test class='inline'/>"
        "<test class='absolute bottom-left'/>"
        "<test class='absolute bottom-right'/>"
        "<test class='absolute full-width-top'/>"
        "<test class='absolute full-width-bottom'/>"
        "<test class='absolute full-height-left'/>"
        "<test class='absolute full-height-right'/>"
        "<test class='absolute full-size'/>"
        "<test class='relative-parent'>"
          "<test class='absolute'/>"
        "</test>"
      "</window>"
    "</template>";
  ImVue::Document& d = createDoc(doc);
  renderDocument(d);

  ImVector<ImVue::Window*> windows = d.getChildren<ImVue::Window>("window");
  ASSERT_GT(windows.size(), 0);
  {
    ImVue::Window* window = windows[0];
    ImVector<TestElement*> els = window->getChildren<TestElement>(".absolute", true);
    ASSERT_EQ(els.size(), 10);

    ImVec2 wPos(30.0f, 20.0f);
    float margin = 5.0f;
    float left = wPos.x + margin;
    float right = wPos.x + window->size.x - margin;
    float top = wPos.y + margin;
    float bottom = wPos.y + window->size.y - margin;

    ImVec2 defaultSize(30.0f, 20.0f);

    EXPECT_EQ(window->size.x, 320.0f);
    EXPECT_EQ(window->size.y, 240.0f);

    // top left
    EXPECT_EQ(els[0]->screenPos.x, left);
    EXPECT_EQ(els[0]->screenPos.y, top);

    EXPECT_EQ(els[0]->size.x, defaultSize.x);
    EXPECT_EQ(els[0]->size.y, defaultSize.y);

    // top right
    EXPECT_EQ(els[1]->screenPos.x, right - els[1]->size.x);
    EXPECT_EQ(els[1]->screenPos.y, top);

    EXPECT_EQ(els[1]->size.x, defaultSize.x);
    EXPECT_EQ(els[1]->size.y, defaultSize.y);

    // bottom left
    EXPECT_EQ(els[2]->screenPos.x, left);
    EXPECT_EQ(els[2]->screenPos.y, bottom - els[2]->size.y);

    EXPECT_EQ(els[2]->size.x, defaultSize.x);
    EXPECT_EQ(els[2]->size.y, defaultSize.y);

    // bottom right
    EXPECT_EQ(els[3]->screenPos.x, right - els[3]->size.x);
    EXPECT_EQ(els[3]->screenPos.y, bottom - els[3]->size.y);

    EXPECT_EQ(els[3]->size.x, defaultSize.x);
    EXPECT_EQ(els[3]->size.y, defaultSize.y);

    // full-width-top
    EXPECT_EQ(els[4]->screenPos.x, left);
    EXPECT_EQ(els[4]->screenPos.y, top);

    float itemWidth = window->size.x - margin * 2;
    float itemHeight = window->size.y - margin * 2;

    EXPECT_EQ(els[4]->size.x, itemWidth);
    EXPECT_EQ(els[4]->size.y, defaultSize.y);

    // full-width-bottom
    EXPECT_EQ(els[5]->screenPos.x, left);
    EXPECT_EQ(els[5]->screenPos.y, bottom - els[3]->size.y);

    EXPECT_EQ(els[5]->size.x, itemWidth);
    EXPECT_EQ(els[5]->size.y, defaultSize.y);

    // full-height-left
    EXPECT_EQ(els[6]->screenPos.x, left);
    EXPECT_EQ(els[6]->screenPos.y, top);

    EXPECT_EQ(els[6]->size.x, defaultSize.x);
    EXPECT_EQ(els[6]->size.y, itemHeight);

    // full-height-right
    EXPECT_EQ(els[7]->screenPos.x, right - els[7]->size.x);
    EXPECT_EQ(els[7]->screenPos.y, top);

    EXPECT_EQ(els[7]->size.x, defaultSize.x);
    EXPECT_EQ(els[7]->size.y, itemHeight);

    // full-size
    EXPECT_EQ(els[8]->screenPos.x, left);
    EXPECT_EQ(els[8]->screenPos.y, top);

    EXPECT_EQ(els[8]->size.x, itemWidth);
    EXPECT_EQ(els[8]->size.y, itemHeight);

    ImVector<TestElement*> inl = window->getChildren<TestElement>(".inline");
    ASSERT_EQ(inl.size(), 2);
    // finally verify wrap mode
    EXPECT_EQ(inl[0]->pos.x, 0);
    EXPECT_EQ(inl[0]->pos.y, 0);

    // test same-line behaviour
    EXPECT_EQ(inl[1]->pos.x, inl[0]->getSize().x);
    EXPECT_EQ(inl[1]->pos.y, 0);

    ImVector<TestElement*> rel = window->getChildren<TestElement>(".relative-parent");
    ASSERT_GT(rel.size(), 0);

    EXPECT_EQ(els[9]->screenPos.x, rel[0]->screenPos.x + 5);
    EXPECT_EQ(els[9]->screenPos.y, rel[0]->screenPos.y + 5);
  }
}

TEST_F(TestStyles, Color)
{
  const char* doc = "<style>.col-set { color: #FFCC00; }</style>"
    "<template>"
      "<test id='col-direct' class='col-set'/>"
      "<test class='col-set'>"
        "<test id='col-inherit'/>"
      "</test>"
      "<test id='default-col'/>"
    "</template>"
  ;
  ImVue::Document& d = createDoc(doc);
  renderDocument(d);

  {
    ImVector<TestElement*> els = d.getChildren<TestElement>("#col-direct", true);
    ASSERT_EQ(els.size(), 1);

    EXPECT_EQ(els[0]->color, 0xFF00CCFF);
  }

  {
    ImVector<TestElement*> els = d.getChildren<TestElement>("#col-inherit", true);
    ASSERT_EQ(els.size(), 1);

    EXPECT_EQ(els[0]->color, 0xFF00CCFF);
  }

  {
    ImVector<TestElement*> els = d.getChildren<TestElement>("#default-col", true);
    ASSERT_EQ(els.size(), 1);

    EXPECT_EQ(els[0]->color, 0xFFFFFFFF);
  }
}

TEST_F(TestStyles, BgColor)
{
  const char* doc = "<style>.col-set { background-color: #FFCC00; }</style>"
    "<template>"
      "<test id='col-direct' class='col-set'/>"
      "<test class='col-set'>"
        "<test id='col-inherit'/>"
      "</test>"
      "<test id='default-col'/>"
    "</template>"
  ;
  ImVue::Document& d = createDoc(doc);
  renderDocument(d);

  {
    ImVector<TestElement*> els = d.getChildren<TestElement>("#col-direct", true);
    ASSERT_EQ(els.size(), 1);

    EXPECT_EQ(els[0]->bgColor, 0xFF00CCFF);
  }

  {
    ImVector<TestElement*> els = d.getChildren<TestElement>("#default-col", true);
    ASSERT_EQ(els.size(), 1);

    EXPECT_EQ(els[0]->bgColor, 0);
  }
}

TEST_F(TestStyles, FontSize)
{
  const char* doc = "<style>"
    "test.absolute { font-size: 20px; }"
    "test.relative { font-size: 1.2em; }"
    "</style>"
    "<template>"
      "<test id='font-size-absolute' class='absolute'/>"
      "<test class='absolute'>"
        "<test id='font-size-relative' class='relative'/>"
      "</test>"
      "<test class='absolute'>"
        "<test id='font-size-inherited'/>"
      "</test>"
    "</template>"
  ;
  ImVue::Document& d = createDoc(doc);
  renderDocument(d);

  {
    ImVector<TestElement*> els = d.getChildren<TestElement>("#font-size-absolute", true);
    ASSERT_EQ(els.size(), 1);

    EXPECT_FLOAT_EQ(els[0]->style()->fontSize, 20.0f);
  }

  {
    ImVector<TestElement*> els = d.getChildren<TestElement>("#font-size-relative", true);
    ASSERT_EQ(els.size(), 1);

    EXPECT_TRUE(els[0]->style()->fontSize - 20.0f * 1.2f < 0.005);
  }

  {
    ImVector<TestElement*> els = d.getChildren<TestElement>("#font-size-inherited", true);
    ASSERT_EQ(els.size(), 1);

    EXPECT_FLOAT_EQ(els[0]->style()->fontSize, 20.0f);
  }
}

TEST_F(TestStyles, ScaleRelativeToFontSize) {
  const char* doc = "<style>"
    ":root { font-size: 30px; }"
    "test { font-size: 20px; }"
    "</style>"
    "<template>"
      "<test>"
        "<test id='relative-to-parent' style='height: 2em;'/>"
        "<test id='relative-to-root' style='height: 2rem;'/>"
      "</test>"
    "</template>"
  ;
  ImVue::Document& d = createDoc(doc);
  renderDocument(d);

  {
    ImVector<TestElement*> els = d.getChildren<TestElement>("#relative-to-parent", true);
    ASSERT_EQ(els.size(), 1);

    EXPECT_FLOAT_EQ(els[0]->size.y, 40.0f);
  }

  {
    ImVector<TestElement*> els = d.getChildren<TestElement>("#relative-to-root", true);
    ASSERT_EQ(els.size(), 1);

    EXPECT_FLOAT_EQ(els[0]->size.y, 60.0f);
  }
}


TEST_F(TestStyles, FontFamily) {
  const char* doc = "<style>"
    "@font-face {"
      "font-family: \"DroidSans\";"
      "src: local(\"../../imgui/misc/fonts/DroidSans.ttf\");"
    "}"
    "</style>"
    "<template>"
      "<test id='font-family-set-directly' style='font-family: DroidSans;'/>"
      "<test id='font-family-fallback' style='font-family: sans, DroidSans;'/>"
      "<test id='fail-to-set' style='font-family: sans;'/>"
    "</template>"
  ;

  ImVue::Document& d = createDoc(doc);
  renderDocument(d);

  {
    ImVector<TestElement*> els = d.getChildren<TestElement>("#font-family-set-directly", true);
    ASSERT_EQ(els.size(), 1);

    EXPECT_STREQ(els[0]->style()->fontName, "DroidSans");
  }

  {
    ImVector<TestElement*> els = d.getChildren<TestElement>("#font-family-fallback", true);
    ASSERT_EQ(els.size(), 1);

    EXPECT_STREQ(els[0]->style()->fontName, "DroidSans");
  }

  {
    ImVector<TestElement*> els = d.getChildren<TestElement>("#fail-to-set", true);
    ASSERT_EQ(els.size(), 1);

    EXPECT_STREQ(els[0]->style()->fontName, NULL);
  }
}

typedef std::tuple<const char*, int*, const char*, const char*> SelectionTestParam;

class SelectionTest : public ::testing::Test, public testing::WithParamInterface<SelectionTestParam> {
#if defined(WITH_LUA)
  protected:
    void SetUp() override {
      L = luaL_newstate();
      luaL_openlibs(L);
      ImVue::registerBindings(L);
    }

    void TearDown() override {
      lua_close(L);
    }

    lua_State* L;
#endif
};

bool shouldApplyStyle(int index, int* indices) {
  for(int i = 0; i < 1024; ++i) {
    if(indices[i] == -1) {
      break;
    }

    if(indices[i] == index) {
      return true;
    }
  }

  return false;
}

TEST_P(SelectionTest, TestStylesApplied) {
  ImVue::ElementFactory* factory = ImVue::createElementFactory();
  factory->element<TestElement>("test")
    .setter("state", &TestElement::lockState);

  const char* testcase;
  const char* file;
  const char* query;
  int* expectedIndices;
  std::tie(testcase, expectedIndices, query, file) = GetParam();

  ImVue::Context* ctx = ImVue::createContext(factory
#if defined(WITH_LUA)
      ,
      new ImVue::LuaScriptState(L)
#endif
  );
  ImVue::Document doc(ctx);
  char* data = ctx->fs->load(file);
  try {
    doc.parse(data);
  } catch(...) {
    ImGui::MemFree(data);
    throw;
  }
  ImGui::MemFree(data);

  renderDocument(doc);

  ImVector<ImVue::ContainerElement*> containers = doc.getChildren<ImVue::ContainerElement>(testcase, true);
  ASSERT_EQ(containers.size(), 1);

  ImVector<ImVue::Element*> elements = containers[0]->getChildren<ImVue::Element>(query, true);
  ASSERT_GT(elements.size(), 0);
  for(int i = 0; i < (int)elements.size(); i++) {
    if(shouldApplyStyle(i, expectedIndices)) {
      EXPECT_EQ(elements[i]->bgColor, 0xFF00FF00);
    } else {
      EXPECT_EQ(elements[i]->bgColor, 0xFF0000FF);
    }
  }
}

const char* staticTest = "selection.xml";
const char* scriptedTest = "selection_scripted.xml";

static int nameSelect[] = {0, -1};
static int classSelect[] = {1, 2, -1};
static int idSelect[] = {2, -1};
static int matchingAllClasses[] = {1, 2, -1};
static int matchingClassAndID[] = {1, -1};
static int nested[] = {0, 1, -1};
static int directParent[] = {0, 2, -1};
static int nextElement[] = {3, 5, -1};
static int previousElement[] = {2, 4, -1};
static int attrDefined[] = {1, -1};
static int attrEqual[] = {0, -1};
static int stateHover[] = {0, -1};
static int stateActive[] = {1, -1};
static int stateDisabled[] = {2, -1};
static int stateChecked[] = {3, -1};
static int stateOther[] = {4, -1};
static int array[] = {1, 4, -1};
static int arrayOfType[] = {0, 4, -1};
#if defined(WITH_LUA)
static int pseudoElement[] = {0, 4, -1};
static int pseudoElementFlat[] = {0, 6, 7, -1};
#endif

INSTANTIATE_TEST_CASE_P(
  TestStylesApplied,
  SelectionTest,
  ::testing::Values(
    std::make_tuple(".name-select", nameSelect, "*", staticTest),
    std::make_tuple(".class-select", classSelect, "test", staticTest),
    std::make_tuple(".id-select", idSelect, "test", staticTest),
    std::make_tuple(".matching-all-classes", matchingAllClasses, "test", staticTest),
    std::make_tuple(".class-plus-id", matchingClassAndID, "test", staticTest),
    std::make_tuple(".nested", nested, "test", staticTest),
    std::make_tuple(".direct-parent", directParent, "test", staticTest),
    std::make_tuple(".next-element", nextElement, "test", staticTest),
    std::make_tuple(".previous-element", previousElement, "test", staticTest),
    std::make_tuple(".attr-defined", attrDefined, "test", staticTest),
    std::make_tuple(".attr-equal", attrEqual, "test", staticTest),
    std::make_tuple(".states-hover", stateHover, "test", staticTest),
    std::make_tuple(".states-active", stateActive, "test", staticTest),
    std::make_tuple(".states-disabled", stateDisabled, "test", staticTest),
    std::make_tuple(".states-checked", stateChecked, "test", staticTest),
    std::make_tuple(".states-link", stateOther, "test", staticTest),
    std::make_tuple(".states-focus", stateOther, "test", staticTest),
    std::make_tuple(".states-visited", stateOther, "test", staticTest),
    std::make_tuple(".array", array, "test", staticTest),
    std::make_tuple(".array-of-type", arrayOfType, "test", staticTest)
#if defined(WITH_LUA)
    ,
    std::make_tuple(".pseudo-element", pseudoElement, "test", scriptedTest),
    std::make_tuple(".pseudo-element-flat", pseudoElementFlat, "test", scriptedTest)
#endif
 ));

#if defined(WITH_LUA)

class TestStylesComponents : public ::testing::Test {

  public:

    TestStylesComponents()
      : mDoc(0)
    {
    }

    ~TestStylesComponents()
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
      factory->element<TestElement>("test");

      ImVue::Context* ctx = ImVue::createContext(factory, new ImVue::LuaScriptState(L));
      ImVue::Document doc(ctx);
      char* data = ctx->fs->load(path);
      try {
        mDoc = new ImVue::Document(ctx);
        mDoc->parse(data);
      } catch(...) {
        delete mDoc;
        ImGui::MemFree(data);
        throw;
      }
      ImGui::MemFree(data);

      return *mDoc;
    }

  private:
    ImVue::Document* mDoc;
    lua_State* L;
};

TEST_F(TestStylesComponents, Dimensions)
{
  ImVue::Document& d = createDoc("dimensions.xml");
  ImVector<ImVue::HtmlContainer*> els = d.getChildren<ImVue::HtmlContainer>("#check", true);

  ASSERT_GT(els.size(), 0);

  renderDocument(d);

  ImVue::HtmlContainer* div = els[0];
  ImVec2 size = div->getSize();
  EXPECT_EQ(size.x, 300);
  EXPECT_EQ(size.y, 300);
}

#endif
