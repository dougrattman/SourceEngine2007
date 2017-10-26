// Copyright © 1996-2007, Valve Corporation, All rights reserved.

// main precompiled header for client rendering code

#include "const.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/imesh.h"
#include "materialsystem/materialsystem_config.h"
#include "utlsymbol.h"

#include "cmodel_engine.h"
#include "convar.h"
#include "gl_cvars.h"
#include "gl_lightmap.h"
#include "gl_matsysiface.h"
#include "gl_model.h"
#include "gl_model_private.h"
#include "gl_rmain.h"
#include "gl_rsurf.h"
#include "gl_shader.h"
#include "r_local.h"
#include "render.h"
#include "sysexternal.h"
#include "view.h"
#include "worldsize.h"
