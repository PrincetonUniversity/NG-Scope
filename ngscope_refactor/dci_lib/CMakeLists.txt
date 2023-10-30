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

#add_subdirectory(src)

file(GLOB_RECURSE SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.c ${CMAKE_CURRENT_SOURCE_DIR}/*.h)
#file(GLOB_RECURSE SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.h)

add_library(NGScopeDCI STATIC ${SOURCES})
target_include_directories(NGScopeDCI PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/hdr/)

target_link_libraries(NGScopeDCI config)

if (LIBASN4G)
  target_link_libraries(NGScopeDCI ${LIBASN4G_LIBRARY})
endif(LIBASN4G)

if (LIBASN5G)
  target_link_libraries(NGScopeDCI ${LIBASN5G_LIBRARY})
endif(LIBASN5G)

if(ENABLE_GUI AND SRSGUI_FOUND)
  target_link_libraries(NGScopeDCI ${SRSGUI_LIBRARIES})
endif()
