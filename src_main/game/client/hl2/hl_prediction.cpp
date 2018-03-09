// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: 
//
// $NoKeywords: $

#include "cbase.h"
#include "prediction.h"
#include "hl_movedata.h"
#include "c_basehlplayer.h"

 
#include "tier0/include/memdbgon.h"

static CHLMoveData g_HLMoveData;
CMoveData *g_pMoveData = &g_HLMoveData;

// Expose interface to engine
static CPrediction g_Prediction;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CPrediction, IPrediction, VCLIENT_PREDICTION_INTERFACE_VERSION, g_Prediction );

CPrediction *prediction = &g_Prediction;

