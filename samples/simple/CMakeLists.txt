
add_executable(imvue_demo main.cpp)

file(GLOB xml ${CMAKE_CURRENT_SOURCE_DIR}/*.xml)
file(GLOB lua ${CMAKE_CURRENT_SOURCE_DIR}/*.lua)

target_link_libraries(imvue_demo imvue imgui_sdl2 ${LUA_LIBRARIES})
set(resources ${xml} ${lua})
foreach(res ${resources})
  configure_file(${res} ${CMAKE_CURRENT_BINARY_DIR} COPYONLY)
endforeach(res)

add_subdirectory(components)
