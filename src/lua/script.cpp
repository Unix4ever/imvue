/*
 *
Copyright (c) 2019 Artem Chernyshev

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "lua/script.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "imvue_errors.h"

#include <iostream>
#include <sstream>

extern "C" {
  #include <lua.h>
  #include <lualib.h>
  #include <lauxlib.h>
}

#include "imvue.h"
#include "imvue_element.h"
#include "imstring.h"
#include "rapidxml.hpp"
#include "rapidxml_print.hpp"

#if defined(LUA_VERSION_NUM) && LUA_VERSION_NUM >= 502
#define IMVUE_LUA_VERSION LUA_VERSION_NUM
#elif defined(LUA_VERSION_NUM) && LUA_VERSION_NUM == 501
#define IMVUE_LUA_VERSION LUA_VERSION_NUM
#elif !defined(LUA_VERSION_NUM)
#define IMVUE_LUA_VERSION 500
#else
#define IMVUE_LUA_VERSION 502
#endif // Lua Version 502, 501 || luajit, 500


namespace ImVue {

extern "C" {
#ifndef lua_absindex
#define lua_absindex(L, i)         ((i) > 0 || (i) <= LUA_REGISTRYINDEX ? (i) : \
    lua_gettop(L) + (i) + 1)
#endif

#if IMVUE_LUA_VERSION > 501
#define lua_setfenv(L, i) \
  const char* env = lua_setupvalue(L, i - 1, 1); \
  (void)env; \
  IM_ASSERT(env != NULL && ImStricmp(env, "_ENV") == 0 && "failed to set up function environment");
#endif

  inline size_t lua_gettablesize(lua_State* L, int index) {
#if IMVUE_LUA_VERSION < 502
    return lua_objlen(L, index);
#else
    return lua_rawlen(L, index);
#endif
  }
#if defined(IMVUE_LUA_VERSION) && IMVUE_LUA_VERSION <= 501
  // backport of luaL_setfuncs
  void luaL_setfuncs (lua_State *L, const luaL_Reg *l, int nup) {
    luaL_checkstack(L, nup+1, "too many upvalues");
    for (; l->name != NULL; l++) {  /* fill the table with given functions */
      int i;
      lua_pushstring(L, l->name);
      for (i = 0; i < nup; i++)  /* copy upvalues to the top */
        lua_pushvalue(L, -(nup + 1));
      lua_pushcclosure(L, l->func, nup);  /* closure with those upvalues */
      lua_settable(L, -(nup + 3)); /* table must be below the upvalues, the name and the closure */
    }
    lua_pop(L, nup);  /* remove upvalues */
  }
#endif
}

  /**
   * Keeps stack consistent by cleaning up top
   */
  struct StackGuard {

    StackGuard(lua_State* state) : L(state) {
      top = lua_gettop(L);
      refs = (int*)ImGui::MemAlloc(sizeof(int));
      (*refs) = 1;
    }

    StackGuard(const StackGuard& other) {
      (*this) = other;
    }

    StackGuard& operator=(const StackGuard& other) {
      top = other.top;
      L = other.L;
      refs = other.refs;
      (*refs)++;
      return *this;
    }

    ~StackGuard()
    {
      IM_ASSERT(refs != 0 && (*refs) > 0);
      if(!L || --(*refs) > 0) {
        return;
      }

      ImGui::MemFree(refs);
      refs = 0;
      int delta = lua_gettop(L) - top;
      if(delta > 0) {
        lua_pop(L, delta);
      }
    }

    lua_State* L;
    int top;
    int* refs;
  };

  Object createObject(lua_State* L);
  Object createObject(lua_State* L, int ref);

  /**
   * Abstract lua object
   */
  class LuaObject : public ObjectImpl {
    public:
      LuaObject(lua_State* L)
        : mLuaState(L)
      {
      }

      virtual ~LuaObject() {};

      virtual ObjectType type() const {
        return mType;
      }

      virtual Object get(const Object& key) {
        if(type() != ObjectType::OBJECT && type() != ObjectType::USERDATA && type() != ObjectType::ARRAY) {
          IMVUE_EXCEPTION(ScriptError, "failed to get key: not an object");
          return Object();
        }
        StackGuard g(mLuaState);
        unwrap();
        const ObjectImpl* impl = key.getImpl();
        if(!impl) {
          IMVUE_EXCEPTION(ScriptError, "null pointer access");
          return Object();
        }
        const LuaObject* obj = static_cast<const LuaObject*>(impl);
        obj->unwrap(mLuaState);
        lua_gettable(mLuaState, -2);
        return createObject(mLuaState);
      }

      virtual Object get(const char* key) {
        if(type() != ObjectType::OBJECT && type() != ObjectType::USERDATA && type() != ObjectType::ARRAY) {
          IMVUE_EXCEPTION(ScriptError, "failed to get key: not an object");
          return Object();
        }
        StackGuard g(mLuaState);
        unwrap();
        lua_pushstring(mLuaState, key);
        lua_gettable(mLuaState, -2);
        return createObject(mLuaState);
      }

      virtual bool call(Object* args, Object* rets, int nargs, int nrets) {
        StackGuard g(mLuaState);
        unwrap();
        if(args) {
          for(int i = 0; i < nargs; ++i) {
            const ObjectImpl* impl = args[i].getImpl();
            if(!impl) {
              lua_pushnil(mLuaState);
              continue;
            }
            const LuaObject* obj = static_cast<const LuaObject*>(impl);
            obj->unwrap(mLuaState);
          }
        }

        int top = lua_gettop(mLuaState) - 1;
        if(lua_pcall(mLuaState, nargs, nrets, 0)) {
          IMVUE_EXCEPTION(ScriptError, lua_tostring(mLuaState, -1));
          return false;
        }

        if(rets) {
          for(int i = 0; i < nrets; ++i) {
            lua_pushvalue(mLuaState, top + i);
            rets[i] = createObject(mLuaState);
          }
        }

        return true;
      }

      virtual void keys(ObjectKeys& res);

      virtual long readInt() {
        StackGuard g(mLuaState);
        unwrap();
        return lua_tointeger(mLuaState, -1);
      }

      virtual ImString readString() {
        StackGuard g(mLuaState);
        unwrap();
        ImString str(lua_tostring(mLuaState, -1));
        return str;
      }

      virtual double readDouble() {
        StackGuard g(mLuaState);
        unwrap();
        return lua_tonumber(mLuaState, -1);
      }

      virtual bool readBool() {
        StackGuard g(mLuaState);
        unwrap();
        return lua_toboolean(mLuaState, -1);
      }

      virtual void setObject(Object& object)
      {
        if(!object.valid()) {
          return;
        }

        static_cast<LuaObject*>(object.getImpl())->unwrap(mLuaState);
        assign();
      }

      virtual void setInteger(long value)
      {
        lua_pushinteger(mLuaState, value);
        assign();
      }

      virtual void setNumber(double value)
      {
        lua_pushnumber(mLuaState, value);
        assign();
      }

      void setString(const char* value)
      {
        lua_pushstring(mLuaState, value);
        assign();
      }

      void setBool(bool value)
      {
        lua_pushboolean(mLuaState, value);
        assign();
      }

      virtual void unwrap(lua_State* L = 0) const = 0;
      virtual void assign(lua_State* L = 0) = 0;
    protected:

      void initType()
      {
        StackGuard g(mLuaState);
        unwrap();
        int t = lua_type(mLuaState, -1);
        switch(t) {
          case LUA_TTABLE:
            if(lua_gettablesize(mLuaState, -1) != 0) {
              mType = ObjectType::ARRAY;
            } else {
              mType = ObjectType::OBJECT;
            }
            break;
          case LUA_TSTRING:
            mType = ObjectType::STRING;
            break;
          case LUA_TNUMBER:
            mType = ObjectType::NUMBER;
            break;
          case LUA_TBOOLEAN:
            mType = ObjectType::BOOLEAN;
            break;
          case LUA_TUSERDATA:
            mType = ObjectType::USERDATA;
            break;
          case LUA_TFUNCTION:
            mType = ObjectType::FUNCTION;
            break;
          default:
            mType = ObjectType::NIL;
            break;
        }
      }

      lua_State* mLuaState;

    private:

      ObjectType mType;
  };

  /**
   * Short lived stack reference object
   */
  class LuaStackReference : public LuaObject {
    public:
      LuaStackReference(lua_State* L, int ref, StackGuard guard)
        : LuaObject(L)
        , mIndex(lua_absindex(L, ref))
        , mStackGuard(guard)
      {
        initType();
      }

      virtual ~LuaStackReference() {
      }

      void unwrap(lua_State* L = 0) const
      {
        lua_pushvalue(L ? L : mLuaState, mIndex);
      }

      void assign(lua_State* L = 0)
      {
        lua_replace(L ? L : mLuaState, mIndex);
      }

    private:
      int mIndex;
      StackGuard mStackGuard;
  };

  /**
   * Registry reference
   */
  class LuaReference : public LuaObject {
    public:
      LuaReference(lua_State* L, int ref)
        : LuaObject(L)
        , mRef(ref)
      {
        initType();
      }

      virtual ~LuaReference()
      {
        if(mRef != LUA_NOREF)
          luaL_unref(mLuaState, LUA_REGISTRYINDEX, mRef);
      }

      void unwrap(lua_State* L = 0) const
      {
        IM_ASSERT(mRef != LUA_NOREF);
        lua_rawgeti(L ? L : mLuaState, LUA_REGISTRYINDEX, mRef);
      }

      void assign(lua_State* L = 0)
      {
        lua_rawseti(L ? L : mLuaState, LUA_REGISTRYINDEX, mRef);
      }
    private:
      int mRef;
  };

  /**
   * Table reference
   */
  class LuaFieldReference : public LuaObject {
    public:
      LuaFieldReference(lua_State* L, int ref, const char* key)
        : LuaObject(L)
        , mRef(ref)
      {
        lua_pushstring(L, key);
        mKeyReference = luaL_ref(L, LUA_REGISTRYINDEX);
      }

      LuaFieldReference(lua_State* L, int index)
        : LuaObject(L)
      {
        lua_pushinteger(L, index);
        mKeyReference = luaL_ref(L, LUA_REGISTRYINDEX);
      }

      virtual ~LuaFieldReference()
      {
        if(mKeyReference != LUA_NOREF)
          luaL_unref(mLuaState, LUA_REGISTRYINDEX, mKeyReference);
      }

      void unwrap(lua_State* L = 0) const
      {
        L = L ? L : mLuaState;
        lua_rawgeti(L, LUA_REGISTRYINDEX, mRef);
        lua_rawgeti(L, LUA_REGISTRYINDEX, mKeyReference);
        lua_gettable(L, -2);
      }

      void assign(lua_State* L = 0)
      {
        L = L ? L : mLuaState;
        int valueIndex = lua_gettop(L);
        lua_rawgeti(L, LUA_REGISTRYINDEX, mRef);
        lua_rawgeti(L, LUA_REGISTRYINDEX, mKeyReference);
        lua_pushvalue(L, valueIndex);
        lua_settable(L, -3);
      }
    private:
      int mKeyReference;
      int mRef;

  };

  Object createObject(lua_State* L) {
    StackGuard g(L);
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);
    return createObject(L, ref);
  }

  Object createObject(lua_State* L, int ref) {
    return Object(new LuaReference(L, ref));
  }


#define IMVUE "ImVue"
#define IMVUE_REFMAPPER "ImVueRefMapper"
#define IMVUE_CONTEXT "ImVueContext"
#define IMVUE_REACTIVE_TABLE "ImVueReactiveTable"

  static int lua_insertIndex(lua_State* L);
  static int lua_removeIndex(lua_State* L);

  class ReactiveTable {
    public:
      ReactiveTable(lua_State* L, int ref, LuaScriptState* scriptState, const char* field)
        : mRef(ref)
        , mScriptState(scriptState)
        , mField(ImStrdup(field))
        , mLuaState(L)
      {
      }

      ~ReactiveTable()
      {
        ImGui::MemFree(mField);
        luaL_unref(mLuaState, LUA_REGISTRYINDEX, mRef);
      }

      int newIndex(lua_State* L)
      {
        StackGuard g(L);
        unwrap(L);
        int top = lua_gettop(L);
        lua_pushvalue(L, 2);
        lua_pushvalue(L, 3);
        lua_settable(L, top);
        invalidate();
        return 0;
      }

      int index(lua_State* L)
      {
        unwrap(L);
        if(lua_type(L, -2) == LUA_TSTRING) {
          const char* field = lua_tostring(L, -2);
          if(ImStricmp(field, "remove") == 0) {
            lua_pushcfunction(L, lua_removeIndex);
            return 1;
          } else if(ImStricmp(field, "insert") == 0) {
            lua_pushcfunction(L, lua_insertIndex);
            return 1;
          }
        }
        lua_pushvalue(L, -2);
        lua_gettable(L, -2);
        mScriptState->requested(lua_tostring(L, 2));
        return 1;
      }

      void unwrap(lua_State* L)
      {
        IM_ASSERT(mRef != LUA_NOREF);
        lua_rawgeti(L, LUA_REGISTRYINDEX, mRef);
      }

      inline void invalidate()
      {
        mScriptState->pushChange(mField);
      }

    private:
      int mRef;
      LuaScriptState* mScriptState;
      char* mField;
      lua_State* mLuaState;
  };

  static ReactiveTable* lua_GetReactiveTable(lua_State* L, int index = -1) {
    return *reinterpret_cast<ReactiveTable**>(luaL_checkudata(L, index, IMVUE_REACTIVE_TABLE));
  }

  static int lua_ImVueReactiveTableIndex(lua_State* L)
  {
    return lua_GetReactiveTable(L, 1)->index(L);
  }

  static int lua_ImVueReactiveTableNewIndex(lua_State* L)
  {
    return lua_GetReactiveTable(L, 1)->newIndex(L);
  }

  static int lua_ImVueReactiveTableLen(lua_State* L)
  {
    lua_GetReactiveTable(L, 1)->unwrap(L);
    size_t len = lua_gettablesize(L, -1);
    lua_pushinteger(L, len);
    return 1;
  }

  static int lua_ImVueReactiveTableDelete(lua_State* L)
  {
    ReactiveTable* rt = lua_GetReactiveTable(L, 1);
    delete rt;
    return 0;
  }

  inline int loadFile(lua_State* L, int name)
  {
    // load data using system io library
    lua_getglobal(L, "io");
    lua_getfield(L, -1, "open");
    lua_replace(L, -2);
    lua_pushvalue(L, name);
    lua_pushstring(L, "rb");
    lua_call(L, 2, 1);
    int file = lua_gettop(L);
    // read file
    lua_getfield(L, file, "read");
    lua_pushvalue(L, file);
    lua_pushstring(L, "*a");
    lua_call(L, 2, 1);

    // close file
    lua_getfield(L, file, "close");
    lua_pushvalue(L, file);
    lua_call(L, 1, 0);
    return file;
  }

  static int lua_loadImv(lua_State* L)
  {
    int modpath = lua_upvalueindex(1);
#if IMVUE_LUA_VERSION < 502
    int modname = lua_gettop(L);
#else
    int modname = lua_gettop(L) - 1;
#endif
    loadFile(L, modpath);

    rapidxml::xml_document<> doc;
    ImString s(lua_tostring(L, -1));
    doc.parse<0>(s.get());

    lua_createtable(L, 0, 2);
    int tableIndex = lua_gettop(L);
    lua_pushstring(L, "tag");
    char* tag = ImStrdup(lua_tostring(L, modname));
    size_t len = strlen(tag);
    size_t offset = 0;
    for(size_t i = len - 1; i > 0; --i) {
      if(tag[i] == '.') {
        offset = i + 1;
        break;
      }
    }

    lua_pushstring(L, &tag[offset]);
    ImGui::MemFree(tag);
    lua_settable(L, tableIndex);

    rapidxml::xml_node<>* script = doc.first_node("script");
    if(!script) {
      return luaL_error(L, "malformed component definition: script tag is required");
    }

    // TODO: make all script tag parsers use the same code path
    rapidxml::xml_attribute<>* src = script->first_attribute("src");
    const char* scriptData = NULL;
    if(src) {
      lua_pushstring(L, src->value());
      scriptData = lua_tostring(L, loadFile(L, -1));
    } else {
      scriptData = script->value();
    }

    if(luaL_dostring(L, scriptData) != 0) {
      return luaL_error(L, lua_tostring(L, -1));
    }

    int componentDesc = lua_gettop(L);
    if(!lua_istable(L, componentDesc)) {
      return luaL_error(L, "malformed component definition: script must return a table");
    }

    rapidxml::xml_node<>* tmpl = doc.first_node("template");
    if(!tmpl) {
      return luaL_error(L, "malformed component definition: template tag is required");
    }

    std::string str;
    rapidxml::print(std::back_inserter(str), *tmpl, rapidxml::print_no_indenting);

    lua_pushstring(L, "template");
    lua_pushstring(L, str.c_str());
    lua_settable(L, componentDesc);

    lua_pushstring(L, "component");
    lua_pushvalue(L, componentDesc);
    lua_settable(L, tableIndex);

    lua_pushvalue(L, tableIndex);

    return 1;
  }

  static int lua_searchImv(lua_State* L)
  {
    lua_getglobal(L, "package");
    int package = lua_gettop(L);
    lua_getfield(L, package, "searchpath");
    lua_pushvalue(L, 1);
    lua_getfield(L, package, "imvpath");
    lua_call(L, 2, 2);

    if(lua_type(L, -2) != LUA_TSTRING) {
      lua_pushnil(L);
      lua_pushstring(L, lua_tostring(L, -1));
      return 2;
    }

    const char* modpath = lua_tostring(L, -2);
    lua_pushstring(L, modpath);
    lua_pushcclosure(L, lua_loadImv, 1);
    lua_pushstring(L, modpath);
    return 2;
  }

  static int lua_invalidateElement(lua_State* L)
  {
    void* e = NULL;
    if(lua_islightuserdata(L, lua_upvalueindex(1))) {
      e = lua_touserdata(L, lua_upvalueindex(1));
    }
    Element* element = (Element*) e;
    if(lua_type(L, -1) == LUA_TNUMBER) {
      element->invalidateFlags(lua_tointeger(L, -1));
    } else if(lua_type(L, -1) == LUA_TSTRING) {
      element->invalidate(lua_tostring(L, -1));
    }
    return 0;
  }

  static int lua_insertIndex(lua_State* L) {
    ReactiveTable* rt = lua_GetReactiveTable(L, -3);
    int valueIndex = lua_gettop(L);
    int keyIndex = valueIndex - 1;
    rt->unwrap(L);
    int tableIndex = lua_gettop(L);
    lua_getglobal(L, "table");
    lua_getfield(L, -1, "insert");
    lua_replace(L, -2); // remove _G.table
    lua_pushvalue(L, tableIndex);
    lua_pushvalue(L, keyIndex);
    lua_pushvalue(L, valueIndex);
    lua_call(L, 3, 0);
    rt->invalidate();
    return 0;
  }

  static int lua_removeIndex(lua_State* L) {
    int index = lua_tointeger(L, -1);
    ReactiveTable* rt = lua_GetReactiveTable(L, -2);
    rt->unwrap(L);
    int tableIndex = lua_gettop(L);
    lua_getglobal(L, "table");
    lua_getfield(L, -1, "remove");
    lua_replace(L, -2); // remove _G.table
    lua_pushvalue(L, tableIndex);
    lua_pushinteger(L, index);
    lua_call(L, 2, 1);
    rt->invalidate();
    return 1;
  }

  static int lua_elementInterface(lua_State* L)
  {
    StackGuard g(L);
    const char* id = lua_tostring(L, -1);
    if(!id) {
      return 0;
    }
    lua_pop(L, 1);

    void* refs = NULL;
    if(lua_islightuserdata(L, lua_upvalueindex(1))) {
      refs = lua_touserdata(L, lua_upvalueindex(1));
    }

    if(!refs) {
      return 0;
    }

    ScriptState::RefHash ref = lua_tointeger(L, lua_upvalueindex(2));
    ScriptState::RefMap& refMap = *(*reinterpret_cast<ScriptState::RefMap**>(refs));

    if(refMap.count(ref) == 0) {
      return 0;
    }
    Element* element = refMap[ref];
    if(ImStricmp(id, "invalidate") == 0) {
      lua_pushlightuserdata(L, element);
      lua_pushcclosure(L, lua_invalidateElement, 1);
      return 1;
    }

    Object object = Object(new LuaStackReference(L, -1, g));
    const Attribute* attribute = element->getAttribute(ImStricmp(id, "text") == 0 ? TEXT_ID : id);
    if(!attribute) {
      return 0;
    }

    return attribute->copy(element, object) ? 1 : 0;
  }

  inline void wrapTable(lua_State* L, int index, int keyIdx, LuaScriptState* scriptState)
  {
    if(lua_type(L, index) == LUA_TTABLE) {
      StackGuard g(L);
      lua_pushvalue(L, index);
      int ref = luaL_ref(L, LUA_REGISTRYINDEX);
      const char* field = lua_tostring(L, keyIdx);
      *reinterpret_cast<ReactiveTable**>(lua_newuserdata(L, sizeof(ReactiveTable*))) = new ReactiveTable(L, ref, scriptState, field);
      luaL_getmetatable(L, IMVUE_REACTIVE_TABLE);
      lua_setmetatable(L, -2);
      // replace table with userdata
      lua_insert(L, index);
    }
  }

  class RefMapper {
    public:
      RefMapper(ScriptState::RefMap* refs)
        : mRefs(refs)
      {
      }

      int index(lua_State* L) {
        const char* str = lua_tostring(L, 2);
        ScriptState::RefHash hash = ImHashStr(str);
        if(mRefs->count(hash) == 0) {
          return 0;
        }
        lua_pop(L, lua_gettop(L));
        lua_createtable(L, 0, 0);
        int tableIndex = lua_gettop(L);
        {
          StackGuard g(L);
          lua_createtable(L, 0, 1);
          int top = lua_gettop(L);
          lua_pushstring(L, "__index");
          lua_pushlightuserdata(L, &mRefs);
          lua_pushinteger(L, hash);
          lua_pushcclosure(L, lua_elementInterface, 2);
          lua_settable(L, top);
          lua_setmetatable(L, tableIndex);
        }
        return 1;
      }
    private:
      ScriptState::RefMap* mRefs;
  };

  /**
   * ImVue state class to be used from lua
   */
  class ImVue {

    public:
      ImVue(lua_State* L)
        : mLuaState(L)
        , mScriptState(NULL)
        , mRef(LUA_NOREF)
      {
        luaL_checktype(L, 1, LUA_TTABLE);
        mRef = luaL_ref(mLuaState, LUA_REGISTRYINDEX);
      }

      ~ImVue()
      {
        luaL_unref(mLuaState, LUA_REGISTRYINDEX, mRef);
      }

      // exposed to Lua

      int newIndex(lua_State* L)
      {
        if(mScriptState) {
          const char* field = lua_tostring(L, 2);
          mScriptState->pushChange(field);
          wrapTable(mLuaState, 3, 2, mScriptState);
        }

        lua_rawgeti(mLuaState, LUA_REGISTRYINDEX, mRef);
        lua_pushvalue(L, 2);
        lua_pushvalue(L, 3);
        lua_settable(mLuaState, -3);
        return 0;
      }

      int index(lua_State* L)
      {
        // refs interface access
        if(ImStricmp(lua_tostring(L, 2), "_refs") == 0) {
          lua_pushlightuserdata(L, &mRefMapper);
          luaL_getmetatable(L, IMVUE_REFMAPPER);
          lua_setmetatable(L, -2);
          return 1;
        }

        if(mScriptState) {
          mScriptState->requested(lua_tostring(L, 2));
        }

        lua_rawgeti(mLuaState, LUA_REGISTRYINDEX, mRef);
        lua_pushvalue(L, -2);
        lua_gettable(L, -2);
        return 1;
      }

      // internal methods

      void initEnvironment(int ref)
      {
        mSelfRef = ref;
      }

      void registerMixins(int tableIndex)
      {
        StackGuard g(mLuaState);
        IM_ASSERT(lua_istable(mLuaState, tableIndex));
        lua_pushstring(mLuaState, "mixins");
        lua_rawget(mLuaState, tableIndex);

        if(!lua_istable(mLuaState, -1)) {
          return;
        }

        int mixins = lua_gettop(mLuaState);

        lua_pushnil(mLuaState);
        while (lua_next(mLuaState, mixins) != 0) {
          int mixin = lua_gettop(mLuaState);
          lua_pushnil(mLuaState);
          while (lua_next(mLuaState, mixin) != 0) {
            if(lua_type(mLuaState, -2) == LUA_TSTRING && ImStricmp(lua_tostring(mLuaState, -2), "data") == 0) {
              registerData(tableIndex, mixin);
            } else {
              lua_pushvalue(mLuaState, -2);
              lua_insert(mLuaState, -2);
              lua_settable(mLuaState, tableIndex);
            }
            lua_pop(mLuaState, 1);
          }
          lua_pop(mLuaState, 1);
        }
      }

      void registerData(int tableIndex, int src = -1)
      {
        StackGuard g(mLuaState);
        IM_ASSERT(lua_istable(mLuaState, tableIndex));
        if(callFunction("data", false, src)) {
          int pos = lua_gettop(mLuaState);
          if(lua_type(mLuaState, -1) != LUA_TTABLE) {
            IMVUE_EXCEPTION(ScriptError, "data() method must return a table, got %d", lua_type(mLuaState, -1));
            return;
          }

          lua_pushnil(mLuaState);
          while (lua_next(mLuaState, pos) != 0) {

            int keyIdx = lua_gettop(mLuaState) - 1;

            // add metatable that will trigger redraw if table changes
            wrapTable(mLuaState, lua_gettop(mLuaState), keyIdx, mScriptState);

            lua_pushvalue(mLuaState, -2);
            lua_insert(mLuaState, -2);
            lua_settable(mLuaState, tableIndex);
          }
        }
      }

      bool callFunction(const char* functionName, bool resetState = true, int tableIndex = -1)
      {
        StackGuard* g = 0;
        if(resetState)
          g = new StackGuard(mLuaState);

        if(tableIndex == -1) {
          lua_rawgeti(mLuaState, LUA_REGISTRYINDEX, mRef);
          tableIndex = lua_gettop(mLuaState);
        }
        lua_pushstring(mLuaState, functionName);
        lua_gettable(mLuaState, tableIndex);
        bool result = false;
        if(lua_type(mLuaState, -1) == LUA_TFUNCTION) {
          lua_rawgeti(mLuaState, LUA_REGISTRYINDEX, mSelfRef);
          lua_pcall(mLuaState, 1, 1, 0);
          result = true;
        }
        if(g) {
          delete g;
        }
        return result;
      }

      inline void injectScriptState(LuaScriptState* state)
      {
        mScriptState = state;
      }

      inline void injectRefMapper(RefMapper* mapper)
      {
        mRefMapper = mapper;
      }

      inline ScriptState* getState()
      {
        return mScriptState;
      }

      inline void unwrap()
      {
        lua_rawgeti(mLuaState, LUA_REGISTRYINDEX, mRef);
      }

    private:
      lua_State* mLuaState;
      LuaScriptState* mScriptState;
      int mRef;
      int mSelfRef;
      RefMapper* mRefMapper;
  };

  static ImVue* lua_GetImVue(lua_State* L, int index = -1) {
    return *reinterpret_cast<ImVue**>(luaL_checkudata(L, index, IMVUE));
  }

  static int lua_CreateImVue(lua_State* L) {
    *reinterpret_cast<ImVue**>(lua_newuserdata(L, sizeof(ImVue*))) = new ImVue(L);
    luaL_getmetatable(L, IMVUE);
    lua_setmetatable(L, -2);
    return 1;
  }

  static int lua_ImVueCreateComponent(lua_State* L) {
    if(!lua_istable(L, -1)) {
      return luaL_error(L, "expected table as the last argument");
    }

    int componentDescriptionIndex = lua_gettop(L);
    int nameIndex = componentDescriptionIndex - 1;
    lua_createtable(L, 0, 2);
    int tableIndex = lua_gettop(L);
    if(lua_type(L, nameIndex) == LUA_TSTRING) {
      lua_pushstring(L, "tag");
      lua_pushvalue(L, nameIndex);
      lua_settable(L, tableIndex);
    }

    lua_pushstring(L, "component");
    lua_pushvalue(L, componentDescriptionIndex);
    lua_settable(L, tableIndex);

    return 1;
  }

  static int lua_DeleteImVue(lua_State* L) {
    delete *reinterpret_cast<ImVue**>(lua_touserdata(L, 1));
    return 0;
  }

  static int lua_ImVueIndex(lua_State* L) {
    return lua_GetImVue(L, 1)->index(L);
  }

  static int lua_ImVueNewIndex(lua_State* L) {
    return lua_GetImVue(L, 1)->newIndex(L);
  }

  static int lua_ImVueMapperIndex(lua_State* L) {
    RefMapper* mapper = *reinterpret_cast<RefMapper**>(luaL_checkudata(L, 1, IMVUE_REFMAPPER));
    return mapper->index(L);
  }

  static int lua_ImVueContextIndex(lua_State* L) {
    int keyIdx = lua_gettop(L);
    int tableIdx = keyIdx - 1;
    lua_pushvalue(L, -1);
    lua_rawget(L, tableIdx);

    // fallback to global table
    if(lua_type(L, -1) == LUA_TNIL) {
      lua_getglobal(L, "_G");
      lua_pushvalue(L, keyIdx);
      lua_gettable(L, -2);
    }

    return 1;
  }

  LuaScriptState::LuaScriptState(lua_State* L)
    : mLuaState(L)
    , mRefMapper(new RefMapper(&mRefMap))
    , mImVue(NULL)
    , mRef(LUA_NOREF)
    , mLogAccess(false)
  {
  }

  LuaScriptState::~LuaScriptState()
  {
    delete mRefMapper;
    if(mRef != LUA_NOREF) {
      luaL_unref(mLuaState, LUA_REGISTRYINDEX, mRef);
    }
  }

  void LuaScriptState::initialize(Object data)
  {
    static_cast<LuaObject*>(data.getImpl())->unwrap(mLuaState);
    int index = lua_gettop(mLuaState);
    // copy table
    lua_createtable(mLuaState, 0, 0);
    int copyIndex = lua_gettop(mLuaState);
    lua_pushnil(mLuaState);

    while(lua_next(mLuaState, index) != 0)
    {
      lua_pushvalue(mLuaState, -2);
      lua_insert(mLuaState, -2);
      lua_settable(mLuaState, copyIndex);
    }

    lua_CreateImVue(mLuaState);
    luaL_checkudata(mLuaState, copyIndex, IMVUE);
    setupState(luaL_ref(mLuaState, LUA_REGISTRYINDEX));
  }

  void LuaScriptState::initialize(const char* scriptData)
  {
    StackGuard g(mLuaState);
    if(luaL_dostring(mLuaState, scriptData) != 0) {
      IMVUE_EXCEPTION(ScriptError, lua_tostring(mLuaState, -1));
      return;
    }

    luaL_checkudata(mLuaState, -1, IMVUE);
    setupState(luaL_ref(mLuaState, LUA_REGISTRYINDEX));
  }

  ScriptState* LuaScriptState::clone() const
  {
    return new LuaScriptState(mLuaState);
  }

  MutableObject LuaScriptState::operator[](const char* key) {
    return MutableObject(new LuaFieldReference(mLuaState, mRef, key));
  }

  bool LuaScriptState::runScript(const char* script, ScriptState::Context* ctx)
  {
    int err = luaL_loadstring(mLuaState, script);
    if(err != 0) {
      if(err == LUA_ERRSYNTAX)
        handleError(lua_tostring(mLuaState, -1));
      else if(err == LUA_ERRMEM)
        handleError("memory allocation error");
      else
        handleError("GC error");

      return false;
    }

    activateContext(ctx);
    if(lua_pcall(mLuaState, 0, LUA_MULTRET, 0)) {
      handleError(lua_tostring(mLuaState, -1));
      return false;
    }

    return true;
  }

  void LuaScriptState::eval(const char* str, std::string* retval, Fields* fields, ScriptState::Context* ctx)
  {
    if(!mImVue) {
      return;
    }

    StackGuard g(mLuaState);
    std::stringstream script;
    std::string data(str);
    if(retval && data.find("return") == std::string::npos) {
      script << "return ";
    }

    script << str;

    if(fields) {
      mLogAccess = true;
    }

    int functionIndex = lua_gettop(mLuaState);
    bool success = runScript(&script.str()[0], ctx);
    mLogAccess = false;

    if(!success) {
      return;
    }

    if(lua_gettop(mLuaState) - functionIndex == 0 && retval) {
      handleError("script eval did not return any values");
      mAccessLog.clear();
      return;
    }

    // read return value
    if(retval) {
      if(lua_type(mLuaState, -1) == LUA_TBOOLEAN) {
        *retval = lua_toboolean(mLuaState, -1) ? "1" : "0";
      } else {
        const char* c = lua_tostring(mLuaState, -1);
        if(c) {
          *retval = std::string(c);
        } else {
          std::stringstream ss;
          ss << "Failed to convert value to string, lua type: " << lua_type(mLuaState, -1) << ", script: " << str;
          IMVUE_EXCEPTION(ScriptError, ss.str().c_str());
        }
      }
    }

    // return access log
    if(fields && mAccessLog.size() > 0) {
      int offset = fields->size();
      fields->resize(offset + mAccessLog.size());
      memcpy(&fields->Data[offset], &mAccessLog[0], sizeof(FieldHash) * mAccessLog.size());
      mAccessLog.clear();
    }
  }

  Object LuaScriptState::getObject(const char* str, Fields* fields, ScriptState::Context* ctx)
  {
    StackGuard g(mLuaState);
    if(str[0] == '\0' || !mImVue) {
      return Object();
    }

    std::stringstream script;
    std::string data(str);
    if(data.find("return") == std::string::npos) {
      script << "return ";
    }

    script << str;

    if(fields) {
      mLogAccess = true;
    }

    bool success = runScript(&script.str()[0], ctx);
    mLogAccess = false;

    if(!success) {
      return Object();
    }

    // return access log
    if(fields && mAccessLog.size() > 0) {
      int offset = fields->size();
      fields->resize(offset + mAccessLog.size());
      memcpy(&fields->Data[offset], &mAccessLog[0], sizeof(FieldHash) * mAccessLog.size());
      mAccessLog.clear();
    }

    return createObject(mLuaState);
  }

  void LuaScriptState::lifecycleCallback(ScriptState::LifecycleCallbackType cb)
  {
    if(!mImVue) {
      return;
    }

    switch(cb) {
      case ScriptState::BEFORE_CREATE:
        mImVue->callFunction("beforeCreate");
        break;
      case ScriptState::CREATED:
        mImVue->callFunction("created");
        break;
      case ScriptState::BEFORE_MOUNT:
        mImVue->callFunction("beforeMount");
        break;
      case ScriptState::MOUNTED:
        mImVue->callFunction("mounted");
        break;
      case ScriptState::BEFORE_UPDATE:
        mImVue->callFunction("beforeUpdate");
        break;
      case ScriptState::UPDATED:
        mImVue->callFunction("updated");
        break;
      case ScriptState::BEFORE_DESTROY:
        mImVue->callFunction("beforeDestroy");
        break;
      case ScriptState::DESTROYED:
        mImVue->callFunction("destroyed");
        break;
    }
  }

  void LuaScriptState::requested(ScriptState::FieldHash h)
  {
    if(mLogAccess) {
      mAccessLog.push_back(h);
    }
  }

  void LuaScriptState::requested(const char* field)
  {
    requested(hash(field));
  }

  void LuaScriptState::setupState(int ref)
  {
    if(mImVue) {
      luaL_unref(mLuaState, LUA_REGISTRYINDEX, mRef);
      mRef = LUA_NOREF;
      mImVue = 0;
    }

    lua_rawgeti(mLuaState, LUA_REGISTRYINDEX, ref);
    ImVue* imvue = lua_GetImVue(mLuaState);

    imvue->injectScriptState(this);
    imvue->injectRefMapper(mRefMapper);

    imvue->unwrap();
    int tableIndex = lua_gettop(mLuaState);
    imvue->initEnvironment(ref);
    imvue->registerMixins(tableIndex);
    imvue->registerData(tableIndex);
    mRef = ref;
    mImVue = imvue;
  }

  void LuaScriptState::setObject(char* key, Object& value, int tableIndex)
  {
    LuaObject* v = static_cast<LuaObject*>(value.getImpl());
    if(!v) {
      IMVUE_EXCEPTION(ScriptError, "tried to set null value in context");
      return;
    }

    if(tableIndex >= 0) {
      lua_pushstring(mLuaState, key);
    }

    v->unwrap();
    if(tableIndex < 0) {
      lua_setglobal(mLuaState, key);
    } else {
      lua_settable(mLuaState, tableIndex);
    }
  }

  void LuaScriptState::activateContext(ScriptState::Context* ctx)
  {
    StackGuard g(mLuaState);
    lua_getupvalue(mLuaState, -1, 1);
    int func = lua_gettop(mLuaState);
    lua_createtable(mLuaState, 0, 0);
    luaL_getmetatable(mLuaState, IMVUE_CONTEXT);
    lua_setmetatable(mLuaState, -2);

    int index = lua_gettop(mLuaState);
    if(ctx) {
      for(size_t i = 0; i < ctx->vars.size(); ++i) {
        ScriptState::Variable& var = ctx->vars[i];
        setObject(var.key, var.value, index);
        requested(ctx->hash);
      }
    }

    lua_pushstring(mLuaState, "self");
    lua_rawgeti(mLuaState, LUA_REGISTRYINDEX, mRef);
    lua_settable(mLuaState, index);

    lua_pushvalue(mLuaState, index);
    lua_setfenv(mLuaState, func);
  }

  void registerBindings(lua_State* L)
  {
    lua_pushnumber(L, ImGuiWindowFlags_NoTitleBar);
    lua_setglobal(L, "ImGuiWindowFlags_NoTitleBar");
    lua_pushnumber(L, ImGuiWindowFlags_NoResize);
    lua_setglobal(L, "ImGuiWindowFlags_NoResize");
    lua_pushnumber(L, ImGuiWindowFlags_NoMove);
    lua_setglobal(L, "ImGuiWindowFlags_NoMove");
    lua_pushnumber(L, ImGuiWindowFlags_NoScrollbar);
    lua_setglobal(L, "ImGuiWindowFlags_NoScrollbar");
    lua_pushnumber(L, ImGuiWindowFlags_NoScrollWithMouse);
    lua_setglobal(L, "ImGuiWindowFlags_NoScrollWithMouse");
    lua_pushnumber(L, ImGuiWindowFlags_NoCollapse);
    lua_setglobal(L, "ImGuiWindowFlags_NoCollapse");
    lua_pushnumber(L, ImGuiWindowFlags_AlwaysAutoResize);
    lua_setglobal(L, "ImGuiWindowFlags_AlwaysAutoResize");
    lua_pushnumber(L, ImGuiWindowFlags_NoSavedSettings);
    lua_setglobal(L, "ImGuiWindowFlags_NoSavedSettings");
    lua_pushnumber(L, ImGuiWindowFlags_NoInputs);
    lua_setglobal(L, "ImGuiWindowFlags_NoInputs");
    lua_pushnumber(L, ImGuiWindowFlags_MenuBar);
    lua_setglobal(L, "ImGuiWindowFlags_MenuBar");
    lua_pushnumber(L, ImGuiWindowFlags_HorizontalScrollbar);
    lua_setglobal(L, "ImGuiWindowFlags_HorizontalScrollbar");
    lua_pushnumber(L, ImGuiWindowFlags_NoFocusOnAppearing);
    lua_setglobal(L, "ImGuiWindowFlags_NoFocusOnAppearing");
    lua_pushnumber(L, ImGuiWindowFlags_NoBringToFrontOnFocus);
    lua_setglobal(L, "ImGuiWindowFlags_NoBringToFrontOnFocus");
    lua_pushnumber(L, ImGuiWindowFlags_ChildWindow);
    lua_setglobal(L, "ImGuiWindowFlags_ChildWindow");
    lua_pushnumber(L, ImGuiWindowFlags_Tooltip);
    lua_setglobal(L, "ImGuiWindowFlags_Tooltip");
    lua_pushnumber(L, ImGuiWindowFlags_Popup);
    lua_setglobal(L, "ImGuiWindowFlags_Popup");
    lua_pushnumber(L, ImGuiWindowFlags_Modal);
    lua_setglobal(L, "ImGuiWindowFlags_Modal");
    lua_pushnumber(L, ImGuiWindowFlags_ChildMenu);
    lua_setglobal(L, "ImGuiWindowFlags_ChildMenu");
    lua_pushnumber(L, ImGuiWindowFlags_AlwaysUseWindowPadding);
    lua_setglobal(L, "ImGuiWindowFlags_AlwaysUseWindowPadding");
    lua_pushnumber(L, ImGuiInputTextFlags_CharsDecimal);
    lua_setglobal(L, "ImGuiInputTextFlags_CharsDecimal");
    lua_pushnumber(L, ImGuiInputTextFlags_CharsHexadecimal);
    lua_setglobal(L, "ImGuiInputTextFlags_CharsHexadecimal");
    lua_pushnumber(L, ImGuiInputTextFlags_CharsUppercase);
    lua_setglobal(L, "ImGuiInputTextFlags_CharsUppercase");
    lua_pushnumber(L, ImGuiInputTextFlags_CharsNoBlank);
    lua_setglobal(L, "ImGuiInputTextFlags_CharsNoBlank");
    lua_pushnumber(L, ImGuiInputTextFlags_AutoSelectAll);
    lua_setglobal(L, "ImGuiInputTextFlags_AutoSelectAll");
    lua_pushnumber(L, ImGuiInputTextFlags_EnterReturnsTrue);
    lua_setglobal(L, "ImGuiInputTextFlags_EnterReturnsTrue");
    lua_pushnumber(L, ImGuiInputTextFlags_CallbackCompletion);
    lua_setglobal(L, "ImGuiInputTextFlags_CallbackCompletion");
    lua_pushnumber(L, ImGuiInputTextFlags_CallbackHistory);
    lua_setglobal(L, "ImGuiInputTextFlags_CallbackHistory");
    lua_pushnumber(L, ImGuiInputTextFlags_CallbackAlways);
    lua_setglobal(L, "ImGuiInputTextFlags_CallbackAlways");
    lua_pushnumber(L, ImGuiInputTextFlags_CallbackCharFilter);
    lua_setglobal(L, "ImGuiInputTextFlags_CallbackCharFilter");
    lua_pushnumber(L, ImGuiInputTextFlags_AllowTabInput);
    lua_setglobal(L, "ImGuiInputTextFlags_AllowTabInput");
    lua_pushnumber(L, ImGuiInputTextFlags_CtrlEnterForNewLine);
    lua_setglobal(L, "ImGuiInputTextFlags_CtrlEnterForNewLine");
    lua_pushnumber(L, ImGuiInputTextFlags_NoHorizontalScroll);
    lua_setglobal(L, "ImGuiInputTextFlags_NoHorizontalScroll");
    lua_pushnumber(L, ImGuiInputTextFlags_AlwaysInsertMode);
    lua_setglobal(L, "ImGuiInputTextFlags_AlwaysInsertMode");
    lua_pushnumber(L, ImGuiInputTextFlags_ReadOnly);
    lua_setglobal(L, "ImGuiInputTextFlags_ReadOnly");
    lua_pushnumber(L, ImGuiInputTextFlags_Password);
    lua_setglobal(L, "ImGuiInputTextFlags_Password");
    lua_pushnumber(L, ImGuiInputTextFlags_Multiline);
    lua_setglobal(L, "ImGuiInputTextFlags_Multiline");
    lua_pushnumber(L, ImGuiSelectableFlags_DontClosePopups);
    lua_setglobal(L, "ImGuiSelectableFlags_DontClosePopups");
    lua_pushnumber(L, ImGuiSelectableFlags_SpanAllColumns);
    lua_setglobal(L, "ImGuiSelectableFlags_SpanAllColumns");
    lua_pushnumber(L, ImGuiStyleVar_FramePadding);
    lua_setglobal(L, "ImGuiStyleVar_FramePadding");
    lua_pushnumber(L, ImGuiStyleVar_FrameRounding);
    lua_setglobal(L, "ImGuiStyleVar_FrameRounding");
    lua_pushnumber(L, ImGuiStyleVar_ItemSpacing);
    lua_setglobal(L, "ImGuiStyleVar_ItemSpacing");
    lua_pushnumber(L, ImGuiStyleVar_WindowPadding);
    lua_setglobal(L, "ImGuiStyleVar_WindowPadding");
    lua_pushnumber(L, ImGuiStyleVar_WindowRounding);
    lua_setglobal(L, "ImGuiStyleVar_WindowRounding");
    lua_pushnumber(L, ImGuiKey_Tab);
    lua_setglobal(L, "ImGuiKey_Tab");
    lua_pushnumber(L, ImGuiKey_LeftArrow);
    lua_setglobal(L, "ImGuiKey_LeftArrow");
    lua_pushnumber(L, ImGuiKey_RightArrow);
    lua_setglobal(L, "ImGuiKey_RightArrow");
    lua_pushnumber(L, ImGuiKey_UpArrow);
    lua_setglobal(L, "ImGuiKey_UpArrow");
    lua_pushnumber(L, ImGuiKey_DownArrow);
    lua_setglobal(L, "ImGuiKey_DownArrow");
    lua_pushnumber(L, ImGuiKey_PageUp);
    lua_setglobal(L, "ImGuiKey_PageUp");
    lua_pushnumber(L, ImGuiKey_PageDown);
    lua_setglobal(L, "ImGuiKey_PageDown");
    lua_pushnumber(L, ImGuiKey_Home);
    lua_setglobal(L, "ImGuiKey_Home");
    lua_pushnumber(L, ImGuiCol_Text);
    lua_setglobal(L, "ImGuiCol_Text");
    lua_pushnumber(L, ImGuiCol_FrameBg);
    lua_setglobal(L, "ImGuiCol_FrameBg");
    lua_pushnumber(L, ImGuiCol_WindowBg);
    lua_setglobal(L, "ImGuiCol_WindowBg");
    lua_pushnumber(L, ImGuiCol_ChildWindowBg);
    lua_setglobal(L, "ImGuiCol_ChildWindowBg");
    lua_pushnumber(L, ImGuiCol_Border);
    lua_setglobal(L, "ImGuiCol_Border");
    lua_pushnumber(L, ImGuiCol_BorderShadow);
    lua_setglobal(L, "ImGuiCol_BorderShadow");
    lua_pushnumber(L, ImGuiCol_Separator);
    lua_setglobal(L, "ImGuiCol_Separator");

    lua_pushnumber(L, ImGuiTreeNodeFlags_Leaf);
    lua_setglobal(L, "ImGuiTreeNodeFlags_Leaf");

    lua_pushnumber(L, ObjectType::ARRAY);
    lua_setglobal(L, "ImVue_Array");
    lua_pushnumber(L, ObjectType::OBJECT);
    lua_setglobal(L, "ImVue_Object");
    lua_pushnumber(L, ObjectType::NUMBER);
    lua_setglobal(L, "ImVue_Number");
    lua_pushnumber(L, ObjectType::STRING);
    lua_setglobal(L, "ImVue_String");
    lua_pushnumber(L, ObjectType::BOOLEAN);
    lua_setglobal(L, "ImVue_Boolean");
    lua_pushnumber(L, ObjectType::NIL);
    lua_setglobal(L, "ImVue_Nil");
    lua_pushnumber(L, ObjectType::USERDATA);
    lua_setglobal(L, "ImVue_Userdata");
    lua_pushnumber(L, ObjectType::FUNCTION);
    lua_setglobal(L, "ImVue_Function");

    if(luaL_newmetatable(L, IMVUE)) {
      // register main userdata object
      static const luaL_Reg imvueFuncs[] = {
        {"new", lua_CreateImVue},
        {"component", lua_ImVueCreateComponent},
        {"__newindex", lua_ImVueNewIndex},
        {"__index", lua_ImVueIndex},
        {"__gc", lua_DeleteImVue},
        {NULL, NULL}
      };
      luaL_setfuncs(L, imvueFuncs, 0);
      lua_setglobal(L, IMVUE);
    }

    if(luaL_newmetatable(L, IMVUE_REFMAPPER)) {
      static const luaL_Reg mapperFuncs[] = {
        {"__index", lua_ImVueMapperIndex},
        {NULL, NULL}
      };
      luaL_setfuncs(L, mapperFuncs, 0);
      lua_setglobal(L, IMVUE_REFMAPPER);
    }

    if(luaL_newmetatable(L, IMVUE_CONTEXT)) {
      static const luaL_Reg contextFuncs[] = {
        {"__index", lua_ImVueContextIndex},
        {NULL, NULL}
      };
      luaL_setfuncs(L, contextFuncs, 0);
      lua_setglobal(L, IMVUE_CONTEXT);
    }

    if(luaL_newmetatable(L, IMVUE_REACTIVE_TABLE)) {
      static const luaL_Reg rtFuncs[] = {
        {"__index", lua_ImVueReactiveTableIndex},
        {"__newindex", lua_ImVueReactiveTableNewIndex},
        {"__len", lua_ImVueReactiveTableLen},
        {"__gc", lua_ImVueReactiveTableDelete},
        {NULL, NULL}
      };
      luaL_setfuncs(L, rtFuncs, 0);
      lua_setglobal(L, IMVUE_REACTIVE_TABLE);
    }

#if IMVUE_LUA_VERSION > 501
    const char* id = "searchers";
#else
    const char* id = "loaders";
    // backport seachpath
    const char* searchpath =
"if not package.searchpath then\n"
"local sep = package.config:sub(1,1)\n"
"function package.searchpath (mod,path)\n"
"mod = mod:gsub('%.',sep)\n"
"for m in path:gmatch('[^;]+') do\n"
"local nm = m:gsub('?',mod)\n"
"local f = io.open(nm,'r')\n"
"if f then f:close(); return nm end\n"
"end\n"
"end\n"
"end";

    luaL_dostring(L, searchpath);
#endif

    // add imv file loader
    lua_getglobal(L, "package");
    int package = lua_gettop(L);
    lua_pushstring(L, id);
    int searchers = lua_gettop(L);
    lua_gettable(L, -2);
    lua_getglobal(L, "table");
    lua_getfield(L, -1, "insert");
    lua_replace(L, -2); // remove _G.table
    lua_pushvalue(L, searchers);
    lua_pushinteger(L, 4);
    lua_pushcclosure(L, lua_searchImv, 0);
    lua_call(L, 3, 0);

    // setup default search path for imvue templates
    lua_pushstring(L, "imvpath");
    lua_gettable(L, package);

    if(lua_type(L, -1) != LUA_TSTRING) {
      lua_pushstring(L, "imvpath");
      lua_pushstring(L, "./?.imv;");
      lua_settable(L, package);
    }
  }

  void LuaObject::keys(ObjectKeys& res) {
    StackGuard g(mLuaState);
    unwrap();
    switch(type()) {
      case ObjectType::USERDATA:
        lua_GetReactiveTable(mLuaState)->unwrap(mLuaState);
        break;
      case ObjectType::ARRAY:
      case ObjectType::OBJECT:
        break;
      default:
        return;
    }

    int index = lua_gettop(mLuaState);
    lua_pushnil(mLuaState);
    ImVector<int> refs;

    while(lua_next(mLuaState, index) != 0)
    {
      lua_pop(mLuaState, 1);
      lua_pushvalue(mLuaState, -1);
      int ref = luaL_ref(mLuaState, LUA_REGISTRYINDEX);
      refs.push_back(ref);
    }

    res.resize(refs.size());

    for(int i = 0; i < refs.size(); i++) {
      res[i] = createObject(mLuaState, refs[i]);
    }
  }
} // namespace ImVue
