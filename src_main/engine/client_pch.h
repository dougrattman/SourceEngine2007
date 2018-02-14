// Copyright © 1996-2007, Valve Corporation, All rights reserved.
//
// Purpose: Main precompiled header for client files

#include "mathlib/mathlib.h"
#include "tier0/include/basetypes.h"
#include "tier0/include/icommandline.h"
#include "tier0/include/platform.h"
#include "tier0/include/vprof.h"
#include "tier1/convar.h"
#include "tier1/fmtstr.h"
#include "tier1/tier1.h"
#include "tier1/utlbuffer.h"
#include "tier1/utlsymbol.h"

#include "common.h"
#include "icliententitylist.h"
#include "iprediction.h"
#include "qlimits.h"

#include "cmd.h"
#include "protocol.h"
#include "render.h"
#include "screen.h"
#include "sysexternal.h"

#include "gl_shader.h"

#include "cdll_engine_int.h"
#include "cl_main.h"
#include "cl_pred.h"
#include "client.h"
#include "client_class.h"

#include "con_nprint.h"
#include "debugoverlay.h"
#include "demo.h"
#include "filesystem.h"
#include "filesystem_engine.h"
#include "host.h"
#include "host_state.h"
#include "proto_version.h"
#include "sys.h"

//#ifndef SWDS            these are all fine under linux now afaik
#include <vgui/IInput.h>
#include <vgui/ILocalize.h>
#include <vgui/IPanel.h>
#include "vgui/IScheme.h"
#include "vgui/ISurface.h"
#include "vgui/IVGui.h"
#include "vgui_basepanel.h"
#include "vgui_baseui_interface.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/CheckButton.h"
#include "vgui_controls/ComboBox.h"
#include "vgui_controls/Controls.h"
#include "vgui_controls/FileOpenDialog.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/ScrollBar.h"
#include "vgui_controls/TextEntry.h"
#include "vgui_controls/TreeView.h"
//#endif
