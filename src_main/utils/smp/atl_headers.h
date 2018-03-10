// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef ATL_HEADERS_H_
#define ATL_HEADERS_H_

#ifndef STRICT
#define STRICT
#endif

#ifndef _WIN32_WINNT
// Windows 10 features.
#define _WIN32_WINNT 0x0A00
#endif

#define _ATL_APARTMENT_THREADED

#include <atlbase.h>

extern CComModule _Module;

#include <afxres.h>
#include <atlcom.h>
#include <atlctl.h>
#include <atlhost.h>

#endif  // !ATL_HEADERS_H_
