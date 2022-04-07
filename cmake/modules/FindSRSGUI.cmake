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

# - Try to find SRSGUI
# Once done this will define
#  SRSGUI_FOUND        - System has srsgui
#  SRSGUI_INCLUDE_DIRS - The srsgui include directories
#  SRSGUI_LIBRARIES    - The srsgui library

find_package(PkgConfig)
pkg_check_modules(PC_SRSGUI QUIET srsgui)
IF(NOT SRSGUI_FOUND)

FIND_PATH(
    SRSGUI_INCLUDE_DIRS
    NAMES srsgui/srsgui.h
    HINTS ${PC_SRSGUI_INCLUDEDIR}
          ${PC_SRSGUI_INCLUDE_DIRS}
          $ENV{SRSGUI_DIR}/include
    PATHS /usr/local/include
          /usr/include
)

FIND_LIBRARY(
    SRSGUI_LIBRARIES
    NAMES srsgui
    HINTS ${PC_SRSGUI_LIBDIR}
          ${CMAKE_INSTALL_PREFIX}/lib
          ${CMAKE_INSTALL_PREFIX}/lib64
          $ENV{SRSGUI_DIR}/lib
    PATHS /usr/local/lib
          /usr/local/lib64
          /usr/lib
          /usr/lib64
)

message(STATUS "SRSGUI LIBRARIES " ${SRSGUI_LIBRARIES})
message(STATUS "SRSGUI INCLUDE DIRS " ${SRSGUI_INCLUDE_DIRS})

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(SRSGUI DEFAULT_MSG SRSGUI_LIBRARIES SRSGUI_INCLUDE_DIRS)
MARK_AS_ADVANCED(SRSGUI_LIBRARIES SRSGUI_INCLUDE_DIRS)

ENDIF(NOT SRSGUI_FOUND)

