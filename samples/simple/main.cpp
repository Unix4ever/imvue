// dear imgui: standalone example application for SDL2 + OpenGL
// If you are new to dear imgui, see examples/README.txt and documentation at the top of imgui.cpp.
// (SDL is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan graphics context creation, etc.)

// **DO NOT USE THIS CODE IF YOUR CODE/ENGINE IS USING MODERN OPENGL (SHADERS, VBO, VAO, etc.)**
// **Prefer using the code in the example_sdl_opengl3/ folder**
// See imgui_impl_sdl.cpp for details.

#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl2.h"
#include <stdio.h>
#include <SDL.h>
#include <SDL_opengl.h>
#include "imvue.h"
#include "imvue_generated.h"
#include <iostream>
#include <fstream>
#include <map>

#include "lua/script.h"

extern "C" {
  #include <lua.h>
  #include <lualib.h>
  #include <lauxlib.h>
}

class OpenGL2TextureManager : public ImVue::TextureManager {
  public:
    ~OpenGL2TextureManager()
    {
      for(int i = 0; i < mTextures.size(); ++i) {
        GLuint tid = mTextures[i];
        glDeleteTextures(1, &tid);
      }
      mTextures.clear();
    }

    ImTextureID createTexture(void* pixels, int width, int height) {
      // Upload texture to graphics system
      GLuint texture_id = 0;
      GLint last_texture;
      glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
      glGenTextures(1, &texture_id);
      glBindTexture(GL_TEXTURE_2D, texture_id);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
#ifdef GL_UNPACK_ROW_LENGTH
      glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
      glBindTexture(GL_TEXTURE_2D, last_texture);
      mTextures.reserve(mTextures.size() + 1);
      mTextures.push_back(texture_id);
      return (ImTextureID)(intptr_t)texture_id;
    }

    void deleteTexture(ImTextureID id) {
      GLuint tex = (GLuint)(intptr_t)id;
      glDeleteTextures(1, &tex);
    }
  private:
    typedef ImVector<GLuint> Textures;

    Textures mTextures;
};

// Main code
int main(int argc, char** argv)
{
  if((argc == 2 && ImStricmp(argv[1], "--help") == 0) || argc > 2) {
    std::cout << "Usage imvue_demo [document]\n";
    return 0;
  }

    // Setup SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
    {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

    // Setup window
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window* window = SDL_CreateWindow("Dear ImGui SDL2+OpenGL example", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    float scale = 1.0f;

    ImGuiStyle& style = ImGui::GetStyle();
    style.FrameRounding = 5.0f;
    style.FrameBorderSize = 1.0f;
    style.ScaleAllSizes(scale);

    // Setup Platform/Renderer bindings
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL2_Init();

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Read 'misc/fonts/README.txt' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    io.Fonts->AddFontFromFileTTF("../../../imgui/misc/fonts/DroidSans.ttf", 15.0f * scale);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != NULL);

    // Our state
    bool show_demo_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    lua_State * L = luaL_newstate();
    luaL_openlibs(L);

    ImVue::registerBindings(L);

    {
      ImVue::Context* ctx = ImVue::createContext(
          ImVue::createElementFactory(),
          new ImVue::LuaScriptState(L),
          new OpenGL2TextureManager()
      );

      ctx->scale = ImVec2(scale, scale);

      ImVue::Document document(ctx);
      const char* page = "simple.xml";
      if(argc == 2) {
        page = argv[1];
      }
      char* data = ctx->fs->load(page);
      document.parse(data);
      ImGui::MemFree(data);

      // Main loop
      bool done = false;
      while (!done)
      {
          // Poll and handle events (inputs, window resize, etc.)
          // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
          // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
          // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
          // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
          SDL_Event event;
          while (SDL_PollEvent(&event))
          {
              ImGui_ImplSDL2_ProcessEvent(&event);
              if (event.type == SDL_QUIT)
                  done = true;
          }

          // Start the Dear ImGui frame
          ImGui_ImplOpenGL2_NewFrame();
          ImGui_ImplSDL2_NewFrame(window);
          ImGui::NewFrame();

          document.render();

          // Rendering
          ImGui::Render();
          glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
          glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
          glClear(GL_COLOR_BUFFER_BIT);
          //glUseProgram(0); // You may want this if using this code in an OpenGL 3+ context where shaders may be bound
          ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
          SDL_GL_SwapWindow(window);
      }

      // Cleanup
      ImGui_ImplOpenGL2_Shutdown();
      ImGui_ImplSDL2_Shutdown();
      ImGui::DestroyContext();

      SDL_GL_DeleteContext(gl_context);
      SDL_DestroyWindow(window);
      SDL_Quit();
    }

    lua_close(L);

    return 0;
}
