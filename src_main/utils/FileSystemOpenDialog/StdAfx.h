// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef AFX_STDAFX_H_
#define AFX_STDAFX_H_

#include "tier0/include/wchartypes.h"

// Exclude rarely-used stuff from Windows headers
#define VC_EXTRALEAN

// Nobody needs it.
#define NOMINMAX

#include <afxext.h>  // MFC extensions
#include <afxwin.h>  // MFC core and standard components

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
#include <afxcmn.h>  // MFC support for Windows Common Controls
#include <afxpriv.h>
#endif  // _AFX_NO_AFXCMN_SUPPORT

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before
// the previous line.

#endif  // !AFX_STDAFX_H_
