// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef ATTRIBUTEELEMENTCHOICELIST_H
#define ATTRIBUTEELEMENTCHOICELIST_H

#include "datamodel/dmehandle.h"
#include "dme_controls/inotifyui.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CDmElement;

//-----------------------------------------------------------------------------
// returns the choice string that AddElementsRecursively would have returned
//-----------------------------------------------------------------------------
const char *GetChoiceString(CDmElement *pElement);

//-----------------------------------------------------------------------------
// Recursively adds all elements of the specified type under pElement into the
// choice list
//-----------------------------------------------------------------------------
void AddElementsRecursively(CDmElement *pElement, ElementChoiceList_t &list,
                            const char *pElementType = NULL);

//-----------------------------------------------------------------------------
// Recursively adds all elements of the specified type under pElement into the
// vector
//-----------------------------------------------------------------------------
void AddElementsRecursively(CDmElement *pElement, DmeHandleVec_t &list,
                            const char *pElementType = NULL);

#endif  // ATTRIBUTEELEMENTCHOICELIST_H