
file(GLOB imv ${CMAKE_CURRENT_SOURCE_DIR}/*.imv)
file(GLOB lua ${CMAKE_CURRENT_SOURCE_DIR}/*.lua)

set(resources ${imv} ${lua})
foreach(res ${resources})
  configure_file(${res} ${CMAKE_CURRENT_BINARY_DIR} COPYONLY)
endforeach(res)
