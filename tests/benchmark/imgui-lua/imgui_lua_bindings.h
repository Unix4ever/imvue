#ifndef __IMGUI_LUA_BINDINGS__
#define __IMGUI_LUA_BINDINGS__

extern "C" {
  #include "lua.h"
  #include "lualib.h"
  #include "lauxlib.h"
}

void LoadImguiBindings(lua_State* L);

#endif
