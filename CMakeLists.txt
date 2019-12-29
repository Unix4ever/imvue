cmake_minimum_required(VERSION 3.10)

project(imvue)

option(BUILD_SAMPLES "build ImVue samples" OFF)
option(BUILD_IMGUI_FROM_SUBMODULE "build Dear ImGui from submodule" ON)
option(BUILD_LUA_BINDINGS "build Lua bindings" ON)
option(BUILD_CSS_STYLING "build css styling" ON)
option(BUILD_CSS_FROM_SUBMODULE "build libcss using submodule" ON)
option(BUILD_TESTS "build unit test for the project" OFF)
option(IMVUE_NO_EXCEPTIONS "disable raising exceptions. Use log errors instead" OFF)
option(IMVUE_USE_LUAJIT "use luajit" OFF)

set(IMVUE_LUA_VERSION "5.1" CACHE STRING "")
set(LIB_NAME "imvue")
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 11)
endif(NOT CMAKE_CXX_STANDARD)

include_directories(src)
include_directories("${imvue_SOURCE_DIR}/imgui")
include_directories("${imvue_SOURCE_DIR}/nanosvg/src")

set(ADDITIONAL_SOURCES "")
set(ADDITIONAL_LIBS "")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

if(BUILD_LUA_BINDINGS)
  if(IMVUE_USE_LUAJIT)
    find_package(LuaJIT ${IMVUE_LUA_VERSION} REQUIRED)
    if(LUA_STATIC_LIBRARY)
      set(LUA_LIBRARIES ${LUA_STATIC_LIBRARY})
    endif(LUA_STATIC_LIBRARY)
  else(IMVUE_USE_LUAJIT)
    find_package(Lua ${IMVUE_LUA_VERSION} REQUIRED)
  endif(IMVUE_USE_LUAJIT)
  include_directories(${LUA_INCLUDE_DIR})
  set(ADDITIONAL_LIBS ${LUA_LIBRARIES})
  set(ADDITIONAL_SOURCES src/lua/script.cpp ${ADDITIONAL_SOURCES})
endif(BUILD_LUA_BINDINGS)

if(BUILD_CSS_FROM_SUBMODULE)
  add_subdirectory(libcss)
  include_directories(libcss/libcss/include)
  include_directories(libcss/libwapcaplet/include)
  include_directories(libcss/libparserutils/include)
  set(ADDITIONAL_LIBS ${ADDITIONAL_LIBS} css wapcaplet parserutils)
endif(BUILD_CSS_FROM_SUBMODULE)

if(BUILD_TESTS)
  find_package(GTest)
  find_package(benchmark)
  if(GTest_FOUND)
    add_subdirectory(tests)
  endif(GTest_FOUND)

endif(BUILD_TESTS)

if(APPLE)
  set(ADDITIONAL_LIBS
    ${ADDITIONAL_LIBS}
    "-framework CoreFoundation"
    "-framework Carbon"
  )
endif(APPLE)

add_library(${LIB_NAME} STATIC 
  src/rapidxml.hpp
  src/imvue.cpp
  src/imvue_element.h
  src/imvue_element.cpp
  src/imvue_script.cpp
  src/imvue_context.cpp
  src/imvue_style.cpp
  src/imvue_layout.cpp
  src/imstring.cpp
  src/css/select.cpp
${ADDITIONAL_SOURCES})

set(IMGUI_SOURCES
  imgui/imgui.cpp
  imgui/imgui_draw.cpp
  imgui/imgui_widgets.cpp
  imgui/imgui_demo.cpp
)

if(IMVUE_NO_EXCEPTIONS)
  add_definitions("-DIMVUE_NO_EXCEPTIONS=${IMVUE_NO_EXCEPTIONS}")
endif(IMVUE_NO_EXCEPTIONS)

if(BUILD_SAMPLES)
  add_definitions("-DIMGUI_IMPL_OPENGL_LOADER_GLEW")
  add_subdirectory(samples)
endif(BUILD_SAMPLES)

if(BUILD_IMGUI_FROM_SUBMODULE)
  add_library(imgui STATIC ${IMGUI_SOURCES})
  target_link_libraries(${LIB_NAME} imgui)
endif(BUILD_IMGUI_FROM_SUBMODULE)

target_link_libraries(${LIB_NAME} ${ADDITIONAL_LIBS})

if(MSVC)
  target_compile_options(${LIB_NAME} PRIVATE /W4)
else()
  target_compile_options(${LIB_NAME} PRIVATE -Wall -Wextra -pedantic)
endif()
