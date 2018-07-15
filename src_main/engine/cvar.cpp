// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "cvar.h"

#include "gl_cvars.h"

#include <ctype.h>
#include "GameEventManager.h"
#include "client.h"
#include "demo.h"
#include "filesystem.h"
#include "filesystem_engine.h"
#include "netmessages.h"
#include "server.h"
#include "sv_main.h"
#include "tier1/convar.h"

#ifdef OS_POSIX
#include <wctype.h>
#endif

#ifndef SWDS
#include <vgui/ILocalize.h>
#include <vgui_controls/Controls.h>
#endif

#include "tier0/include/memdbgon.h"

static CCvarUtilities g_CvarUtilities;
CCvarUtilities *cv = &g_CvarUtilities;

// Purpose: Update clients/server when FCVAR_REPLICATED etc vars change
static void ConVarNetworkChangeCallback(IConVar *con_var, const char *old_value,
                                        f32 old_value_f32) {
  ConVarRef var(con_var);

  if (!old_value) {
    if (var.GetFloat() == old_value_f32) return;
  } else {
    if (!Q_strcmp(var.GetString(), old_value)) return;
  }

  if (var.IsFlagSet(FCVAR_USERINFO)) {
    // Are we not a server, but a client and have a change?
    if (cl.IsConnected()) {
      // send changed cvar to server
      NET_SetConVar convar(var.GetName(), var.GetString());
      cl.m_NetChannel->SendNetMsg(convar);
    }
  }

  // Log changes to server variables

  // Print to clients
  if (var.IsFlagSet(FCVAR_NOTIFY)) {
    IGameEvent *event = g_GameEventManager.CreateEvent("server_cvar");

    if (event) {
      event->SetString("cvarname", var.GetName());

      if (var.IsFlagSet(FCVAR_PROTECTED)) {
        event->SetString("cvarvalue", "***PROTECTED***");
      } else {
        event->SetString("cvarvalue", var.GetString());
      }

      g_GameEventManager.FireEvent(event);
    }
  }

  // Force changes down to clients (if running server)
  if (var.IsFlagSet(FCVAR_REPLICATED) && sv.IsActive()) {
    SV_ReplicateConVarChange(static_cast<ConVar *>(con_var), var.GetString());
  }
}

// Implementation of the ICvarQuery interface
class CCvarQuery : public CBaseAppSystem<ICvarQuery> {
 public:
  bool Connect(CreateInterfaceFn factory) override {
    ICvar *c_var = (ICvar *)factory(CVAR_INTERFACE_VERSION, 0);
    if (!c_var) return false;

    c_var->InstallCVarQuery(this);
    return true;
  }

  InitReturnVal_t Init() override {
    // If the value has changed, notify clients/server based on ConVar flags.
    // NOTE: this will only happen for non-FCVAR_NEVER_AS_STRING vars.
    // Also, this happened in SetDirect for older clients that don't have the
    // callback interface.
    g_pCVar->InstallGlobalChangeCallback(ConVarNetworkChangeCallback);
    return INIT_OK;
  }

  void Shutdown() override {
    g_pCVar->RemoveGlobalChangeCallback(ConVarNetworkChangeCallback);
  }

  void *QueryInterface(const char *pInterfaceName) override {
    if (!_stricmp(pInterfaceName, CVAR_QUERY_INTERFACE_VERSION))
      return implicit_cast<ICvarQuery *>(this);

    return nullptr;
  }

  // Purpose: Returns true if the commands can be aliased to one another
  //  Either game/client .dll shared with engine,
  //  or game and client dll shared and marked FCVAR_REPLICATED
  bool AreConVarsLinkable(const ConVar *child, const ConVar *parent) override {
    // Both parent and child must be marked replicated for this to work
    const bool is_replicated_child = child->IsFlagSet(FCVAR_REPLICATED);
    const bool is_replicated_parent = parent->IsFlagSet(FCVAR_REPLICATED);

    char sz[512];

    if (is_replicated_child && is_replicated_parent) {
      // Never on protected vars
      if (child->IsFlagSet(FCVAR_PROTECTED) ||
          parent->IsFlagSet(FCVAR_PROTECTED)) {
        sprintf_s(sz, "FCVAR_REPLICATED can't also be FCVAR_PROTECTED (%s)\n",
                  child->GetName());
        ConMsg(sz);
        return false;
      }

      // Only on ConVars
      if (child->IsCommand() || parent->IsCommand()) {
        sprintf_s(sz, "FCVAR_REPLICATED not valid on ConCommands (%s)\n",
                  child->GetName());
        ConMsg(sz);
        return false;
      }

      // One must be in client .dll and the other in the game .dll, or both in
      // the engine
      if (child->IsFlagSet(FCVAR_GAMEDLL) &&
          !parent->IsFlagSet(FCVAR_CLIENTDLL)) {
        sprintf_s(sz,
                  "For FCVAR_REPLICATED, ConVar must be defined in client and "
                  "game .dlls (%s)\n",
                  child->GetName());
        ConMsg(sz);
        return false;
      }

      if (child->IsFlagSet(FCVAR_CLIENTDLL) &&
          !parent->IsFlagSet(FCVAR_GAMEDLL)) {
        sprintf_s(sz,
                  "For FCVAR_REPLICATED, ConVar must be defined in client and "
                  "game .dlls (%s)\n",
                  child->GetName());
        ConMsg(sz);
        return false;
      }

      // Allowable
      return true;
    }

    // Otherwise need both to allow linkage
    if (is_replicated_child || is_replicated_parent) {
      sprintf_s(sz,
                "Both ConVars must be marked FCVAR_REPLICATED for linkage to "
                "work (%s)\n",
                child->GetName());
      ConMsg(sz);
      return false;
    }

    if (parent->IsFlagSet(FCVAR_CLIENTDLL)) {
      sprintf_s(sz, "Parent cvar in client.dll not allowed (%s)\n",
                child->GetName());
      ConMsg(sz);
      return false;
    }

    if (parent->IsFlagSet(FCVAR_GAMEDLL)) {
      sprintf_s(sz, "Parent cvar in server.dll not allowed (%s)\n",
                child->GetName());
      ConMsg(sz);
      return false;
    }

    return true;
  }
};

static CCvarQuery s_CvarQuery;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CCvarQuery, ICvarQuery,
                                  CVAR_QUERY_INTERFACE_VERSION, s_CvarQuery);

// CVar utilities begins here
static bool IsAllSpaces(const wch *str) {
  const wch *p = str;
  while (p && *p) {
    if (!iswspace(*p)) return false;

    ++p;
  }

  return true;
}

void CCvarUtilities::SetDirect(ConVar *var, const char *value) {
  char szNew[1024];

  // Bail early if we're trying to set a FCVAR_USERINFO cvar on a dedicated
  // server
  if (var->IsFlagSet(FCVAR_USERINFO)) {
    if (sv.IsDedicated()) return;
  }

  const char *pszValue = value;
  // This cvar's string must only contain printable characters.
  // Strip out any other crap.
  // We'll fill in "empty" if nothing is left
  if (var->IsFlagSet(FCVAR_PRINTABLEONLY)) {
    wch unicode[512];
    usize converted_chars_num;

#ifndef SWDS
    if (sv.IsDedicated()) {
      // Dedicated servers don't have g_pVGuiLocalize, so fall back
      mbstowcs_s(&converted_chars_num, unicode, pszValue, std::size(unicode));
    } else {
      g_pVGuiLocalize->ConvertANSIToUnicode(pszValue, unicode, sizeof(unicode));
    }
#else
    mbstowcs(&converted_chars_num, unicode, pszValue, std::size(unicode));
#endif

    wch newUnicode[512];
    // Clear out new string
    newUnicode[0] = L'\0';

    const wch *pS = unicode;
    wch *pD = newUnicode;

    // Step through the string, only copying back in characters that are
    // printable
    while (*pS) {
      if (iswcntrl(*pS) || *pS == '~') {
        pS++;
        continue;
      }

      *pD++ = *pS++;
    }

    // Terminate the new string
    *pD = L'\0';

    // If it's empty or all spaces, then insert a marker string
    if (!wcslen(newUnicode) || IsAllSpaces(newUnicode)) {
      wcscpy_s(newUnicode, L"#empty");
    }

    usize converted_chars_num2;
#ifndef SWDS
    if (sv.IsDedicated()) {
      wcstombs_s(&converted_chars_num2, szNew, newUnicode, sizeof(szNew));
    } else {
      g_pVGuiLocalize->ConvertUnicodeToANSI(newUnicode, szNew, sizeof(szNew));
    }
#else
    wcstombs_s(&converted_chars_num2, szNew, newUnicode, sizeof(szNew));
#endif
    // Point the value here.
    pszValue = szNew;
  }

  if (var->IsFlagSet(FCVAR_NEVER_AS_STRING)) {
    var->SetValue((f32)atof(pszValue));
  } else {
    var->SetValue(pszValue);
  }
}

// If you are changing this, please take a look at IsValidToggleCommand()
bool CCvarUtilities::IsCommand(const CCommand &args) {
  int c = args.ArgC();
  if (c == 0) return false;

  // check variables
  ConVar *v = g_pCVar->FindVar(args[0]);
  if (!v) return false;

  // NOTE: Not checking for 'HIDDEN' here so we can actually set hidden convars
  if (v->IsFlagSet(FCVAR_DEVELOPMENTONLY)) return false;

  // perform a variable print or set
  if (c == 1) {
    ConVar_PrintDescription(v);
    return true;
  }

  if (v->IsFlagSet(FCVAR_SPONLY)) {
#ifndef SWDS
    // Connected to server?
    if (cl.IsConnected()) {
      // Is it not a single player game?
      if (cl.m_nMaxClients > 1) {
        ConMsg("Can't set %s in multiplayer\n", v->GetName());
        return true;
      }
    }
#endif
  }

  if (v->IsFlagSet(FCVAR_NOT_CONNECTED)) {
#ifndef SWDS
    // Connected to server?
    if (cl.IsConnected()) {
      ConMsg("Can't set %s when connected\n", v->GetName());
      return true;
    }
#endif
  }

  // Allow cheat commands in singleplayer, debug, or multiplayer with sv_cheats
  // on
  if (v->IsFlagSet(FCVAR_CHEAT)) {
    if (!Host_IsSinglePlayerGame() && !CanCheat()
#if !defined(SWDS)
        && !cl.ishltv && !demoplayer->IsPlayingBack()
#endif
    ) {
      ConMsg(
          "Can't use cheat cvar %s in multiplayer, unless the server has "
          "sv_cheats set to 1.\n",
          v->GetName());
      return true;
    }
  }

  // Text invoking the command was typed into the console, decide what to do
  // with it
  //  if this is a replicated ConVar, except don't worry about restrictions if
  //  playing a .dem file
  if (v->IsFlagSet(FCVAR_REPLICATED)
#if !defined(SWDS)
      && !demoplayer->IsPlayingBack()
#endif
  ) {
    // If not running a server but possibly connected as a client, then
    //  if the message came from console, don't process the command
    if (!sv.IsActive() && !sv.IsLoading() && (cmd_source == src_command) &&
        cl.IsConnected()) {
      ConMsg(
          "Can't change replicated ConVar %s from console of client, only "
          "server operator can change its value\n",
          v->GetName());
      return true;
    }

    // TODO(d.rattman):  Do we need a case where cmd_source == src_client?
    Assert(cmd_source != src_client);
  }

  // Note that we don't want the tokenized list, send down the entire string
  // except for surrounding quotes
  char remaining[1024];
  const char *pArgS = args.ArgS();
  isize nLen = strlen(pArgS);
  bool bIsQuoted = pArgS[0] == '\"';

  if (!bIsQuoted) {
    strcpy_s(remaining, args.ArgS());
  } else {
    --nLen;
    strcpy_s(remaining, &pArgS[1]);
  }

  // Now strip off any trailing spaces
  char *p = remaining + nLen - 1;
  while (p >= remaining) {
    if (*p > ' ') break;

    *p-- = 0;
  }

  // Strip off ending quote
  if (bIsQuoted && p >= remaining) {
    if (*p == '\"') {
      *p = 0;
    }
  }

  SetDirect(v, remaining);
  return true;
}

// This is a band-aid copied directly from IsCommand().
bool CCvarUtilities::IsValidToggleCommand(const char *cmd) {
  // check variables
  ConVar *v = g_pCVar->FindVar(cmd);
  if (!v) {
    ConMsg("%s is not a valid cvar\n", cmd);
    return false;
  }

  if (v->IsFlagSet(FCVAR_DEVELOPMENTONLY) || v->IsFlagSet(FCVAR_HIDDEN))
    return false;

  if (v->IsFlagSet(FCVAR_SPONLY)) {
#ifndef SWDS
    // Connected to server?
    if (cl.IsConnected()) {
      // Is it not a single player game?
      if (cl.m_nMaxClients > 1) {
        ConMsg("Can't set %s in multiplayer\n", v->GetName());
        return false;
      }
    }
#endif
  }

  if (v->IsFlagSet(FCVAR_NOT_CONNECTED)) {
#ifndef SWDS
    // Connected to server?
    if (cl.IsConnected()) {
      ConMsg("Can't set %s when connected\n", v->GetName());
      return false;
    }
#endif
  }

  // Allow cheat commands in singleplayer, debug, or multiplayer with sv_cheats
  // on
  if (v->IsFlagSet(FCVAR_CHEAT)) {
    if (!Host_IsSinglePlayerGame() && !CanCheat()
#if !defined(SWDS) && !defined(_XBOX)
        && !demoplayer->IsPlayingBack()
#endif
    ) {
      ConMsg(
          "Can't use cheat cvar %s in multiplayer, unless the server has "
          "sv_cheats set to 1.\n",
          v->GetName());
      return false;
    }
  }

  // Text invoking the command was typed into the console, decide what to do
  // with it
  //  if this is a replicated ConVar, except don't worry about restrictions if
  //  playing a .dem file
  if (v->IsFlagSet(FCVAR_REPLICATED)
#if !defined(SWDS) && !defined(_XBOX)
      && !demoplayer->IsPlayingBack()
#endif
  ) {
    // If not running a server but possibly connected as a client, then
    //  if the message came from console, don't process the command
    if (!sv.IsActive() && !sv.IsLoading() && (cmd_source == src_command) &&
        cl.IsConnected()) {
      ConMsg(
          "Can't change replicated ConVar %s from console of client, only "
          "server operator can change its value\n",
          v->GetName());
      return false;
    }
  }

  // TODO(d.rattman):  Do we need a case where cmd_source == src_client?
  Assert(cmd_source != src_client);
  return true;
}

void CCvarUtilities::WriteVariables(CUtlBuffer &buff) {
  for (auto *var = g_pCVar->GetCommands(); var; var = var->GetNext()) {
    if (var->IsCommand()) continue;

    bool archive =
        var->IsFlagSet(IsX360() ? FCVAR_ARCHIVE_XBOX : FCVAR_ARCHIVE);
    if (archive) {
      buff.Printf("%s \"%s\"\n", var->GetName(), ((ConVar *)var)->GetString());
    }
  }
}

static char *StripTabsAndReturns(const char *inbuffer, char *outbuffer,
                                 int outbufferSize) {
  char *out = outbuffer;
  const char *i = inbuffer;
  char *o = out;

  out[0] = 0;

  while (*i && o - out < outbufferSize - 1) {
    if (*i == '\n' || *i == '\r' || *i == '\t') {
      *o++ = ' ';
      i++;
      continue;
    }
    if (*i == '\"') {
      *o++ = '\'';
      i++;
      continue;
    }

    *o++ = *i++;
  }

  *o = '\0';

  return out;
}

static char *StripQuotes(const char *inbuffer, char *outbuffer,
                         int outbufferSize) {
  char *out = outbuffer;
  const char *i = inbuffer;
  char *o = out;

  out[0] = 0;

  while (*i && o - out < outbufferSize - 1) {
    if (*i == '\"') {
      *o++ = '\'';
      i++;
      continue;
    }

    *o++ = *i++;
  }

  *o = '\0';

  return out;
}

struct ConVarFlags_t {
  int bit;
  const char *desc;
  const char *shortdesc;
};

#define CONVARFLAG(x, y) \
  { FCVAR_##x, #x, #y }

static ConVarFlags_t g_ConVarFlags[] = {
    //	CONVARFLAG( UNREGISTERED, "u" ),
    CONVARFLAG(ARCHIVE, "a"),
    CONVARFLAG(SPONLY, "sp"),
    CONVARFLAG(GAMEDLL, "sv"),
    CONVARFLAG(CHEAT, "cheat"),
    CONVARFLAG(USERINFO, "user"),
    CONVARFLAG(NOTIFY, "nf"),
    CONVARFLAG(PROTECTED, "prot"),
    CONVARFLAG(PRINTABLEONLY, "print"),
    CONVARFLAG(UNLOGGED, "log"),
    CONVARFLAG(NEVER_AS_STRING, "numeric"),
    CONVARFLAG(REPLICATED, "rep"),
    CONVARFLAG(DEMO, "demo"),
    CONVARFLAG(DONTRECORD, "norecord"),
    CONVARFLAG(SERVER_CAN_EXECUTE, "server_can_execute"),
    CONVARFLAG(CLIENTCMD_CAN_EXECUTE, "clientcmd_can_execute"),
    CONVARFLAG(CLIENTDLL, "cl"),
};

static void PrintListHeader(FileHandle_t &f) {
  char csvflagstr[1024];
  csvflagstr[0] = 0;

  for (ConVarFlags_t &entry : g_ConVarFlags) {
    char csvf[64];

    sprintf_s(csvf, "\"%s\",", entry.desc);
    strcat_s(csvflagstr, csvf);
  }

  g_pFileSystem->FPrintf(f, "\"%s\",\"%s\",%s,\"%s\"\n", "Name", "Value",
                         csvflagstr, "Help Text");
}

static void PrintCvar(const ConVar *var, bool logging, FileHandle_t &f) {
  char flagstr[128];
  char csvflagstr[1024];

  flagstr[0] = 0;
  csvflagstr[0] = 0;

  usize c = std::size(g_ConVarFlags);
  for (usize i = 0; i < c; ++i) {
    char fl[32];
    char csvf[64];

    ConVarFlags_t &entry = g_ConVarFlags[i];
    if (var->IsFlagSet(entry.bit)) {
      Q_snprintf(fl, std::size(fl), ", %s", entry.shortdesc);
      Q_strncat(flagstr, fl, std::size(flagstr), COPY_ALL_CHARACTERS);
      Q_snprintf(csvf, std::size(csvf), "\"%s\",", entry.desc);
    } else {
      Q_snprintf(csvf, std::size(csvf), ",");
    }

    Q_strncat(csvflagstr, csvf, std::size(csvflagstr), COPY_ALL_CHARACTERS);
  }

  char valstr[32];
  char tempbuff[128];

  // Clean up integers
  if (var->GetInt() == (int)var->GetFloat()) {
    Q_snprintf(valstr, std::size(valstr), "%-8i", var->GetInt());
  } else {
    Q_snprintf(valstr, std::size(valstr), "%-8.3f", var->GetFloat());
  }

  // Print to console
  ConMsg(
      "%-40s : %-8s : %-16s : %s\n", var->GetName(), valstr, flagstr,
      StripTabsAndReturns(var->GetHelpText(), tempbuff, std::size(tempbuff)));
  if (logging) {
    g_pFileSystem->FPrintf(
        f, "\"%s\",\"%s\",%s,\"%s\"\n", var->GetName(), valstr, csvflagstr,
        StripQuotes(var->GetHelpText(), tempbuff, std::size(tempbuff)));
  }
}

static void PrintCommand(const ConCommand *cmd, bool logging, FileHandle_t &f) {
  // Print to console
  char tempbuff[128];
  ConMsg("%-40s : %-8s : %-16s : %s\n", cmd->GetName(), "cmd", "",
         StripTabsAndReturns(cmd->GetHelpText(), tempbuff, sizeof(tempbuff)));
  if (logging) {
    char emptyflags[256];

    emptyflags[0] = 0;

    int c = std::size(g_ConVarFlags);
    for (int i = 0; i < c; ++i) {
      char csvf[64];
      Q_snprintf(csvf, sizeof(csvf), ",");
      Q_strncat(emptyflags, csvf, sizeof(emptyflags), COPY_ALL_CHARACTERS);
    }
    // Names staring with +/- need to be wrapped in single quotes
    char name[256];
    Q_snprintf(name, sizeof(name), "%s", cmd->GetName());
    if (name[0] == '+' || name[0] == '-') {
      Q_snprintf(name, sizeof(name), "'%s'", cmd->GetName());
    }
    char tempbuff2[128];
    g_pFileSystem->FPrintf(
        f, "\"%s\",\"%s\",%s,\"%s\"\n", name, "cmd", emptyflags,
        StripQuotes(cmd->GetHelpText(), tempbuff2, sizeof(tempbuff2)));
  }
}

static bool ConCommandBaseLessFunc(const ConCommandBase *const &lhs,
                                   const ConCommandBase *const &rhs) {
  const char *left = lhs->GetName();
  const char *right = rhs->GetName();

  if (*left == '-' || *left == '+') left++;
  if (*right == '-' || *right == '+') right++;

  return (_stricmp(left, right) < 0);
}

void CCvarUtilities::CvarList(const CCommand &args) {
  const ConCommandBase *var;      // Temporary Pointer to cvars
  int iArgs;                      // Argument count
  const char *partial = nullptr;  // Partial cvar to search for...
                                  // E.eg
  int ipLen = 0;                  // Length of the partial cvar

  FileHandle_t f = FILESYSTEM_INVALID_HANDLE;  // FilePointer for logging
  bool bLogging = false;
  // Are we logging?
  iArgs = args.ArgC();  // Get count

  // Print usage?
  if (iArgs == 2 && !Q_strcasecmp(args[1], "?")) {
    ConMsg("cvarlist:  [log logfile] [ partial ]\n");
    return;
  }

  if (!Q_strcasecmp(args[1], "log") && iArgs >= 3) {
    char fn[256];
    sprintf_s(fn, "%s", args[2]);

    f = g_pFileSystem->Open(fn, "wb");
    if (f) {
      bLogging = true;
    } else {
      ConMsg("Couldn't open '%s' for writing!\n", fn);
      return;
    }

    if (iArgs == 4) {
      partial = args[3];
      ipLen = Q_strlen(partial);
    }
  } else {
    partial = args[1];
    ipLen = Q_strlen(partial);
  }

  // Banner
  ConMsg("cvar list\n--------------\n");

  CUtlRBTree<const ConCommandBase *> sorted(0, 0, ConCommandBaseLessFunc);

  // Loop through cvars...
  for (var = g_pCVar->GetCommands(); var; var = var->GetNext()) {
    bool print = false;

    if (var->IsFlagSet(FCVAR_DEVELOPMENTONLY) || var->IsFlagSet(FCVAR_HIDDEN))
      continue;

    if (partial)  // Partial string searching?
    {
      if (!Q_strncasecmp(var->GetName(), partial, ipLen)) {
        print = true;
      }
    } else {
      print = true;
    }

    if (!print) continue;

    sorted.Insert(var);
  }

  if (bLogging) PrintListHeader(f);

  for (int i = sorted.FirstInorder(); i != sorted.InvalidIndex();
       i = sorted.NextInorder(i)) {
    var = sorted[i];
    if (var->IsCommand()) {
      PrintCommand((ConCommand *)var, bLogging, f);
    } else {
      PrintCvar((ConVar *)var, bLogging, f);
    }
  }

  // Show total and syntax help...
  if (partial && partial[0]) {
    ConMsg("--------------\n%3i convars/concommands for [%s]\n", sorted.Count(),
           partial);
  } else {
    ConMsg("--------------\n%3i total convars/concommands\n", sorted.Count());
  }

  if (bLogging) g_pFileSystem->Close(f);
}

int CCvarUtilities::CountVariablesWithFlags(int flags) {
  int i = 0;

  for (auto *var = g_pCVar->GetCommands(); var; var = var->GetNext()) {
    if (var->IsCommand()) continue;

    if (var->IsFlagSet(flags)) i++;
  }

  return i;
}

void CCvarUtilities::CvarHelp(const CCommand &args) {
  if (args.ArgC() != 2) {
    ConMsg("Usage:  help <cvarname>\n");
    return;
  }

  // Get name of var to find
  const char *search = args[1];

  // Search for it
  const ConCommandBase *var = g_pCVar->FindCommandBase(search);
  if (!var) {
    ConMsg("help:  no cvar or command named %s\n", search);
    return;
  }

  // Show info
  ConVar_PrintDescription(var);
}

void CCvarUtilities::CvarDifferences(const CCommand &args) {
  // Loop through vars and print out findings
  for (auto *var = g_pCVar->GetCommands(); var; var = var->GetNext()) {
    if (var->IsCommand()) continue;
    if (var->IsFlagSet(FCVAR_DEVELOPMENTONLY) || var->IsFlagSet(FCVAR_HIDDEN))
      continue;

    if (!_stricmp(((ConVar *)var)->GetDefault(), ((ConVar *)var)->GetString()))
      continue;

    ConVar_PrintDescription((ConVar *)var);
  }
}

// Purpose: Toggles a cvar on/off, or cycles through a set of values
void CCvarUtilities::CvarToggle(const CCommand &args) {
  int c = args.ArgC();
  if (c < 2) {
    ConMsg("Usage:  toggle <cvarname> [value1] [value2] [value3]...\n");
    return;
  }

  ConVar *var = g_pCVar->FindVar(args[1]);

  if (!IsValidToggleCommand(args[1])) {
    return;
  }

  if (c == 2) {
    // just toggle it on and off
    var->SetValue(!var->GetBool());
    ConVar_PrintDescription(var);
  } else {
    int i;
    // look for the current value in the command arguments
    for (i = 2; i < c; i++) {
      if (!Q_strcmp(var->GetString(), args[i])) break;
    }

    // choose the next one
    i++;

    // if we didn't find it, or were at the last value in the command arguments,
    // use the 1st argument
    if (i >= c) {
      i = 2;
    }

    var->SetValue(args[i]);
    ConVar_PrintDescription(var);
  }
}

void CCvarUtilities::CvarFindFlags_f(const CCommand &args) {
  if (args.ArgC() < 2) {
    ConMsg("Usage:  findflags <string>\n");
    ConMsg("Available flags to search for: \n");

    for (usize i = 0; i < std::size(g_ConVarFlags); i++) {
      ConMsg("   - %s\n", g_ConVarFlags[i].desc);
    }

    return;
  }

  // Get substring to find
  const char *search = args[1];

  // Loop through vars and print out findings
  for (auto *var = g_pCVar->GetCommands(); var; var = var->GetNext()) {
    if (var->IsFlagSet(FCVAR_DEVELOPMENTONLY) || var->IsFlagSet(FCVAR_HIDDEN))
      continue;

    for (usize i = 0; i < std::size(g_ConVarFlags); i++) {
      if (!var->IsFlagSet(g_ConVarFlags[i].bit)) continue;
      if (!V_stristr(g_ConVarFlags[i].desc, search)) continue;

      ConVar_PrintDescription(var);
    }
  }
}

// Purpose: Hook to command
CON_COMMAND(findflags, "Find concommands by flags.") {
  cv->CvarFindFlags_f(args);
}

// Purpose: Hook to command
CON_COMMAND(cvarlist, "Show the list of convars/concommands.") {
  cv->CvarList(args);
}

// Purpose: Print help text for cvar
CON_COMMAND(help, "Find help about a convar/concommand.") {
  cv->CvarHelp(args);
}

CON_COMMAND(differences,
            "Show all convars which are not at their default values.") {
  cv->CvarDifferences(args);
}

CON_COMMAND(toggle,
            "Toggles a convar on or off, or cycles through a set of values.") {
  cv->CvarToggle(args);
}
