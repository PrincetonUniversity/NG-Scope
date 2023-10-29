#file(GLOB_RECURSE SOURCES "*.cc" "*.c")
file(GLOB_RECURSE SOURCES "*.c")

add_library(ngscope_dci_lib STATIC ${SOURCES})

target_link_libraries(ngscope_dci_lib config)

if (LIBASN4G)
  target_link_libraries(ngscope_dci_lib ${LIBASN4G_LIBRARY})
endif(LIBASN4G)

if (LIBASN5G)
  target_link_libraries(ngscope_dci_lib ${LIBASN5G_LIBRARY})
endif(LIBASN5G)

if(ENABLE_GUI AND SRSGUI_FOUND)
  target_link_libraries(ngscope_dci_lib ${SRSGUI_LIBRARIES})
endif()
