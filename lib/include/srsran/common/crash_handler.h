/**
 * Copyright 2013-2021 Software Radio Systems Limited
 *
 * This file is part of srsRAN.
 *
 * srsRAN is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * srsRAN is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * A copy of the GNU Affero General Public License can be found in
 * the LICENSE file in the top-level directory of this distribution
 * and at http://www.gnu.org/licenses/.
 *
 */

/**
 * @file crash_handler.h
 * @brief Common handler to catch segfaults and write backtrace to file.
 */

#ifndef SRSRAN_CRASH_HANDLER_H
#define SRSRAN_CRASH_HANDLER_H

#include "srsran/config.h"
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

void srsran_debug_handle_crash(int argc, char** argv);

#ifdef __cplusplus
}
#endif // __cplusplus
#endif // SRSRAN_CRASH_HANDLER_H
