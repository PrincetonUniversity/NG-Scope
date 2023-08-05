#file(GLOB_RECURSE SOURCES "*.cc" "*.c")
file(GLOB_RECURSE SOURCES "*.c")

add_library(ngscope_dci STATIC ${SOURCES})

target_link_libraries(ngscope_dci config)

if (LIBASN4G)
  target_link_libraries(ngscope_dci ${LIBASN4G_LIBRARY})
endif(LIBASN4G)

if(ENABLE_GUI AND SRSGUI_FOUND)
  target_link_libraries(ngscope_dci ${SRSGUI_LIBRARIES})
endif()
