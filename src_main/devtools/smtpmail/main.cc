// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// SMTP RFC 821 Specifies <CR><LF> to terminate lines.
//
// <CR> = Carriage Return			C: \r	Dec: 13 Hex: 0x0d Oct:
// 0015 <LF> = Line Feed	( Newline )		C: \n	Dec: 10
// Hex: 0x0a Oct: 0012

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <iterator>
#include <limits>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

#include "base/include/posix_errno_info.h"
#include "base/include/stdio_file_stream.h"
#include "base/include/windows/scoped_winsock_initializer.h"
#include "base/include/windows/windows_errno_info.h"

#include "simple_socket.h"

// Command line args.
struct Args {
  // Args count.
  int argc;
  // Args values.
  char **argv;
};

// Initialzie args.
Args ArgsInit(int argc, char *argv[]) { return {argc, argv}; }

// Tests for the presence of arg |arg_name|.
bool ArgsExist(const Args &args, const char *arg_name) {
  if (arg_name && arg_name[0] == '-') {
    for (int i = 0; i < args.argc; ++i) {
      // Is this a switch?
      if (args.argv[i][0] != '-') continue;

      if (!_stricmp(arg_name, args.argv[i])) return true;
    }
  }

  return false;
}

// Looks for the arg |arg_name| and returns it's parameter or
// |arg_default_value| otherwise.
const char *ArgsGet(const Args &args, const char *arg_name,
                    const char *arg_default_value) {
  // don't bother with the last arg, it can't be a switch with a parameter
  for (int i = 0; i < args.argc - 1; ++i) {
    // Is this a switch?
    if (args.argv[i][0] != '-') continue;

    if (!_stricmp(arg_name, args.argv[i])) {
      // If the next arg is not a switch, return it
      if (args.argv[i + 1][0] != '-') return args.argv[i + 1];
    }
  }

  return arg_default_value;
}

// Prints error |format, ...| to stderr + usage to stdout and exit.
[[noreturn]] void Fail(const char *format, ...) {
  va_list list;
  va_start(list, format);

  vfprintf(stderr, format, list);
  printf(
      "Usage: smtpmail -to <address(-es, ';' separated)> -from <address> "
      "-subject \"subject text\" [-verbose] [-server server.name] "
      "<FILENAME.TXT>\n");

  exit(EXIT_FAILURE);
}

// Parser args.
struct ParsedArgs {
  const char *server_name;
  const unsigned short port_number;
  const char *from;
  const std::vector<std::string> to;
  const char *subject;
  const char *file_name;
  const bool is_verbose;
};

// Split std::string.
template <typename TOut>
void split(const std::string &s, char delim, TOut result,
           bool remove_empty = true) {
  std::stringstream ss{s};
  std::string item;

  while (std::getline(ss, item, delim)) {
    if (!(remove_empty && item.empty())) {
      *(result++) = item;
    }
  }
}

// Split std::string.
std::vector<std::string> split(const std::string &s, char delim,
                               bool remove_empty = true) {
  std::vector<std::string> elems;
  split(s, delim, std::back_inserter(elems), remove_empty);
  return elems;
}

// Parse args.
ParsedArgs ParseArgs(int argc, char *argv[]) {
  const Args args{ArgsInit(argc, argv)};
  const char *server_name{ArgsGet(args, "-server", "smtp1.valvesoftware.com")};
  const char *to_raw{ArgsGet(args, "-to", nullptr)};

  if (!to_raw[0]) Fail("Must specify a recipient with -to <address(es)>.\n");

  // split ; separated To string into an array of strings
  std::vector<std::string> to = split(to_raw, ';');

  const char *from{ArgsGet(args, "-from", nullptr)};
  if (!from) Fail("Must specify a sender with -from <address>.\n");

  const char *subject{ArgsGet(args, "-subject", "<NONE>")};

  char *end_char;
  unsigned long port_number{
      strtoul(ArgsGet(args, "-port", "25"), &end_char, 10)};

  if (port_number == 0 || port_number > 65535 ||
      source::failed(source::posix_errno_code_last_error())) {
    Fail("port %ul is bad: %s.\n", port_number,
         end_char == nullptr ? source::posix_errno_info_last_error().description
                             : "not in range [1, 65535]");
  }

  static_assert(std::numeric_limits<unsigned short>::max() <= 65535,
                "unsigned short should store port number up to max.");

  const char *file_name{argv[argc - 1]};
  const bool is_verbose{ArgsExist(args, "-verbose")};

  return {server_name, static_cast<unsigned short>(port_number),
          from,        to,
          subject,     file_name,
          is_verbose};
}

// Simple routine to printf() all of the socket traffic for -verbose.
void DumpSocket(HSOCKET, const char *pData) {
  printf("%s", pData);
  fflush(stdout);
}

static const char *months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                               "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};

// Mail a text file using the SMTP mail server connected to socket.
void MailSendFile(HSOCKET socket, const char *from,
                  const std::vector<std::string> to, const char *subject,
                  const char *file_name) {
  auto[file, errno_info] =
      source::stdio_file_stream_factory::open(file_name, "r");

  if (!errno_info.is_success()) {
    Fail("can't open %s: %s.\n", file_name, errno_info.description);
  }

  SocketSendString(socket, "HELO\r\n");
  SocketWait(socket, "\n");

  char buffer[1024];

  sprintf_s(buffer, "MAIL FROM: <%s>\r\n", from);
  SocketSendString(socket, buffer);
  SocketWait(socket, "\n");

  for (const auto &rec : to) {
    sprintf_s(buffer, "RCPT TO: <%s>\r\n", rec.c_str());

    SocketSendString(socket, buffer);
    SocketWait(socket, "\n");
  }

  SocketSendString(socket, "DATA\r\n");
  SocketWait(socket, "\n");

  time_t now;
  if (time(&now) == -1) {
    Fail("can't get current time, stop send mail.\n");
  }

  tm local_now;
  if (!gmtime_s(&local_now, &now)) {
    Fail("can't get current time, stop send mail.\n");
  }

  sprintf_s(buffer, "DATE: %02d %s %4d %02d:%02d:%02d\r\n", local_now.tm_mday,
            months[local_now.tm_mon], local_now.tm_year + 1900,
            local_now.tm_hour, local_now.tm_min, local_now.tm_sec);
  SocketSendString(socket, buffer);

  sprintf_s(buffer, "FROM: %s\r\n", from);
  SocketSendString(socket, buffer);

  for (const auto &rec : to) {
    sprintf_s(buffer, "TO: %s\r\n", rec.c_str());

    SocketSendString(socket, buffer);
  }

  sprintf_s(buffer, "SUBJECT: %s\r\n\r\n", subject);
  SocketSendString(socket, buffer);

  bool is_eof;
  while ((std::tie(is_eof, errno_info) = file.eof()),
         errno_info.is_success() && !is_eof) {
    file.gets(buffer);

    // A period on a line by itself would end the transmission
    if (!strcmp(buffer, ".\n") && !strcmp(buffer, ".\r\n")) continue;

    SocketSendString(socket, buffer);
  }

  SocketSendString(socket, "\r\n.\r\n");
  SocketWait(socket, "\n");
}

int main(int argc, char *argv[]) {
  const ParsedArgs args{ParseArgs(argc, argv)};

  using namespace source::windows;

  ScopedWinsockInitializer scoped_winsock{WinsockVersion::Version2_2};

  if (failed(scoped_winsock.error_code())) {
    Fail("can't initialize Winsock 2.2: %s.\n",
         make_windows_errno_info(scoped_winsock.error_code()).description);
  }

  // in verbose mode, printf all of the traffic
  if (args.is_verbose) SocketReport(DumpSocket);

  HSOCKET socket{SocketOpen(args.server_name, args.port_number)};

  if (socket) {
    SocketWait(socket, "\n");

    MailSendFile(socket, args.from, args.to, args.subject, args.file_name);

    SocketSendString(socket, "QUIT\r\n");
    SocketWait(socket, "\n");

    SocketClose(socket);
  } else {
    Fail("can't open socket to '%s:%hu'.\n", args.server_name,
         args.port_number);
  }

  return EXIT_SUCCESS;
}
