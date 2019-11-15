
#include <gtest/gtest.h>
#include "imvue.h"
#include "imvue_generated.h"
#include "imvue_errors.h"
#include "utils.h"

const char* simple =
"<template>"
  "<window name=\"test\">"
    "<button>button</button>"
  "</window>"
"</template>";

const char* badElements =
"<template>"
  "<no-such-element some=\"a\">hello</no-such-element>"
"</template>";

const char* badStructure =
"<button>I am the button</button>";

const char* badScript =
"<template><button>I am the button</button></template>"
"<script></script>";

/**
 * Parse and render simple document
 */
TEST(DocumentParser, TestSimple)
{
  ImVue::Document document;
  document.parse(simple);
  renderDocument(document);
}

/**
 * Check valid xml with not existing elements
 *
 * Expected to throw exception if found unknown elements
 */
TEST(DocumentParser, BadElements)
{

  ImVue::Document document;
  bool gotError = false;
  ASSERT_THROW(document.parse(badElements), ImVue::ElementError);
  renderDocument(document);
}

/**
 * Check valid xml with wrong structure
 *
 * Expected to raise DocumentError
 */
TEST(DocumentParser, BadStructure)
{
  ImVue::Document document;
  document.parse(badStructure);
  renderDocument(document);
}

/**
 * Script state is not initialized, should handle that
 *
 * Expected to raise DocumentError
 */
TEST(DocumentParser, BadScript)
{
  ImVue::Document document;
  document.parse(badScript);
  renderDocument(document);
}

/**
 * Create document and copy it, should delete, ctx and other pointers only once
 */
TEST(DocumentParser, DocumentCopy)
{
  ImVue::Context* ctx = ImVue::createContext(
      ImVue::createElementFactory()
  );
  ImVue::Document document(ctx);
  document.parse(simple);
  renderDocument(document);

  ImVue::Document copied(document);
  renderDocument(document);
  renderDocument(copied);

  ImVue::Document duplicate = document;
  renderDocument(duplicate);
}

#if defined(WITH_LUA)
#include "lua/script.h"
extern "C" {
  #include <lua.h>
  #include <lualib.h>
  #include <lauxlib.h>
}
#endif

class Validator : public ImVue::Element
{
  public:
    Validator()
      : arrVariadic(nullptr)
      , ch(nullptr)
    {
    }

    ~Validator()
    {
      if(ch) ImGui::MemFree(ch);
      if(arrVariadic) ImGui::MemFree(arrVariadic);
    }

    void renderBody()
    {
    }

    bool flag;
    float fl;
    double dbl;
    ImU16 u16;
    ImU32 u32;
    ImU64 u64;
    short i16;
    int i32;
    long i64;
    ImVec2 vec2;
    ImVec4 vec4;
    char* ch;
    int arrFixed[3];
    int* arrVariadic;
#if defined(WITH_LUA)
    ImVue::Object object;
#endif
};

typedef std::tuple<const char*, bool> ReadTypesParam;

class ReadTypesTest : public ::testing::Test, public testing::WithParamInterface<ReadTypesParam> {
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

/**
 * Test parsing various types
 */
TEST_P(ReadTypesTest, ParseTypes)
{
  ImVue::ElementFactory* factory = ImVue::createElementFactory();
  factory->element<Validator>("validator")
    .attribute("flag", &Validator::flag, true)
    .attribute("float", &Validator::fl, true)
    .attribute("double", &Validator::dbl, true)
    .attribute("u16", &Validator::u16, true)
    .attribute("u32", &Validator::u32, true)
    .attribute("u64", &Validator::u64, true)
    .attribute("i16", &Validator::i16, true)
    .attribute("i32", &Validator::i32, true)
    .attribute("i64", &Validator::i64, true)
    .attribute("ch", &Validator::ch, true)
    .attribute("vec2", &Validator::vec2, true)
    .attribute("vec4", &Validator::vec4, true)
    .attribute("arr-fixed", &Validator::arrFixed, true)
    .attribute("arr-variadic", &Validator::arrVariadic, true)
#if defined(WITH_LUA)
    .attribute("object", &Validator::object)
#endif
    ;

  ImVue::Context* ctx = ImVue::createContext(
      factory
#if defined(WITH_LUA)
      ,
      new ImVue::LuaScriptState(L)
#endif
  );
  ImVue::Document document(ctx);
  const char* body = NULL;
  bool checkObject;
  std::tie(body, checkObject) = GetParam();
  document.parse(body);
  renderDocument(document);
  ImVector<Validator*> children = document.getChildren<Validator>("validator", true);
  Validator* el = children[0];
  EXPECT_EQ(el->u16, 65535);
  EXPECT_EQ(el->u32, 65536);
  EXPECT_EQ(el->u64, 4294967295);
  EXPECT_EQ(el->i16, -32765);
  EXPECT_EQ(el->i32, -65536);
  EXPECT_EQ(el->i64, -1073741823);
  EXPECT_FLOAT_EQ(el->vec2.x, 2);
  EXPECT_FLOAT_EQ(el->vec2.y, 2);

  EXPECT_FLOAT_EQ(el->vec4.x, 2);
  EXPECT_FLOAT_EQ(el->vec4.y, 2);
  EXPECT_FLOAT_EQ(el->vec4.z, 2);
  EXPECT_FLOAT_EQ(el->vec4.w, 2);

  EXPECT_FLOAT_EQ(el->fl, 0.1);
  EXPECT_DOUBLE_EQ(el->dbl, 0.05);

  EXPECT_STREQ(el->ch, "hello");
  for(size_t i = 0; i < 3; ++i) {
    EXPECT_EQ(el->arrFixed[i], i + 1);
  }

  ASSERT_NE(el->arrVariadic, nullptr);
  for(size_t i = 0; i < 5; ++i) {
    EXPECT_EQ(el->arrVariadic[i], i + 1);
  }
#if defined(WITH_LUA)
  if(checkObject) {
    ASSERT_NE(el->object.type(), ImVue::ObjectType::NIL);
    EXPECT_STREQ(el->object["it"].as<ImString>(), "works");
  }
#endif
}

const char* dataStatic = "<template>"
  "<validator "
    "flag='true' "
    "u16='65535' u32='65536' u64='4294967295' "
    "i16='-32765' i32='-65536' i64='-1073741823' "
    "vec2='{2,2}' vec4='{2,2,2,2}' "
    "float='0.1' double='0.05' "
    "ch='hello' "
    "arr-fixed='{1,2,3}' arr-variadic='{1,2,3,4,5}' "
    "/>"
"</template>";

const char* dataBind = "<template>"
  "<validator "
    ":flag='true' "
    ":u16='65535' :u32='65536' :u64='4294967295' "
    ":i16='-32765' :i32='-65536' :i64='-1073741823' "
    ":vec2='{2,2}' :vec4='{2,2,2,2}' "
    ":ch='\"hello\"' "
    ":arr-fixed='{1,2,3}' :arr-variadic='{1,2,3,4,5}' "
    ":float='0.1' :double='0.05' "
    ":object='{it=\"works\"}'"
    "/>"
"</template>"
"<script>"
  "return ImVue.new({})"
"</script>";

INSTANTIATE_TEST_CASE_P(
  ParseTypes,
  ReadTypesTest,
  ::testing::Values(
    std::make_tuple(dataStatic, false)
#if defined(WITH_LUA)
    ,
    std::make_tuple(dataBind, true)
#endif
  )
);

/**
 * Test reading various types
 * low level test
 */
TEST(DocumentParser, StaticArrayReader) {
  // reading array of length 1
  {
    float vals[1] = {0.0f};
    EXPECT_TRUE(ImVue::detail::read("{1.2}", &vals));
    EXPECT_FLOAT_EQ(vals[0], 1.2);
  }
  // reading array of length 2
  {
    double vals[2] = {0.0};
    EXPECT_TRUE(ImVue::detail::read("{1.2, 2.4}", &vals));
    EXPECT_DOUBLE_EQ(vals[0], 1.2);
    EXPECT_DOUBLE_EQ(vals[1], 2.4);
  }
  // various correct inputs with traling spaces/different bracket types
  {
    const char* cases[4] = {
      "  \t\n{1.2, 2.4}\n\t",
      "[1.2, 2.4]",
      "(1.2, 2.4)",
      "1.2, 2.4"
    };

    for (int i = 0; i < 4; ++i) {
      double vals[2] = {0.0};
      EXPECT_TRUE(ImVue::detail::read(cases[i], &vals));
      EXPECT_DOUBLE_EQ(vals[0], 1.2);
      EXPECT_DOUBLE_EQ(vals[1], 2.4);
    }
  }
  // various incorrect inputs
  {
    const char* cases[4] = {
      "{[1.2, 2.4]",
      "(1.2, 2.4}",
      "{}",
      " "
    };

    for (int i = 0; i < 4; ++i) {
      double vals[2] = {0.0};
      EXPECT_FALSE(ImVue::detail::read(cases[i], &vals));
      EXPECT_DOUBLE_EQ(vals[0], 0.0);
      EXPECT_DOUBLE_EQ(vals[1], 0.0);
    }
  }
  // variable array length
  {
    int* vars = nullptr;
    EXPECT_TRUE(ImVue::detail::read("{1,2,3,4,5,6}", &vars));
    EXPECT_NE(vars, nullptr);
    for (int i = 0; i < 6; ++i) {
      EXPECT_EQ(vars[i], i+1);
    }
    // re-read to verify proper memory usage
    EXPECT_TRUE(ImVue::detail::read("{1,2,3,4,5,6}", &vars));
    EXPECT_NE(vars, nullptr);
    for (int i = 0; i < 6; ++i) {
      EXPECT_EQ(vars[i], i+1);
    }
    ImGui::MemFree(vars);
  }
}

class BaseRequiredValidator : public ImVue::Element
{
  public:
    void renderBody()
    {
    }

    int base;
};

class RequiredValidator : public BaseRequiredValidator
{
  public:
    int value;
};
/**
 * Test required fields validation
 */
TEST(DocumentParser, RequiredPropsValidation) {

  ImVue::ElementFactory* factory = ImVue::createElementFactory();
  factory->element<BaseRequiredValidator>("base-validator")
    .attribute("base", &BaseRequiredValidator::base, true);

  factory->element<RequiredValidator>("validator")
    .inherits("base-validator")
    .attribute("value", &RequiredValidator::value, true);

  ImVue::Context* ctx = ImVue::createContext(
      factory
  );

  ImVue::Document document(ctx);
  {
    const char* body = "<template><validator value='crashme'/></template>";

    bool gotError = false;
    ASSERT_THROW(document.parse(body), ImVue::ElementError);
    renderDocument(document);
  }

  {
    const char* body = "<template><validator value='1' base='?'/></template>";

    bool gotError = false;
    ASSERT_THROW(document.parse(body), ImVue::ElementError);
    renderDocument(document);
  }

  {
    const char* body = "<template><validator/></template>";

    bool gotError = false;
    ASSERT_THROW(document.parse(body), ImVue::ElementError);
    renderDocument(document);
  }

  // now pass
  {
    const char* body = "<template><validator value='1' base='3'/></template>";

    bool gotError = false;
    document.parse(body);
    renderDocument(document);
  }
}
