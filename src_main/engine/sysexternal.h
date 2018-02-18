// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SYSEXTERNAL_H
#define SYSEXTERNAL_H

// an error will cause the entire program to exit
[[noreturn]] void Sys_Error(const char *format, ...);

#endif  // SYSEXTERNAL_H
