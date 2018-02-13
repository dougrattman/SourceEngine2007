// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#include "client_pch.h"

#include "cbenchmark.h"

#ifdef IS_WINDOWS_PC
#include "base/include/windows/windows_light.h"

#include <winsock2.h>  // INADDR_ANY defn
#endif

#include "FindSteamServers.h"
#include "filesystem_engine.h"
#include "steam/steam_api.h"
#include "sv_uploaddata.h"
#include "sys.h"
#include "tier0/include/vcrmode.h"
#include "tier1/keyvalues.h"
#include "vstdlib/random.h"

#include "tier0/include/memdbgon.h"

#define DEFAULT_RESULTS_FOLDER "results"
#define DEFAULT_RESULTS_FILENAME "results.txt"

CBenchmarkResults g_BenchmarkResults;
extern ConVar host_framerate;
extern void GetMaterialSystemConfigForBenchmarkUpload(KeyValues *dataToUpload);

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CBenchmarkResults::CBenchmarkResults() {
  m_bIsTestRunning = false;
  m_szFilename[0] = 0;
  m_flStartTime = 0.0f;
  m_iStartFrame = 0;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CBenchmarkResults::IsBenchmarkRunning() { return m_bIsTestRunning; }

//-----------------------------------------------------------------------------
// Purpose: starts recording data
//-----------------------------------------------------------------------------
void CBenchmarkResults::StartBenchmark(const CCommand &args) {
  const char *pszFilename = DEFAULT_RESULTS_FILENAME;

  if (args.ArgC() > 1) {
    pszFilename = args[1];
  }

  // check path first
  if (!COM_IsValidPath(pszFilename)) {
    ConMsg("bench_start %s: invalid path.\n", pszFilename);
    return;
  }

  m_bIsTestRunning = true;

  SetResultsFilename(pszFilename);

  // set any necessary settings
  host_framerate.SetValue((float)(1.0f / host_state.interval_per_tick));

  // get the current frame and time
  m_iStartFrame = host_framecount;
  m_flStartTime = realtime;
}

//-----------------------------------------------------------------------------
// Purpose: writes out results to file
//-----------------------------------------------------------------------------
void CBenchmarkResults::StopBenchmark() {
  m_bIsTestRunning = false;

  // reset
  host_framerate.SetValue(0);

  // print out some stats
  int numticks = host_framecount - m_iStartFrame;
  float framerate = numticks / (realtime - m_flStartTime);
  Msg("Average framerate: %.2f\n", framerate);

  // work out where to write the file
  g_pFileSystem->CreateDirHierarchy(DEFAULT_RESULTS_FOLDER, "MOD");
  char szFilename[256];
  Q_snprintf(szFilename, sizeof(szFilename), "%s\\%s", DEFAULT_RESULTS_FOLDER,
             m_szFilename);

  // write out the data as keyvalues
  KeyValues *kv = new KeyValues("benchmark");
  kv->SetFloat("framerate", framerate);
  kv->SetInt("build", build_number());

  // get material system info
  GetMaterialSystemConfigForBenchmarkUpload(kv);

  // save
  kv->SaveToFile(g_pFileSystem, szFilename, "MOD");
  kv->deleteThis();
}

//-----------------------------------------------------------------------------
// Purpose: Sets which file the results will be written to
//-----------------------------------------------------------------------------
void CBenchmarkResults::SetResultsFilename(const char *pFilename) {
  Q_strncpy(m_szFilename, pFilename, sizeof(m_szFilename));
  Q_DefaultExtension(m_szFilename, ".txt", sizeof(m_szFilename));
}

//-----------------------------------------------------------------------------
// Purpose: uploads the most recent results to Steam
//-----------------------------------------------------------------------------
void CBenchmarkResults::Upload() {
#ifndef NO_STEAM
  if (!m_szFilename[0] || !SteamUtils()) return;
  uint32_t cserIP = 0;
  uint16_t cserPort = 0;
  while (cserIP == 0) {
    SteamUtils()->GetCSERIPPort(&cserIP, &cserPort);
    if (!cserIP) Sys_Sleep(10);
  }

  netadr_t netadr_CserIP(cserIP, cserPort);
  // upload
  char szFilename[256];
  Q_snprintf(szFilename, sizeof(szFilename), "%s\\%s", DEFAULT_RESULTS_FOLDER,
             m_szFilename);
  KeyValues *kv = new KeyValues("benchmark");
  if (kv->LoadFromFile(g_pFileSystem, szFilename, "MOD")) {
    // this sends the data to the Steam CSER
    UploadData(netadr_CserIP.ToString(), "benchmark", kv);
  }

  kv->deleteThis();
#endif
}

CON_COMMAND_F(
    bench_start,
    "Starts gathering of info. Arguments: filename to write results into",
    FCVAR_CHEAT) {
  GetBenchResultsMgr()->StartBenchmark(args);
}

CON_COMMAND_F(bench_end, "Ends gathering of info.", FCVAR_CHEAT) {
  GetBenchResultsMgr()->StopBenchmark();
}

CON_COMMAND_F(bench_upload,
              "Uploads most recent benchmark stats to the Valve servers.",
              FCVAR_CHEAT) {
  GetBenchResultsMgr()->Upload();
}
