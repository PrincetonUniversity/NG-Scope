# Link libstdc++ and libgcc
if(BUILD_STATIC)
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++ -static-libgcc")
  set(LIBCONFIGPP_LIBRARIES "${LIBCONFIGPP_STATIC_LIBRARY_PATH}")
endif(BUILD_STATIC)

if (RPATH)
  set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
endif (RPATH)


#add_executable(srsue main.cc ue.cc metrics_stdout.cc metrics_csv.cc metrics_json.cc)
add_executable(ngscope_refact main.c)

set(SRSRAN_SOURCES srsran_common srsran_mac srsran_phy srsran_radio srsran_gtpu  srsran_rlc srsran_pdcp rrc_asn1 srslog support system)
set(SRSRAN_SOURCES ${SRSRAN_SOURCES} NGScopeDCI rrc_nr_asn1 ngap_nr_asn1)

target_link_libraries(ngscope_refact ${SRSRAN_SOURCES}
                              ${CMAKE_THREAD_LIBS_INIT}
                              ${Boost_LIBRARIES}
                              ${LIBCONFIG_LIBRARY}
                              ${ATOMIC_LIBS})


if (RPATH)
  set_target_properties(ngscope_refact PROPERTIES INSTALL_RPATH ".")
endif (RPATH)

install(TARGETS ngscope_refact DESTINATION ${RUNTIME_DIR})

