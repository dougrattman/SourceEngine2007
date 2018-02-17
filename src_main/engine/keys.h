// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef KEYS_H
#define KEYS_H

#include "inputsystem/ButtonCode.h"

class CUtlBuffer;
struct InputEvent_t;

void Key_Event(const InputEvent_t &event);

void Key_Init(void);
void Key_Shutdown(void);
void Key_WriteBindings(CUtlBuffer &buf);
int Key_CountBindings(void);
void Key_SetBinding(ButtonCode_t code, const char *pBinding);
const char *Key_BindingForKey(ButtonCode_t code);
const char *Key_NameForBinding(const char *pBinding);

void Key_StartTrapMode(void);
bool Key_CheckDoneTrapping(ButtonCode_t &key);

#endif  // KEYS_H
