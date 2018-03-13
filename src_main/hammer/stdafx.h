// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#if !defined(AFX_STDAFX_H__2871A74F_7D2F_4026_9DB0_DBACAFB3B7F5__INCLUDED_)
#define AFX_STDAFX_H__2871A74F_7D2F_4026_9DB0_DBACAFB3B7F5__INCLUDED_

// Windows 7 features.
#define _WIN32_WINNT 0x0501

#define NO_THREAD_LOCAL 1

#include "tier0/include/vprof.h"
#include "tier0/include/wchartypes.h"

// Exclude rarely-used stuff from Windows headers
#define VC_EXTRALEAN

#include <afxext.h>  // MFC extensions
#include <afxwin.h>  // MFC core and standard components
#include <process.h>

#ifndef _AFX_NO_OLE_SUPPORT
#include <afxdisp.h>   // MFC Automation classes
#include <afxodlgs.h>  // MFC OLE dialog classes
#include <afxole.h>    // MFC OLE classes
#endif                 // _AFX_NO_OLE_SUPPORT

#ifndef _AFX_NO_DB_SUPPORT
#include <afxdb.h>  // MFC ODBC database classes
#endif              // _AFX_NO_DB_SUPPORT

#ifndef _AFX_NO_DAO_SUPPORT
#include <afxdao.h>  // MFC DAO database classes
#endif               // _AFX_NO_DAO_SUPPORT

#include <afxdtctl.h>  // MFC support for Internet Explorer 4 Common Controls
#ifndef _AFX_NO_AFXCMN_SUPPORT
#include <afxcmn.h>  // MFC support for Windows 95 Common Controls
#include <afxpriv.h>
#include <fstream>
#endif  // _AFX_NO_AFXCMN_SUPPORT

#include "tier0/include/platform.h"

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before
// the previous line.

#endif  // !defined(AFX_STDAFX_H__2871A74F_7D2F_4026_9DB0_DBACAFB3B7F5__INCLUDED_)
