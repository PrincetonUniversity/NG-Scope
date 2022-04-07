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
endif(BUILD_STATIC)

if (RPATH)
  set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
endif (RPATH)


#add_executable(srsue main.cc ue.cc metrics_stdout.cc metrics_csv.cc metrics_json.cc)
add_executable(hello main.c)

set(SRSRAN_SOURCES srsran_common srsran_mac srsran_phy srsran_radio srsran_gtpu  srsran_rlc srsran_pdcp rrc_asn1 srslog support system)
set(SRSRAN_SOURCES ${SRSRAN_SOURCES} ngscope_dci rrc_nr_asn1 ngap_nr_asn1)

target_link_libraries(hello   ${SRSRAN_SOURCES}
                              ${CMAKE_THREAD_LIBS_INIT}
                              ${Boost_LIBRARIES}
                              ${LIBCONFIG_LIBRARY}
                              ${ATOMIC_LIBS})

if (RPATH)
  set_target_properties(hello PROPERTIES INSTALL_RPATH ".")
endif (RPATH)

install(TARGETS hello DESTINATION ${RUNTIME_DIR})

