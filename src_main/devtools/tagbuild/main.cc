// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include <process.h>
#include <cstdio>
#include <cstring>

#include "imysqlwrapper.h"
#include "interface.h"

const char *search = "VLV_INTERNAL                    ";

[[noreturn]] void printusage() {
  printf(
      "usage:  tagbuild engine.dll buildid <hostname database username "
      "password>\n");

  // Exit app
  exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
  if (argc != 3 && argc != 7) printusage();

  std::size_t id_size{strlen(argv[2])};

  if (id_size > 32) {
    fprintf(stderr, "id string '%s' is %i long, 32 is max.\n", argv[2],
            id_size);
    printusage();
  }

  // Open for reading and writing
  char error[96];
  FILE *fp;
  if (fopen_s(&fp, argv[1], "r+")) {
    strerror_s(error, errno);
    fprintf(stderr, "unable to open %s: %s.\n", argv[1], error);

    exit(EXIT_FAILURE);
  }

  fseek(fp, 0, SEEK_END);
  const long size = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  std::size_t searchlen = strlen(search);

  char already_tagged[32];

  memset(already_tagged, ' ', sizeof(already_tagged));
  already_tagged[std::size(already_tagged) - 1] = '\0';
  // Copies in the id, but doesn't copy the terminating '\0'
  memcpy(already_tagged, argv[2], strlen(argv[2]));

  bool found = false;
  char *buf = new char[size];
  if (buf) {
    fread(buf, sizeof(char), size, fp);

    // Now search
    char *start = buf;
    while (start - buf < (size - searchlen)) {
      if (!memcmp(start, search, searchlen - 1)) {
        std::ptrdiff_t pos = start - buf;

        printf("found placeholder at %td, writing '%s' into file.\n", pos,
               argv[2]);

        fseek(fp, pos, SEEK_SET);
        fwrite(argv[2], id_size, 1, fp);

        while (id_size < searchlen) {
          fputc(' ', fp);
          ++id_size;
        }

        found = true;
        break;
      } else if (!memcmp(start, already_tagged, searchlen - 1)) {
        std::ptrdiff_t pos = start - buf;

        printf("found tag at %td (%s).\n", pos, argv[2]);

        found = true;
        break;
      }

      ++start;
    }

    if (!found) {
      fprintf(stderr, "couldn't find search string '%s' in binary data.\n",
              search);

      exit(EXIT_FAILURE);
    }
  } else {
    fprintf(stderr, "unable to allocate %zu bytes for %s.\n", size, argv[1]);

    exit(EXIT_FAILURE);
  }

  fclose(fp);

  if (argc <= 3) {
    printf("skipping database insertion.\n");
    return EXIT_SUCCESS;
  }

  // Now connect to steamweb and update the engineaccess table
  CSysModule *sql = Sys_LoadModule("mysql_wrapper");

  if (sql) {
    CreateInterfaceFn factory = Sys_GetFactory(sql);

    if (factory) {
      auto *mysql = reinterpret_cast<IMySQL *>(
          factory(MYSQL_WRAPPER_VERSION_NAME, nullptr));

      if (mysql) {
        if (mysql->InitMySQL(argv[4], argv[3], argv[5], argv[6])) {
          char q[512];
          sprintf_s(q,
                    "insert into engineaccess (BuildIdentifier,AllowAccess) "
                    "values (\"%s\",1);",
                    argv[2]);

          int sql_result = mysql->Execute(q);

          if (sql_result != 0) {
            fprintf(stderr, "mysql query %s failed: %s.\n", q,
                    mysql->GetLastError());
          } else {
            printf("successfully added build identifier '%s' to %s: %s.\n",
                   argv[2], argv[3], argv[4]);
          }
        } else {
          fprintf(stderr, "can't init mysql: %s.\n", mysql->GetLastError());
        }

        mysql->Release();
        return EXIT_SUCCESS;
      }

      fprintf(stderr, "unable to connect via mysql_wrapper.\n");
      return EXIT_FAILURE;
    }

    fprintf(stderr,
            "unable to get factory from mysql_wrapper.dll, not updating access "
            "mysql table.");
    return EXIT_FAILURE;
  }

  fprintf(stderr,
          "Unable to load mysql_wrapper.dll, not updating access mysql "
          "table.");
  return EXIT_FAILURE;
}
