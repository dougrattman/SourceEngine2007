// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "mem.h"

#include <memory.h>
#include <cstdlib>
#include <cstring>

void *Mem_Malloc(size_t size) { return malloc(size); }

void *Mem_ZeroMalloc(size_t size) {
  void *p;

  p = malloc(size);
  memset((unsigned char *)p, 0, size);
  return p;
}

void *Mem_Realloc(void *memblock, size_t size) {
  return realloc(memblock, size);
}

void *Mem_Calloc(int num, size_t size) { return calloc(num, size); }

char *Mem_Strdup(const char *strSource) { return strdup(strSource); }

void Mem_Free(void *p) { free(p); }
