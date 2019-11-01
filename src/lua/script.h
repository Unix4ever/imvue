/*
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

#ifndef __IMVUE_LUA_SCRIPT_H__
#define __IMVUE_LUA_SCRIPT_H__

#include "imvue_script.h"
#include <vector>
#include <string>

struct lua_State;
struct luaL_Reg;

namespace ImVue {
  class ImVue;
  class Element;
  class RefMapper;

  class LuaScriptState : public ScriptState
  {
    public:
      LuaScriptState(lua_State* L);
      virtual ~LuaScriptState();

      /**
       * Runs lua script. Script must return Lua table
       *
       * @param scriptData script string
       */
      void initialize(const char* scriptData);

      /**
       * Creates new script state object from Lua table
       *
       * @param state Lua table
       */
      void initialize(Object state);

      /**
       * Clones script state
       *
       * Creates a new script non-initialized script state which is using the same lua_State
       */
      ScriptState* clone() const;

      /**
       * Get or create object from state object
       */
      MutableObject operator[](const char* key);

      /**
       * Evaluate expression
       *
       * @param str script data
       * @param retval evaluated script converted to string
       */
      void eval(const char* str, std::string* retval = 0, Fields* fields = 0, ScriptState::Context* ctx = 0);

      Object getObject(const char* str, Fields* fields = 0, ScriptState::Context* ctx = 0);

      /**
       * Removes all field listeners
       */
      void removeListeners();

      /**
       * Handle lifecycle callbacks
       */
      void lifecycleCallback(LifecycleCallbackType cb);

      void requested(ScriptState::FieldHash h);

      void requested(const char* field);

    private:
      friend class ImVue;

      void setupState(int ref);

      bool runScript(const char* script, ScriptState::Context* ctx);

      typedef ImVector<ScriptState::FieldHash> FieldAccessLog;

      void setObject(char* key, Object& value, int tableIndex = -1);

      void activateContext(ScriptState::Context* ctx);

      lua_State* mLuaState;
      FieldAccessLog mAccessLog;
      RefMapper* mRefMapper;
      ImVue* mImVue;
      int mRef;
      int mFuncUpvalue;

      bool mLogAccess;
  };

  /**
   * Registers ImVue lua bindings
   */
  void registerBindings(lua_State* L);
}

#endif
