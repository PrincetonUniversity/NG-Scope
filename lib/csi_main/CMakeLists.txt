LINK_DIRECTORIES(../lib_csi/)
INCLUDE_DIRECTORIES(../lib_csi/)

add_executable(testCSI testCSI.cpp)
target_link_libraries(testCSI csiLib srslte_phy srslte_common srslte_rf pthread config)

add_executable(socketTest socketTest.c)

if(SRSGUI_FOUND)
  include_directories(${SRSGUI_INCLUDE_DIRS})
  target_link_libraries(testCSI ${SRSGUI_LIBRARIES})
else(SRSGUI_FOUND)
  add_definitions(-DDISABLE_GRAPHICS)
endif(SRSGUI_FOUND)


