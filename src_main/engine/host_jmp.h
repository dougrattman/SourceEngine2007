// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#ifndef HOST_JMP_H
#define HOST_JMP_H

#include <setjmp.h>

extern jmp_buf host_abortserver;
extern jmp_buf host_enddemo;

#endif
