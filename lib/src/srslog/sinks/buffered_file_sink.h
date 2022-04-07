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

#ifndef SRSLOG_BUFFERED_FILE_SINK_H
#define SRSLOG_BUFFERED_FILE_SINK_H

#include "file_utils.h"
#include "srsran/srslog/sink.h"

namespace srslog {

/// This class is a wrapper of a file handle that buffers the input data into an internal buffer and writes its contents
/// to the file once the buffer is full or in object destruction.
class buffered_file_sink : public sink
{
public:
  buffered_file_sink(std::string filename, std::size_t capacity, std::unique_ptr<log_formatter> f) :
    sink(std::move(f)), filename(std::move(filename))
  {
    buffer.reserve(capacity);
  }

  ~buffered_file_sink() override { flush_buffer(); }

  buffered_file_sink(const buffered_file_sink& other) = delete;
  buffered_file_sink& operator=(const buffered_file_sink& other) = delete;

  detail::error_string write(detail::memory_buffer input_buffer) override
  {
    // Create a new file the first time we hit this method.
    if (!is_file_created) {
      is_file_created = true;
      assert(!handler && "No handler should be created yet");
      if (auto err_str = handler.create(filename)) {
        return err_str;
      }
    }

    if (has_room_for(input_buffer.size())) {
      buffer.insert(buffer.end(), input_buffer.begin(), input_buffer.end());
      return {};
    }

    return flush_buffer();
  }

  detail::error_string flush() override
  {
    if (auto err = flush_buffer()) {
      return err;
    }
    return handler.flush();
  }

private:
  /// Returns true if the internal buffer has room for the specified input size,
  /// otherwise returns false.
  bool has_room_for(std::size_t s) const { return s + buffer.size() < buffer.capacity(); }

  /// Flushes the buffer contents into the file.
  detail::error_string flush_buffer()
  {
    if (buffer.empty()) {
      return {};
    }
    auto err = handler.write(detail::memory_buffer(buffer.data(), buffer.size()));
    buffer.clear();
    return err;
  }

private:
  const std::string filename;
  file_utils::file  handler;
  std::vector<char> buffer;
  bool              is_file_created = false;
};

} // namespace srslog

#endif // SRSLOG_BUFFERED_FILE_SINK_H
