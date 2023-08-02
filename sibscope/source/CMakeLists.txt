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

# Link libstdc++ and libgcc
if(BUILD_STATIC)
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++ -static-libgcc")
  set(LIBCONFIGPP_LIBRARIES "${LIBCONFIGPP_STATIC_LIBRARY_PATH}")
endif(BUILD_STATIC)

if (RPATH)
  set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
endif (RPATH)

add_executable(sibscope main.c)

set(SRSRAN_SOURCES srsran_common srsran_mac srsran_phy srsran_radio srslog support system)

target_link_libraries(sibscope ${SRSRAN_SOURCES}
                              ${CMAKE_THREAD_LIBS_INIT}
                              ${LIBCONFIG_LIBRARY}
                              ${ATOMIC_LIBS})

if (LIBASN1DEC)
    target_link_libraries(sibscope ${LIBASN1DEC_LIBRARY})
endif(LIBASN1DEC)
                            


if (RPATH)
  set_target_properties(sibscope PROPERTIES INSTALL_RPATH ".")
endif (RPATH)

install(TARGETS ngscope DESTINATION ${RUNTIME_DIR})

