// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "tier0/include/icommandline.h"

#include <ctype.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "base/include/stdio_file_stream.h"
#include "tier0/include/dbg.h"
#include "tier0/include/platform.h"

#include "tier0/include/memdbgon.h"

namespace {
// Finds a string in another string with a case insensitive test.
ch *_stristr(ch *const string, const ch *substring) {
  if (!string || !substring) return nullptr;

  ch *letter = string;

  // Check the entire string.
  while (*letter != '\0') {
    // Skip over non-matches.
    if (tolower(*letter) == tolower(*substring)) {
      // Check for match
      ch const *match = letter + 1;
      ch const *test = substring + 1;

      while (*test != '\0') {
        // We've run off the end; don't bother.
        if (*match == '\0') return 0;

        if (tolower(*match) != tolower(*test)) break;

        ++match;
        ++test;
      }

      // Found a match!
      if (*test == '\0') return letter;
    }

    ++letter;
  }

  return nullptr;
}
}  // namespace

// Purpose: Implements ICommandLine
class CCommandLine : public ICommandLine {
 public:
  CCommandLine();
  virtual ~CCommandLine();

  // Implements ICommandLine
  void CreateCmdLine(const ch *command_line) override;
  void CreateCmdLine(i32 argc, ch **argv) override;
  const ch *GetCmdLine() const override;
  const ch *CheckParm(const ch *param,
                      const ch **param_value = nullptr) const override;

  void RemoveParm(const ch *param) override;
  void AppendParm(const ch *param, const ch *param_value) override;

  usize ParmCount() const override;
  usize FindParm(const ch *param) const override;
  const ch *GetParm(usize index) const override;

  const ch *ParmValue(const ch *param,
                      const ch *default_param_value = nullptr) const override;
  i32 ParmValue(const ch *param, i32 default_param_value) const override;
  f32 ParmValue(const ch *param, f32 default_param_value) const override;

 private:
  // Statically define maximum parameters count.
  static const usize kMaxParametersCount = 512;
  // Copy of actual command line
  ch *command_line_;
  // Real parameters count. Should be 0 <= params_count_ <= kMaxParametersCount.
  usize params_count_;
  // Pointers to each argument...
  ch *params_[kMaxParametersCount];

  // When the commandline contains @name, it reads the parameters from that
  // file.
  void LoadParametersFromFile(const ch *&file_name_start, ch *&destination,
                              isize max_destination_length,
                              bool is_file_name_in_quotes);
  // Parse command line.
  void ParseCommandLine();
  // Frees the command line arguments.
  void CleanUpParms();
  // Adds an argument.
  void AddArgument(const ch *begin_argument, const ch *end_argument);

  CCommandLine(CCommandLine &command_line) = delete;
  CCommandLine &operator=(CCommandLine &command_line) = delete;
};

// Instance singleton and expose interface to rest of code.
static CCommandLine g_command_line;
ICommandLine *CommandLine() { return &g_command_line; }

CCommandLine::CCommandLine() : command_line_{nullptr}, params_count_{0} {
  memset(params_, 0, sizeof(params_));
}

CCommandLine::~CCommandLine() {
  CleanUpParms();
  delete[] command_line_;
}

void CCommandLine::LoadParametersFromFile(const ch *&file_name_start,
                                          ch *&destination,
                                          isize max_destination_length,
                                          bool is_file_name_in_quotes) {
  if (max_destination_length < 3) return;

  ch *destination_start = destination;

  // Skip the @ sign.
  file_name_start++;

  // Suck out the file name.
  ch file_name[SOURCE_MAX_PATH];
  ch *file_name_output = file_name;
  ch terminating_char = is_file_name_in_quotes ? '\"' : ' ';

  while (*file_name_start && *file_name_start != terminating_char) {
    *file_name_output++ = *file_name_start++;
    if (file_name_output - file_name >= SOURCE_MAX_PATH - 1) break;
  }

  *file_name_output = '\0';

  // Skip the space after the file name.
  if (*file_name_start) file_name_start++;

  // Now read in parameters from file
  auto [file, errno_info] =
      source::stdio_file_stream_factory::open(file_name, "r");
  bool is_read_success{errno_info.is_success()};
  i32 c = 0;

  if (is_read_success) {
    std::tie(c, errno_info) = file.getc();
    is_read_success = errno_info.is_success();
  }

  if (is_read_success) {
    while (is_read_success && c != EOF) {
      // Turn return characters into spaces.
      if (c == '\n') c = ' ';

      *destination++ = c;

      // Don't go past the end, and allow for our terminating space character
      // AND a terminating 0 character.
      if (destination - destination_start >= max_destination_length - 2) break;

      // Get the next character, if there are more.
      std::tie(c, errno_info) = file.getc();
      is_read_success = errno_info.is_success();
    }

    // Add a terminating space character.
    *destination++ = ' ';
  }

  if (!is_read_success) {
    fprintf(stderr, "Can't read cmd line from file '%s': %s.\n", file_name,
            errno_info.description);
  }
}

void CCommandLine::CreateCmdLine(i32 argc, ch **argv) {
  ch command_line[2048];
  command_line[0] = '\0';

  constexpr usize kMaxCommandLineChars = sizeof(command_line) - 1;
  command_line[kMaxCommandLineChars] = '\0';

  for (i32 i = 0; i < argc; ++i) {
    strcat_s(command_line, "\"");
    strcat_s(command_line, argv[i]);
    strcat_s(command_line, "\"");
    strcat_s(command_line, " ");
  }

  CreateCmdLine(command_line);
}

// If you pass in a @filename, then the routine will read settings from a file
// instead of the command line.
void CCommandLine::CreateCmdLine(const ch *command_line) {
  delete[] command_line_;

  ch full_command_line_buffer[4096];
  full_command_line_buffer[0] = '\0';

  ch *full_command_line = full_command_line_buffer;
  const ch *source_command_line = command_line;

  bool is_in_quotes = false;
  const ch *quotes_start = nullptr;

  while (*source_command_line) {
    // Is this an unslashed quote?
    if (*source_command_line == '"') {
      if (source_command_line == command_line ||
          (source_command_line[-1] != '/' && source_command_line[-1] != '\\')) {
        is_in_quotes = !is_in_quotes;
        quotes_start = source_command_line + 1;
      }
    }

    if (*source_command_line == '@') {
      if (source_command_line == command_line ||
          (!is_in_quotes && isspace(source_command_line[-1])) ||
          (is_in_quotes && source_command_line == quotes_start)) {
        LoadParametersFromFile(
            source_command_line, full_command_line,
            sizeof(full_command_line_buffer) -
                (full_command_line - full_command_line_buffer),
            is_in_quotes);
        continue;
      }
    }

    // Don't go past the end.
    if (full_command_line - full_command_line_buffer >=
        sizeof(full_command_line_buffer) - 1)
      break;

    *full_command_line++ = *source_command_line++;
  }

  *full_command_line = '\0';

  const usize command_line_length = strlen(full_command_line_buffer) + 1;
  command_line_ = new ch[command_line_length];
  memcpy(command_line_, full_command_line_buffer,
         command_line_length * sizeof(full_command_line_buffer[0]));

  ParseCommandLine();
}

// Purpose: Remove specified string (and any args attached to it) from command
// line.
void CCommandLine::RemoveParm(const ch *param) {
  if (!command_line_) return;

  // Search for first occurrence of param.
  usize n;
  const isize param_length = strlen(param);
  ch *command_line = command_line_;

  while (*command_line) {
    const usize command_line_length = strlen(command_line);

    ch *param_in_comand_line = _stristr(command_line, param);
    if (!param_in_comand_line) break;

    ch *next_param = param_in_comand_line + 1;
    while (*next_param && *next_param != ' ') ++next_param;

    if (next_param - param_in_comand_line > param_length) {
      command_line = next_param;
      continue;
    }

    while (*next_param && *next_param != '-' && *next_param != '+')
      ++next_param;

    if (*next_param) {
      // We are either at the end of the string, or at the next param. Just
      // chop out the current param.
      n = command_line_length -
          (next_param - command_line);  // # of characters after this param.
      memcpy(param_in_comand_line, next_param, n);

      param_in_comand_line[n] = '\0';
    } else {
      // Clear out rest of string.
      n = next_param - param_in_comand_line;
      memset(param_in_comand_line, 0, n);
    }
  }

  // Strip and trailing ' ' characters left over.
  while (true) {
    const usize length = strlen(command_line_);
    if (length == 0 || command_line_[length - 1] != ' ') break;

    command_line_[length - 1] = '\0';
  }

  ParseCommandLine();
}

void CCommandLine::AppendParm(const ch *param, const ch *value) {
  usize param_length = strlen(param);

  // Values + leading space character.
  if (value) param_length += strlen(value) + 1;
  ++param_length;  // Terminal 0;

  if (!command_line_) {
    command_line_ = new ch[param_length];
    strcpy_s(command_line_, param_length, param);

    if (value) {
      strcat_s(command_line_, param_length, " ");
      strcat_s(command_line_, param_length, value);
    }

    ParseCommandLine();
    return;
  }

  // Remove any remnants from the current command line.
  RemoveParm(param);

  param_length += strlen(command_line_) + 1 + 1;

  ch *command_line = new ch[param_length];
  memset(command_line, 0, param_length);

  strcpy_s(command_line, param_length,
           command_line_);                    // Copy old command line.
  strcat_s(command_line, param_length, " ");  // Put in a space.
  strcat_s(command_line, param_length, param);

  if (value) {
    strcat_s(command_line, param_length, " ");
    strcat_s(command_line, param_length, value);
  }

  // Kill off the old one
  delete[] command_line_;

  // Point at the new command line.
  command_line_ = command_line;

  ParseCommandLine();
}

const ch *CCommandLine::GetCmdLine() const { return command_line_; }

const ch *CCommandLine::CheckParm(const ch *param,
                                  const ch **param_value) const {
  if (param_value) *param_value = nullptr;

  const usize param_index = FindParm(param);
  if (param_index == 0) return nullptr;

  if (param_value) {
    *param_value =
        param_index + 1 < params_count_ ? params_[param_index + 1] : nullptr;
  }

  return params_[param_index];
}

void CCommandLine::AddArgument(const ch *arg_start, const ch *arg_end) {
  if (arg_end == arg_start) return;

  if (params_count_ >= kMaxParametersCount) {
    Error("CCommandLine::AddArgument: exceeded %zu parameters.",
          kMaxParametersCount);
    return;
  }

  const usize length = arg_end - arg_start + 1;
  params_[params_count_] = new ch[length];
  memcpy(params_[params_count_], arg_start, length - 1);
  params_[params_count_][length - 1] = 0;

  ++params_count_;
}

void CCommandLine::ParseCommandLine() {
  CleanUpParms();
  if (!command_line_) return;

  const ch *command_line = command_line_;
  while (*command_line && isspace(*command_line)) ++command_line;

  bool is_in_quotes = false;
  const ch *first_letter = nullptr;

  for (; *command_line; ++command_line) {
    if (is_in_quotes) {
      if (*command_line != '\"') continue;

      AddArgument(first_letter, command_line);

      first_letter = nullptr;
      is_in_quotes = false;
      continue;
    }

    // Haven't started a word yet.
    if (!first_letter) {
      if (*command_line == '\"') {
        is_in_quotes = true;
        first_letter = command_line + 1;
        continue;
      }

      if (isspace(*command_line)) continue;

      first_letter = command_line;
      continue;
    }

    // Here, we're in the middle of a word. Look for the end of it.
    if (isspace(*command_line)) {
      AddArgument(first_letter, command_line);
      first_letter = nullptr;
    }
  }

  if (first_letter) {
    AddArgument(first_letter, command_line);
  }
}

void CCommandLine::CleanUpParms() {
  for (usize i = 0; i < params_count_; ++i) {
    delete[] params_[i];
    params_[i] = nullptr;
  }
  params_count_ = 0;
}

usize CCommandLine::ParmCount() const { return params_count_; }

usize CCommandLine::FindParm(const ch *psz) const {
  // Start at 1 so as to not search the exe name.
  for (usize i = 1; i < params_count_; ++i) {
    if (!_stricmp(psz, params_[i])) return i;
  }
  return 0;
}

const ch *CCommandLine::GetParm(usize index) const {
  Assert(index < params_count_);
  return index < params_count_ ? params_[index] : '\0';
}

const ch *CCommandLine::ParmValue(const ch *param,
                                  const ch *param_default_value) const {
  const usize param_index = FindParm(param);
  if (param_index == 0 || param_index == params_count_ - 1)
    return param_default_value;

  // Probably another command line parameter instead of a valid arg if it starts
  // with '+' or '-'.
  if (params_[param_index + 1][0] == '-' || params_[param_index + 1][0] == '+')
    return param_default_value;

  return params_[param_index + 1];
}

i32 CCommandLine::ParmValue(const ch *param, i32 param_default_value) const {
  const usize param_index = FindParm(param);
  if (param_index == 0 || param_index == params_count_ - 1)
    return param_default_value;

  // Probably another cmdline parameter instead of a valid arg if it starts with
  // '+' or '-'.
  if (params_[param_index + 1][0] == '-' || params_[param_index + 1][0] == '+')
    return param_default_value;

  return atoi(params_[param_index + 1]);
}

f32 CCommandLine::ParmValue(const ch *param, f32 param_default_value) const {
  const usize param_index = FindParm(param);
  if ((param_index == 0) || (param_index == params_count_ - 1))
    return param_default_value;

  // Probably another cmdline parameter instead of a valid arg if it starts with
  // '+' or '-'.
  if (params_[param_index + 1][0] == '-' || params_[param_index + 1][0] == '+')
    return param_default_value;

  return atof(params_[param_index + 1]);
}
