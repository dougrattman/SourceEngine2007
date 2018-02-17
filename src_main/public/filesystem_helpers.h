// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef FILESYSTEM_HELPERS_H
#define FILESYSTEM_HELPERS_H

// Call until it returns NULL. Each time you call it, it will parse out a token.
struct characterset_t;
const char* ParseFile(const char* pFileBytes, char* pToken, bool* pWasQuoted,
                      characterset_t* pCharSet = nullptr);
char* ParseFile(char* pFileBytes, char* pToken,
                bool* pWasQuoted);  // (same exact thing as the const version)

#endif  // FILESYSTEM_HELPERS_H
