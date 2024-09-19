#
# Copyright 2013-2021 Software Radio Systems Limited
#
# This file is part of srsRAN
#
# srsRAN is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as
# published by the Free Software Foundation, either version 3 of
# the License, or (at your option) any later version.
#
# srsRAN is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU Affero General Public License for more details.
#
# A copy of the GNU Affero General Public License can be found in
# the LICENSE file in the top-level directory of this distribution
# and at http://www.gnu.org/licenses/.
#

add_subdirectory(dciLib)

# Link libstdc++ and libgcc
if(BUILD_STATIC)
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++ -static-libgcc")
  set(LIBCONFIGPP_LIBRARIES "${LIBCONFIGPP_STATIC_LIBRARY_PATH}")
endif(BUILD_STATIC)

if (RPATH)
  set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
endif (RPATH)


#add_executable(srsue main.cc ue.cc metrics_stdout.cc metrics_csv.cc metrics_json.cc)
# add_executable(ngscope main.c asn_decoder.c cell_status.c dci_decoder.cpp dci_log.c dci_ring_buffer.c dci_sink_client.c dci_sink_recv_dci.c dci_sink_ring_buffer.c dci_sink_send_dci.c dci_sink_serv.c dci_sink_sock.c load_config.c ngscope_main.c ngscope_util.c parse_args.c phich_decoder.c radio.c sib1_helper.cpp skip_tti.c socket.c status_plot.c status_tracker.c sync_dci_remote.c task_scheduler.c task_sf_ring_buffer.c thread_exit.c time_stamp.c ue_list.c ue_tracker.c)
add_executable(ngscope main.c)
# add_executable(ngscope main.c decode_sib.cpp)
add_executable(remote_client remote_client.c)
add_executable(remote_server remote_server.c)

set(SRSRAN_SOURCES srsran_common srsran_mac srsran_phy srsran_radio srsran_gtpu  srsran_rlc srsran_pdcp rrc_asn1 srslog support system)
# set(SRSRAN_SOURCES ${SRSRAN_SOURCES} rrc_nr_asn1 ngap_nr_asn1)
set(SRSRAN_SOURCES ${SRSRAN_SOURCES} ngscope_dci rrc_nr_asn1 ngap_nr_asn1)


target_link_libraries(ngscope ${SRSRAN_SOURCES}
                              ${CMAKE_THREAD_LIBS_INIT}
                              ${Boost_LIBRARIES}
                              ${LIBCONFIG_LIBRARY}
                              ${ATOMIC_LIBS})

target_link_libraries(remote_client   ${SRSRAN_SOURCES}
                              ${CMAKE_THREAD_LIBS_INIT}
                              ${Boost_LIBRARIES}
                              ${LIBCONFIG_LIBRARY}
                              ${ATOMIC_LIBS})


target_link_libraries(remote_server  ${SRSRAN_SOURCES}
                              ${CMAKE_THREAD_LIBS_INIT}
                              ${Boost_LIBRARIES}
                              ${LIBCONFIG_LIBRARY}
                              ${ATOMIC_LIBS})


if (RPATH)
  set_target_properties(ngscope PROPERTIES INSTALL_RPATH ".")
endif (RPATH)

install(TARGETS ngscope DESTINATION ${RUNTIME_DIR})

