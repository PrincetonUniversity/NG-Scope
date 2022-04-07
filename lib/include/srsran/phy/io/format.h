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

#ifndef SRSRAN_FORMAT_H
#define SRSRAN_FORMAT_H

typedef enum {
  SRSRAN_TEXT,
  SRSRAN_FLOAT,
  SRSRAN_COMPLEX_FLOAT,
  SRSRAN_COMPLEX_SHORT,
  SRSRAN_FLOAT_BIN,
  SRSRAN_COMPLEX_FLOAT_BIN,
  SRSRAN_COMPLEX_SHORT_BIN
} srsran_datatype_t;

#endif // SRSRAN_FORMAT_H
