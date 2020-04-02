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


#ifndef __IMVUE_CONTEXT_H__
#define __IMVUE_CONTEXT_H__

#include <map>
#include "imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"

namespace ImVue {

  class Context;
  class ComponentContainer;
  class ElementFactory;
  class Element;
  class ScriptState;
  class Style;
  class FontManager;
  struct Layout;

  /**
   * Customization point for file system access
   */
  class FileSystem {
    public:
      enum Mode {
        TEXT,
        BINARY
      };

      typedef void LoadCallback(char* data);

      virtual ~FileSystem() {}

      /**
       * Loads file
       *
       * @param path file path
       * @param size output size to the variable
       * @param mode file read mode
       * @return loaded data
       */
      virtual char* load(const char* path, int* size = NULL, Mode mode = Mode::TEXT) = 0;

      /**
       * Load file async
       *
       * @param path file path
       * @param callback load callback
       */
      virtual void loadAsync(const char* path, LoadCallback* callback) = 0;
  };

  /**
   * Default filesystem implementation
   */
  class SimpleFileSystem : public FileSystem {
    public:
      /**
       * Loads file
       *
       * @param path file path
       * @param size output size to the variable
       * @param mode file read mode
       * @return loaded data
       */
      char* load(const char* path, int* size = NULL, Mode mode = Mode::TEXT);

      /**
       * Load file async
       *
       * @param path file path
       * @param callback load callback
       */
      void loadAsync(const char* path, LoadCallback* callback);
  };

  /**
   * Texture manager can be used by elements to load images
   */
  class TextureManager {
    public:
      virtual ~TextureManager() {};
      /**
       * Creates texture from raw data
       *
       * @param data image data in rgba format
       * @param width texture width
       * @param height texture height
       * @returns generated texture id
       */
      virtual ImTextureID createTexture(void* data, int width, int height) = 0;

      /**
       * Deletes old texture
       */
      virtual void deleteTexture(ImTextureID id) = 0;
  };

  /**
   * Font manager
   */
  class FontManager {
    public:
      struct FontHandle {
        ImFont* font;
        ImFontConfig cfg;
        ImVector<ImWchar> glyphRanges;
        float size;
      };

      FontManager();

      /**
       * Load font from disk
       *
       * @param name Font name
       * @param path Font path
       *
       * @returns ImFont if succeed
       */
      ImFont* loadFontFromFile(const char* name, const char* path, ImVector<ImWchar> glyphRanges);

      /**
       * Activate font using name
       *
       * @param name Font name
       * @param size Font size
       *
       * @returns true if pushed
       */
      bool pushFont(const char* name);

      inline FontHandle& getFont(const char* name) {
        return mFonts.at(ImHashStr(name));
      }

      FileSystem* fs;

    private:
      typedef std::map<ImU32, FontHandle> Fonts;
      Fonts mFonts;
  };

  /**
   * Object that keeps imvue configuration
   */
  class Context {
    public:
      ~Context();

      ScriptState* script;
      TextureManager* texture;
      ElementFactory* factory;
      FileSystem* fs;
      ComponentContainer* root;
      Context* parent;
      Style* style;
      FontManager* fontManager;
      Layout* layout;
      // additional userdata that will be available from all the components
      void* userdata;

      // adjust styles scale using this variable
      ImVec2 scale;
  };

  /**
   * Create new context
   */
  Context* createContext(ElementFactory* factory, ScriptState* script = 0, TextureManager* texture = 0, FileSystem* fs = 0, void* userdata = 0);

  /**
   * Clone existing context
   */
  Context* newChildContext(Context* ctx);
}

#endif
