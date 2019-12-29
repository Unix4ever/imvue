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

#include "imvue_context.h"
#include "imvue_generated.h"
#include "imvue_script.h"
#include "imvue_style.h"
#include "extras/xhtml.h"
#include "extras/svg.h"

#include <iostream>
#include <fstream>

namespace ImVue {

  char* SimpleFileSystem::load(const char* path, int* size, Mode mode)
  {
    std::ifstream stream(path, mode == TEXT ? std::ifstream::in : std::ifstream::binary);
    char* data = NULL;
    if(size) {
      *size = 0;
    }

    try
    {
      stream.seekg(0, std::ios::end);
      int length = stream.tellg();
      stream.seekg(0, std::ios::beg);

      if(length < 0) {
        return data;
      }
      data = (char*)ImGui::MemAlloc(length + 1);
      if(mode == Mode::TEXT) {
        data[length] = '\0';
      }
      stream.read(data, length);
      if(size) {
        *size = length;
      }
    }
    catch(std::exception& e)
    {
      (void)e;
      return NULL;
    }

    stream.close();
    return data;
  }

  void SimpleFileSystem::loadAsync(const char* path, LoadCallback* callback)
  {
    // not really async
    char* data = load(path);
    callback(data);
    ImGui::MemFree(data);
  }

  FontManager::FontManager()
  {

  }

  ImFont* FontManager::loadFontFromFile(const char* name, const char* path, ImVector<ImWchar> glyphRanges)
  {
    ImU32 id = ImHashStr(name);
    if(mFonts.find(id) != mFonts.end()) {
      return mFonts[id].font;
    }

    int size = 0;
    char* data = fs->load(path, &size, FileSystem::BINARY);
    if(!data || size == 0) {
      return NULL;
    }

    ImGuiIO& io = ImGui::GetIO();
    ImFontConfig fontCfg;
    FontHandle handle;
    memset(&handle, 0, sizeof(FontHandle));
    strcpy(&fontCfg.Name[0], name);
    handle.glyphRanges = glyphRanges;
    handle.size = 25.0f;
    mFonts[id] = handle;
    if(glyphRanges.size() != 0) {
      fontCfg.GlyphRanges = mFonts[id].glyphRanges.Data;
    }

    ImFont* font = io.Fonts->AddFontFromMemoryTTF(data, size, handle.size, &fontCfg);
    if(font) {
      mFonts[id].font = font;
    } else {
      mFonts.erase(id);
    }

    return font;
  }

  bool FontManager::pushFont(const char* name)
  {
    ImU32 id = ImHashStr(name);
    if(mFonts.find(id) != mFonts.end()) {
      ImGui::PushFont(mFonts[id].font);
      return true;
    }

    return false;
  }

  Context::~Context()
  {
    if(!parent) {
      if(factory) {
        delete factory;
      }

      if(texture) {
        delete texture;
      }

      if(fs) {
        delete fs;
      }

      if(fontManager) {
        delete fontManager;
      }
    }

    if(script) {
      delete script;
    }

    if(style) {
      delete style;
    }
  }

  Context* createContext(ElementFactory* factory, ScriptState* script, TextureManager* texture, FileSystem* fs, FontManager* fontManager, Style* s, void* userdata)
  {
    Context* ctx = new Context();
    memset(ctx, 0, sizeof(Context));
    if(factory == 0) {
      ctx->factory = createElementFactory();
    }
    ctx->factory = factory;
    registerHtmlExtras(*ctx->factory);
    ctx->texture = texture;
    if(!fs) {
      fs = new SimpleFileSystem();
    }
    ctx->fs = fs;
    ctx->script = script;
    ctx->userdata = userdata;
    ctx->style = s ? s : new Style();
    ctx->fontManager = fontManager;
    ctx->scale = ImVec2(1.0f, 1.0f);
    fontManager->fs = fs;
    return ctx;
  }

  Context* createContext(ElementFactory* factory, ScriptState* script, TextureManager* texture, FileSystem* fs, void* userdata)
  {
    FontManager* fontManager = new FontManager();
    Style* style = new Style();
    return createContext(factory, script, texture, fs, fontManager, style, userdata);
  }

  Context* newChildContext(Context* ctx) {
    ScriptState* script = 0;
    if(ctx->script) {
      script = ctx->script->clone();
    }

    Style* style = new Style(ctx->style);
    Context* child = createContext(ctx->factory, script, ctx->texture, ctx->fs, ctx->fontManager, style, ctx->userdata);
    child->parent = ctx;
    return child;
  }
}
