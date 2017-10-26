// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#include "pch_tier0.h"

#include "tier0/icommandline.h"

#include <ctype.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef _LINUX
#include <climits>
#define _MAX_PATH PATH_MAX
#endif

#include "tier0/dbg.h"

#include "tier0/memdbgon.h"

namespace {
// Finds a string in another string with a case insensitive test.
char *_stristr(char *const string, const char *substring) {
  if (!string || !substring) return nullptr;

  char *letter = string;

  // Check the entire string.
  while (*letter != '\0') {
    // Skip over non-matches.
    if (tolower(*letter) == tolower(*substring)) {
      // Check for match
      char const *match = letter + 1;
      char const *test = substring + 1;

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
  void CreateCmdLine(const char *command_line) override;
  void CreateCmdLine(int argc, char **argv) override;
  const char *GetCmdLine() const override;
  const char *CheckParm(const char *param,
                        const char **param_value = nullptr) const override;

  void RemoveParm(const char *param) override;
  void AppendParm(const char *param, const char *param_value) override;

  size_t ParmCount() const override;
  size_t FindParm(const char *param) const override;
  const char *GetParm(size_t index) const override;

  const char *ParmValue(const char *param, const char *default_param_value =
                                               nullptr) const override;
  int ParmValue(const char *param, int default_param_value) const override;
  float ParmValue(const char *param, float default_param_value) const override;

 private:
  // Statically define maximum parameters count.
  static const size_t kMaxParametersCount = 512;
  // Copy of actual command line
  char *command_line_;
  // Real parameters count. Should be 0 <= params_count_ <= kMaxParametersCount.
  size_t params_count_;
  // Pointers to each argument...
  char *params_[kMaxParametersCount];

  // When the commandline contains @name, it reads the parameters from that
  // file.
  void LoadParametersFromFile(const char *&file_name_start, char *&destination,
                              ptrdiff_t max_destination_length,
                              bool is_file_name_in_quotes);
  // Parse command line.
  void ParseCommandLine();
  // Frees the command line arguments.
  void CleanUpParms();
  // Adds an argument.
  void AddArgument(const char *begin_argument, const char *end_argument);

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

void CCommandLine::LoadParametersFromFile(const char *&file_name_start,
                                          char *&destination,
                                          ptrdiff_t max_destination_length,
                                          bool is_file_name_in_quotes) {
  if (max_destination_length < 3) return;

  char *destination_start = destination;

  // Skip the @ sign.
  file_name_start++;

  // Suck out the file name.
  char file_name[_MAX_PATH];
  char *file_name_output = file_name;
  char terminating_char = is_file_name_in_quotes ? '\"' : ' ';

  while (*file_name_start && *file_name_start != terminating_char) {
    *file_name_output++ = *file_name_start++;
    if (file_name_output - file_name >= _MAX_PATH - 1) break;
  }

  *file_name_output = '\0';

  // Skip the space after the file name.
  if (*file_name_start) file_name_start++;

  // Now read in parameters from file
  FILE *file = fopen(file_name, "r");
  if (file) {
    int c = fgetc(file);

    while (c != EOF) {
      // Turn return characters into spaces.
      if (c == '\n') c = ' ';

      *destination++ = c;

      // Don't go past the end, and allow for our terminating space character
      // AND a terminating 0 character.
      if (destination - destination_start >= max_destination_length - 2) break;

      // Get the next character, if there are more.
      c = fgetc(file);
    }

    // Add a terminating space character.
    *destination++ = ' ';

    fclose(file);
  } else {
    fprintf(stderr, "Parameter file '%s' not found, skipping.\n", file_name);
  }
}

void CCommandLine::CreateCmdLine(int argc, char **argv) {
  char command_line[2048];
  command_line[0] = '\0';

  constexpr size_t kMaxCommandLineChars = sizeof(command_line) - 1;
  command_line[kMaxCommandLineChars] = '\0';

  for (int i = 0; i < argc; ++i) {
    strcat_s(command_line, "\"");
    strcat_s(command_line, argv[i]);
    strcat_s(command_line, "\"");
    strcat_s(command_line, " ");
  }

  CreateCmdLine(command_line);
}

// If you pass in a @filename, then the routine will read settings from a file
// instead of the command line.
void CCommandLine::CreateCmdLine(const char *command_line) {
  delete[] command_line_;

  char full_command_line_buffer[4096];
  full_command_line_buffer[0] = '\0';

  char *full_command_line = full_command_line_buffer;
  const char *source_command_line = command_line;

  bool is_in_quotes = false;
  const char *quotes_start = nullptr;

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

  const size_t command_line_length = strlen(full_command_line_buffer) + 1;
  command_line_ = new char[command_line_length];
  memcpy(command_line_, full_command_line_buffer,
         command_line_length * sizeof(full_command_line_buffer[0]));

  ParseCommandLine();
}

// Purpose: Remove specified string (and any args attached to it) from command
// line.
void CCommandLine::RemoveParm(const char *param) {
  if (!command_line_) return;

  // Search for first occurrence of param.
  size_t n;
  const ptrdiff_t param_length = strlen(param);
  char *command_line = command_line_;

  while (*command_line) {
    const size_t command_line_length = strlen(command_line);

    char *param_in_comand_line = _stristr(command_line, param);
    if (!param_in_comand_line) break;

    char *next_param = param_in_comand_line + 1;
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
    const size_t length = strlen(command_line_);
    if (length == 0 || command_line_[length - 1] != ' ') break;

    command_line_[length - 1] = '\0';
  }

  ParseCommandLine();
}

void CCommandLine::AppendParm(const char *param, const char *value) {
  size_t param_length = strlen(param);

  // Values + leading space character.
  if (value) param_length += strlen(value) + 1;
  ++param_length;  // Terminal 0;

  if (!command_line_) {
    command_line_ = new char[param_length];
    strcpy(command_line_, param);

    if (value) {
      strcat(command_line_, " ");
      strcat(command_line_, value);
    }

    ParseCommandLine();
    return;
  }

  // Remove any remnants from the current command line.
  RemoveParm(param);

  param_length += strlen(command_line_) + 1 + 1;

  char *command_line = new char[param_length];
  memset(command_line, 0, param_length);

  strcpy(command_line, command_line_);  // Copy old command line.
  strcat(command_line, " ");            // Put in a space.
  strcat(command_line, param);

  if (value) {
    strcat(command_line, " ");
    strcat(command_line, value);
  }

  // Kill off the old one
  delete[] command_line_;

  // Point at the new command line.
  command_line_ = command_line;

  ParseCommandLine();
}

const char *CCommandLine::GetCmdLine() const { return command_line_; }

const char *CCommandLine::CheckParm(const char *param,
                                    const char **param_value) const {
  if (param_value) *param_value = nullptr;

  const size_t param_index = FindParm(param);
  if (param_index == 0) return nullptr;

  if (param_value) {
    *param_value =
        param_index + 1 < params_count_ ? params_[param_index + 1] : nullptr;
  }

  return params_[param_index];
}

void CCommandLine::AddArgument(const char *arg_start, const char *arg_end) {
  if (arg_end == arg_start) return;

  if (params_count_ >= kMaxParametersCount) {
    Error("CCommandLine::AddArgument: exceeded %zu parameters.",
          kMaxParametersCount);
    return;
  }

  const size_t length = arg_end - arg_start + 1;
  params_[params_count_] = new char[length];
  memcpy(params_[params_count_], arg_start, length - 1);
  params_[params_count_][length - 1] = 0;

  ++params_count_;
}

void CCommandLine::ParseCommandLine() {
  CleanUpParms();
  if (!command_line_) return;

  const char *command_line = command_line_;
  while (*command_line && isspace(*command_line)) ++command_line;

  bool is_in_quotes = false;
  const char *first_letter = nullptr;

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
  for (size_t i = 0; i < params_count_; ++i) {
    delete[] params_[i];
    params_[i] = nullptr;
  }
  params_count_ = 0;
}

size_t CCommandLine::ParmCount() const { return params_count_; }

size_t CCommandLine::FindParm(const char *psz) const {
  // Start at 1 so as to not search the exe name.
  for (size_t i = 1; i < params_count_; ++i) {
    if (!_stricmp(psz, params_[i])) return i;
  }
  return 0;
}

const char *CCommandLine::GetParm(size_t index) const {
  Assert(index < params_count_);
  return index < params_count_ ? params_[index] : '\0';
}

const char *CCommandLine::ParmValue(const char *param,
                                    const char *param_default_value) const {
  const size_t param_index = FindParm(param);
  if (param_index == 0 || param_index == params_count_ - 1)
    return param_default_value;

  // Probably another command line parameter instead of a valid arg if it starts
  // with '+' or '-'.
  if (params_[param_index + 1][0] == '-' || params_[param_index + 1][0] == '+')
    return param_default_value;

  return params_[param_index + 1];
}

int CCommandLine::ParmValue(const char *param, int param_default_value) const {
  const size_t param_index = FindParm(param);
  if (param_index == 0 || param_index == params_count_ - 1)
    return param_default_value;

  // Probably another cmdline parameter instead of a valid arg if it starts with
  // '+' or '-'.
  if (params_[param_index + 1][0] == '-' || params_[param_index + 1][0] == '+')
    return param_default_value;

  return atoi(params_[param_index + 1]);
}

float CCommandLine::ParmValue(const char *param,
                              float param_default_value) const {
  const size_t param_index = FindParm(param);
  if ((param_index == 0) || (param_index == params_count_ - 1))
    return param_default_value;

  // Probably another cmdline parameter instead of a valid arg if it starts with
  // '+' or '-'.
  if (params_[param_index + 1][0] == '-' || params_[param_index + 1][0] == '+')
    return param_default_value;

  return atof(params_[param_index + 1]);
}
