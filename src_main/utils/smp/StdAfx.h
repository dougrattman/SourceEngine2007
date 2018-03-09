// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef STDAFX_H_
#define STDAFX_H_

#ifndef STRICT
#define STRICT
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif

#define _ATL_APARTMENT_THREADED

#include <atlbase.h>

extern CComModule _Module;

#include <afxres.h>
#include <atlcom.h>
#include <atlctl.h>
#include <atlhost.h>

#endif  // !STDAFX_H_
