find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(GLEW 2.0 REQUIRED)

include_directories("${imvue_SOURCE_DIR}/imgui/examples")
include_directories(${OPENGL_INCLUDE_DIR})
include_directories(${GLEW_INCLUDE_DIR})
include_directories(${SDL2_INCLUDE_DIRS})

add_library(imgui_sdl2
  ${imvue_SOURCE_DIR}/imgui/examples/imgui_impl_opengl2.cpp
  ${imvue_SOURCE_DIR}/imgui/examples/imgui_impl_sdl.cpp
  ${imvue_SOURCE_DIR}/imgui/imgui_demo.cpp
)

set(LIBS
  imgui
  ${OPENGL_LIBRARIES}
  ${GLEW_LIBRARIES}
  #${SDL2_LIBRARIES}
  ${LUA_LIBRARIES}
  SDL2::SDL2main
  SDL2::SDL2-static
)

if(APPLE)
  find_library(CORE_FOUNDATION NAMES CoreFoundation)
  find_library(APPLICATION_SERVICES NAMES ApplicationServices)

  set(LIBS ${LIBS}
    ${CORE_FOUNDATION}
    ${APPLICATION_SERVICES}
  )
endif()

target_link_libraries(
  imgui_sdl2
  ${LIBS}
)

add_subdirectory(simple)
