// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: replaces the cl_*.cpp files with stubs

#include "client_pch.h"

#ifdef SWDS
#include "audio/public/soundservice.h"
#include "bspfile.h"  // dworldlight_t
#include "enginestats.h"
#include "hltvclientstate.h"
#include "tier1/convar.h"

ISoundServices *g_pSoundServices = NULL;
Vector listener_origin;

 
#include "tier0/include/memdbgon.h"

#define MAXPRINTMSG 4096

CEngineStats g_EngineStats;

ClientClass *g_pClientClassHead = NULL;

bool CL_IsHL2Demo() { return false; }

bool CL_IsPortalDemo() { return false; }

bool HandleRedirectAndDebugLog(const ch *msg);

void BeginLoadingUpdates(MaterialNonInteractiveMode_t mode) {}
void RefreshScreenIfNecessary() {}
void EndLoadingUpdates() {}

void Con_ColorPrintf(const Color &clr, const ch *fmt, ...) {
  va_list argptr;
  ch msg[MAXPRINTMSG];
  static bool inupdate;

  if (!host_initialized) return;

  va_start(argptr, fmt);
  Q_vsnprintf(msg, sizeof(msg), fmt, argptr);
  va_end(argptr);

  if (!HandleRedirectAndDebugLog(msg)) {
    return;
  }

  Msg("%s", msg);
}

void Con_NPrintf(int pos, const ch *fmt, ...) {
  va_list argptr;
  ch text[4096];
  va_start(argptr, fmt);
  Q_vsnprintf(text, sizeof(text), fmt, argptr);
  va_end(argptr);

  Msg("%s", text);
}

void SCR_UpdateScreen() {}

void SCR_EndLoadingPlaque() {}

void ClientDLL_FrameStageNotify(ClientFrameStage_t frameStage) {}

ClientClass *ClientDLL_GetAllClasses() { return g_pClientClassHead; }

#define LIGHT_MIN_LIGHT_VALUE 0.03f

float ComputeLightRadius(dworldlight_t *pLight, bool bIsHDR) {
  float flLightRadius = pLight->radius;
  if (flLightRadius == 0.0f) {
    // Compute the light range based on attenuation factors
    float flIntensity = sqrtf(DotProduct(pLight->intensity, pLight->intensity));
    if (pLight->quadratic_attn == 0.0f) {
      if (pLight->linear_attn == 0.0f) {
        // Infinite, but we're not going to draw it as such
        flLightRadius = 2000;
      } else {
        flLightRadius =
            (flIntensity / LIGHT_MIN_LIGHT_VALUE - pLight->constant_attn) /
            pLight->linear_attn;
      }
    } else {
      float a = pLight->quadratic_attn;
      float b = pLight->linear_attn;
      float c = pLight->constant_attn - flIntensity / LIGHT_MIN_LIGHT_VALUE;
      float discrim = b * b - 4 * a * c;
      if (discrim < 0.0f) {
        // Infinite, but we're not going to draw it as such
        flLightRadius = 2000;
      } else {
        flLightRadius = (-b + sqrtf(discrim)) / (2.0f * a);
        if (flLightRadius < 0) flLightRadius = 0;
      }
    }
  }

  return flLightRadius;
}

CClientState::CClientState() {}
CClientState::~CClientState() {}
void CClientState::ConnectionClosing(const ch *reason) {}
void CClientState::ConnectionCrashed(const ch *reason) {}
bool CClientState::ProcessConnectionlessPacket(netpacket_t *packet) {
  return false;
}
void CClientState::PacketStart(int incoming_sequence,
                               int outgoing_acknowledged) {}
void CClientState::PacketEnd() {}
void CClientState::FileRequested(const ch *fileName,
                                 unsigned int transferID) {}
void CClientState::Disconnect(bool showmainmenu) {}
void CClientState::FullConnect(netadr_t &adr) {}
bool CClientState::SetSignonState(int state, int count) { return false; }
void CClientState::SendClientInfo() {}
void CClientState::InstallStringTableCallback(ch const *tableName) {}
bool CClientState::InstallEngineStringTableCallback(ch const *tableName) {
  return false;
}
void CClientState::ReadEnterPVS(CEntityReadInfo &u) {}
void CClientState::ReadLeavePVS(CEntityReadInfo &u) {}
void CClientState::ReadDeltaEnt(CEntityReadInfo &u) {}
void CClientState::ReadPreserveEnt(CEntityReadInfo &u) {}
void CClientState::ReadDeletions(CEntityReadInfo &u) {}
const ch *CClientState::GetCDKeyHash() { return "123"; }
void CClientState::Clear() {}
bool CClientState::ProcessGameEvent(SVC_GameEvent *msg) { return true; }
bool CClientState::ProcessUserMessage(SVC_UserMessage *msg) { return true; }
bool CClientState::ProcessEntityMessage(SVC_EntityMessage *msg) { return true; }
bool CClientState::ProcessBSPDecal(SVC_BSPDecal *msg) { return true; }
bool CClientState::ProcessCrosshairAngle(SVC_CrosshairAngle *msg) {
  return true;
}
bool CClientState::ProcessFixAngle(SVC_FixAngle *msg) { return true; }
bool CClientState::ProcessVoiceData(SVC_VoiceData *msg) { return true; }
bool CClientState::ProcessVoiceInit(SVC_VoiceInit *msg) { return true; }
bool CClientState::ProcessSetPause(SVC_SetPause *msg) { return true; }
bool CClientState::ProcessClassInfo(SVC_ClassInfo *msg) { return true; }
bool CClientState::ProcessStringCmd(NET_StringCmd *msg) { return true; }
bool CClientState::ProcessServerInfo(SVC_ServerInfo *msg) { return true; }
bool CClientState::ProcessTick(NET_Tick *msg) { return true; }
bool CClientState::ProcessTempEntities(SVC_TempEntities *msg) { return true; }
bool CClientState::ProcessPacketEntities(SVC_PacketEntities *msg) {
  return true;
}
bool CClientState::ProcessSounds(SVC_Sounds *msg) { return true; }
bool CClientState::ProcessPrefetch(SVC_Prefetch *msg) { return true; }
float CClientState::GetTime() const { return 0.0f; }
void CClientState::FileDenied(const ch *fileName, unsigned int transferID) {}
void CClientState::FileReceived(const ch *fileName, unsigned int transferID) {
}
void CClientState::RunFrame() {}
void CClientState::ConsistencyCheck(bool bChanged) {}
bool CClientState::HookClientStringTable(ch const *tableName) {
  return false;
}

CClientState cl;

#endif
