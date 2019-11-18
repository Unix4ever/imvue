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

#include "imgui.h"
#include "imvue_context.h"
#include "imvue_generated.h"
#include "imvue_script.h"

#include <iostream>
#include <fstream>

namespace ImVue {

  char* SimpleFileSystem::load(const char* path)
  {
    std::ifstream stream(path, std::ifstream::binary);
    char* data = NULL;

    try
    {
      stream.seekg(0, std::ios::end);
      size_t length = stream.tellg();
      stream.seekg(0, std::ios::beg);

      data = (char*)ImGui::MemAlloc(length + 1);
      data[length] = '\0';
      stream.read(data, length);
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
    }

    if(script) {
      delete script;
    }
  }

  Context* createContext(ElementFactory* factory, ScriptState* script, TextureManager* texture, FileSystem* fs, void* userdata)
  {
    Context* ctx = new Context();
    memset(ctx, 0, sizeof(Context));
    if(factory == 0) {
      ctx->factory = createElementFactory();
    }
    ctx->factory = factory;
    ctx->texture = texture;
    if(!fs) {
      fs = new SimpleFileSystem();
    }
    ctx->fs = fs;
    ctx->script = script;
    ctx->userdata = userdata;
    return ctx;
  }

  Context* newChildContext(Context* ctx) {
    ScriptState* script = 0;
    if(ctx->script) {
      script = ctx->script->clone();
    }

    Context* child = createContext(ctx->factory, script, ctx->texture, ctx->fs, ctx->userdata);
    child->parent = ctx;
    return child;
  }
}
