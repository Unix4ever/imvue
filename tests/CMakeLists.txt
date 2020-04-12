set(UNIT_TESTS "unit-tests")
set(BENCHMARK "benchmark")

set(SOURCES
  unit/main.cpp
  unit/parser.cpp
  unit/imvue_tests.cpp
  unit/state.cpp
  unit/style.cpp
  unit/layout.cpp
)

include_directories(${GTEST_INCLUDE_DIRS})

set(LIBS
  imgui
  imvue
  ${GTEST_LIBRARIES}
  ${ADDITIONAL_LIBS}
)

if(LUA_FOUND)
  include_directories(${LUA_INCLUDE_DIR})
  add_definitions(-DWITH_LUA)
endif(LUA_FOUND)

add_executable(${UNIT_TESTS} ${SOURCES})

if(UNIX)
  set(LIBS ${LIBS} pthread)
endif(UNIX)

target_link_libraries(
  ${UNIT_TESTS}
  ${LIBS}
)

if(benchmark_FOUND)
  include_directories(tests/benchmark)
  set(SOURCES
    benchmark/main.cpp
  )

  if(LUA_FOUND)
    include_directories(benchmark/imgui-lua/)
    set(SOURCES
      ${SOURCES}
      benchmark/imgui-lua/imgui_lua_bindings.cpp
    )
  endif(LUA_FOUND)

  set(LIBS
    ${LIBS}
    benchmark::benchmark
  )

  add_executable(${BENCHMARK} ${SOURCES})
  target_link_libraries(
    ${BENCHMARK}
    ${LIBS}
  )
endif(benchmark_FOUND)

file(GLOB xml ${CMAKE_CURRENT_SOURCE_DIR}/resources/*.xml)
file(GLOB lua ${CMAKE_CURRENT_SOURCE_DIR}/resources/*.lua)
file(GLOB imv ${CMAKE_CURRENT_SOURCE_DIR}/resources/*.imv)

set(resources ${xml} ${lua} ${imv})
foreach(res ${resources})
  configure_file(${res} ${CMAKE_CURRENT_BINARY_DIR} COPYONLY)
endforeach(res)
