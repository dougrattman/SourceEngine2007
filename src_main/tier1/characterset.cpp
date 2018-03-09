// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "tier1/characterset.h"

#include <cstring>

 
#include "tier0/include/memdbgon.h"


// Purpose: builds a simple lookup table of a group of important characters
// Input  : *pParseGroup - pointer to the buffer for the group
//			*pGroupString - 0 terminated list of characters to
// flag

void CharacterSetBuild(characterset_t *pSetBuffer, const char *pszSetString) {
  size_t i = 0;

  // Test our pointers
  if (!pSetBuffer || !pszSetString) return;

  memset(pSetBuffer->set, 0, sizeof(pSetBuffer->set));

  while (pszSetString[i]) {
    pSetBuffer->set[pszSetString[i]] = 1;
    ++i;
  }
}
