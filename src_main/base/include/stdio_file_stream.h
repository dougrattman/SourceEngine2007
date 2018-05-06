// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef BASE_INCLUDE_STDIO_FILE_STREAM_H_
#define BASE_INCLUDE_STDIO_FILE_STREAM_H_

#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <tuple>

#include "base/include/posix_errno_info.h"

namespace source {
// Input/output result.
template <typename T>
using io_result = std::tuple<T, posix_errno_info>;

// stdio-based file stream.
class stdio_file_stream {
  // Allows factory to open files.
  friend class stdio_file_stream_factory;

 public:
  // Move constructor.
  stdio_file_stream(stdio_file_stream&& f) noexcept : fd_{f.fd_} {
    f.fd_ = nullptr;
  }

  // Move assignment operator.
  stdio_file_stream& operator=(stdio_file_stream&& f) noexcept {
    const posix_errno_code errno_code{close()};
    CHECK(errno_code == 0, errno_code);

    std::swap(fd_, f.fd_);

    return *this;
  }

  // Like fscanf_s, read from file by |format|.
  io_result<size_t> scan(_In_z_ _Scanf_s_format_string_ const char* format,
                         ...) const noexcept {
    // fscanf_s analog.
    int fields_assigned_count;
    va_list arg_list;
    va_start(arg_list, format);
    fields_assigned_count = _vfscanf_s_l(fd_, format, nullptr, arg_list);
    va_end(arg_list);

    return {fields_assigned_count != EOF ? fields_assigned_count : 0,
            make_posix_errno_info(ferror(fd_))};
  }

  // Like fgetc, get char from file.
  io_result<int> getc() const noexcept {
    int c = fgetc(fd_);
    return {
        c, c != EOF ? posix_errno_info_ok : make_posix_errno_info(ferror(fd_))};
  }

  // Like fgets, get C string from file.
  template <size_t buffer_size>
  io_result<char*> gets(char (&buffer)[buffer_size]) const noexcept {
    char* string = fgets(buffer, buffer_size, fd_);
    return {string, string != nullptr ? posix_errno_info_ok
                                      : make_posix_errno_info(ferror(fd_))};
  }

  // Reads into |buffer| of size |buffer_size| |elements_count| elements.
  template <typename T, size_t buffer_size>
  io_result<size_t> read(T (&buffer)[buffer_size], size_t elements_count) const
      noexcept {
    return read(buffer, buffer_size * sizeof(T), sizeof(T), elements_count);
  }

  // Reads into |buffer|.
  template <typename T, size_t buffer_size>
  io_result<size_t> read(T (&buffer)[buffer_size]) const noexcept {
    return read(buffer, buffer_size);
  }

  // Reads into |buffer|, terminates read data with '\0'.
  template <size_t buffer_size>
  io_result<size_t> read(char (&buffer)[buffer_size]) const noexcept {
    static_assert(buffer_size >= 1);

    auto [read_size, error_info] = read(buffer, buffer_size - 1);

    if (error_info.is_success()) buffer[read_size] = '\0';

    return {read_size, error_info};
  }

  // Reads into |buffer|, terminates read data with L'\0'.
  template <size_t buffer_size>
  io_result<size_t> read(wchar_t (&buffer)[buffer_size]) const noexcept {
    static_assert(buffer_size >= 1);

    auto [read_size, error_info] = read(buffer, buffer_size - 1);

    if (error_info.is_success()) buffer[read_size] = L'\0';

    return {read_size, error_info};
  }

  // Like fprintf_s, write to file by |format|.
  io_result<size_t> print(_In_z_ _Printf_format_string_ const char* format,
                          ...) const noexcept {
    // fprintf_s analog.
    int bytes_written_count;
    va_list arg_list;
    va_start(arg_list, format);
    bytes_written_count = _vfprintf_s_l(fd_, format, nullptr, arg_list);
    va_end(arg_list);

    return {bytes_written_count >= 0 ? bytes_written_count : 0,
            make_posix_errno_info(ferror(fd_))};
  }

  // Writes |elements_count| elements from |buffer| to file.
  template <typename T, size_t buffer_size>
  io_result<size_t> write(T (&buffer)[buffer_size], size_t elements_count) const
      noexcept {
    return write(&buffer, sizeof(T), elements_count);
  }

  // Writes all elements from |buffer| to file.
  template <typename T, size_t buffer_size>
  io_result<size_t> write(T (&buffer)[buffer_size]) const noexcept {
    return write(&buffer, sizeof(T), buffer_size);
  }

  ~stdio_file_stream() noexcept {
    const posix_errno_code errno_code{close()};
    CHECK(errno_code == posix_errno_code_ok, errno_code);
  }

 private:
  // stdio file descriptor.
  FILE* fd_;

  // Reads data from file into |buffer| of size |buffer_size_bytes| with element
  // of |element_size_bytes| and elements count |max_elements_count|.  Gets "the
  // number of (whole) items that were read into the buffer, which may be less
  // than count if a read error or the end of the file is encountered before
  // count is reached".  Uses fread_s internally, see
  // https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/fread-s.
  [[nodiscard]] io_result<size_t> read(void* buffer, size_t buffer_size_bytes,
                                       size_t element_size_bytes,
                                       size_t max_elements_count) const
      noexcept {
    return {fread_s(buffer, buffer_size_bytes, element_size_bytes,
                    max_elements_count, fd_),
            make_posix_errno_info(ferror(fd_))};
  }

  // Writes data from |buffer| of size |element_size_bytes| and count
  // |max_elements_count| to file. Gets "the number of full items actually
  // written, which may be less than count if an error occurs. Also, if an error
  // occurs, the file-position indicator cannot be determined".  Uses fwrite
  // internally, see
  // https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/fwrite.
  [[nodiscard]] io_result<size_t> write(const void* buffer,
                                        size_t element_size_bytes,
                                        size_t max_elements_count) const
      noexcept {
    return {std::fwrite(buffer, element_size_bytes, max_elements_count, fd_),
            make_posix_errno_info(ferror(fd_))};
  }

  // Closes file.
  [[nodiscard]] posix_errno_code close() noexcept {
    const posix_errno_code errno_code{fd_ ? fclose(fd_) : posix_errno_code_ok};
    fd_ = nullptr;
    return errno_code;
  }

  // Opens file stream from file path |file_path| and mode |mode|.
  stdio_file_stream(_In_ FILE* fd) noexcept : fd_{fd} {}

  // Empty file stream, invalid.
  static stdio_file_stream null() noexcept {
    return stdio_file_stream{nullptr};
  }

  stdio_file_stream(const stdio_file_stream& f) = delete;
  stdio_file_stream& operator=(const stdio_file_stream& f) = delete;
};

// Small file factory to simplify initial file open process.
class stdio_file_stream_factory {
 public:
  // Opens file with path |file_path| and mode |mode|.
  [[nodiscard]] static std::tuple<stdio_file_stream, posix_errno_info> open(
      const char* file_path, const char* mode) noexcept {
    FILE* fd;
    const posix_errno_code errno_code{fopen_s(&fd, file_path, mode)};
    return errno_code == posix_errno_code_ok
               ? std::make_tuple(stdio_file_stream{fd}, posix_errno_info_ok)
               : std::make_tuple(stdio_file_stream::null(),
                                 make_posix_errno_info(errno_code));
  }

 private:
  stdio_file_stream_factory() = delete;
  ~stdio_file_stream_factory() = delete;
  stdio_file_stream_factory(const stdio_file_stream_factory& f) = delete;
  stdio_file_stream_factory& operator=(const stdio_file_stream_factory& f) =
      delete;
};
}  // namespace source

#endif  // !BASE_INCLUDE_STDIO_FILE_STREAM_H_
