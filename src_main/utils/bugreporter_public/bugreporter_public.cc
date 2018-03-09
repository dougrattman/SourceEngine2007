// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "bugreporter/bugreporter.h"

#define PROTECTED_THINGS_DISABLE
#undef PROTECT_FILEIO_FUNCTIONS
#undef fopen
#include "base/include/windows/windows_light.h"

#include "filesystem_tools.h"
#include "shared_file_system.h"
#include "steamcommon.h"
#include "tier0/include/basetypes.h"
#include "tier1/keyvalues.h"
#include "tier1/netadr.h"
#include "tier1/utlbuffer.h"
#include "tier1/utldict.h"
#include "tier1/utlsymbol.h"
#include "tier1/utlvector.h"

bool UploadBugReport(const netadr_t &cserIP, const TSteamGlobalUserID &userid,
                     int build, char const *title, char const *body,
                     char const *exename, char const *gamedir,
                     char const *mapname, char const *reporttype,
                     char const *email, char const *accountname, int ram,
                     int cpu, char const *processor, unsigned int high,
                     unsigned int low, unsigned int vendor, unsigned int device,
                     char const *osversion, char const *attachedfile,
                     unsigned int attachedfilesize);

struct PublicBug : public Bug {
  PublicBug() { Clear(); }

  void Clear() {
    Bug::Clear();

    Q_memset(exename, 0, sizeof(exename));
    Q_memset(gamedir, 0, sizeof(gamedir));
    ram = 0;
    cpu = 0;
    Q_memset(processor, 0, sizeof(processor));
    dxversionhigh = 0;
    dxversionlow = 0;
    dxvendor = 0;
    dxdevice = 0;
    Q_memset(osversion, 0, sizeof(osversion));

    Q_memset(zip, 0, sizeof(zip));
  }

  char exename[256];
  char gamedir[256];
  unsigned int ram;
  unsigned int cpu;
  char processor[256];
  unsigned int dxversionhigh;
  unsigned int dxversionlow;
  unsigned int dxvendor;
  unsigned int dxdevice;
  char osversion[256];

  char zip[256];
};

#undef GetUserName

class BugReporter : public IBugReporter {
 public:
  BugReporter() {
    Q_memset(&m_cserIP, 0, sizeof(m_cserIP));
    bug_ = nullptr;

    severities_.AddToTail(bug_strings_.AddString("Zero"));
    severities_.AddToTail(bug_strings_.AddString("Low"));
    severities_.AddToTail(bug_strings_.AddString("Medium"));
    severities_.AddToTail(bug_strings_.AddString("High"));
    severities_.AddToTail(bug_strings_.AddString("Showstopper"));

    report_types_.AddToTail(bug_strings_.AddString("<<Choose Item>>"));
    report_types_.AddToTail(bug_strings_.AddString("Video / Display Problems"));
    report_types_.AddToTail(
        bug_strings_.AddString("Network / Connectivity Problems"));
    report_types_.AddToTail(
        bug_strings_.AddString("Download / Installation Problems"));
    report_types_.AddToTail(bug_strings_.AddString("In-game Crash"));
    report_types_.AddToTail(
        bug_strings_.AddString("Game play / Strategy Problems"));
    report_types_.AddToTail(bug_strings_.AddString("Steam Problems"));
    report_types_.AddToTail(bug_strings_.AddString("Unlisted PublicBug"));
    report_types_.AddToTail(
        bug_strings_.AddString("Feature Request / Suggestion"));

    file_system_ = nullptr;
    SetSharedFileSystem(file_system_);
  }
  virtual ~BugReporter() {
    bug_strings_.RemoveAll();
    severities_.Purge();
    areas_.Purge();
    map_numbers_.Purge();
    report_types_.Purge();

    delete bug_;
  }

  // Initialize and login with default username/password for this computer (from
  // resource/bugreporter.res)
  virtual bool Init(CreateInterfaceFn engineFactory) {
    file_system_ =
        (IFileSystem *)engineFactory(FILESYSTEM_INTERFACE_VERSION, nullptr);
    if (!file_system_) {
      AssertMsg(0, "Failed to create/get IFileSystem");
      return false;
    }
    SetSharedFileSystem(file_system_);

    return true;
  }

  virtual void Shutdown() {}

  virtual bool IsPublicUI() { return true; }

  virtual char const *GetUserName() { return user_name_.String(); }
  virtual char const *GetUserName_Display() { return GetUserName(); }

  virtual int GetNameCount() { return 1; }
  virtual char const *GetName(int index) {
    if (index < 0 || index >= 1) return "<<Invalid>>";

    return GetUserName();
  }

  virtual int GetDisplayNameCount() { return 1; }
  virtual char const *GetDisplayName(int index) {
    if (index < 0 || index >= 1) return "<<Invalid>>";

    return GetUserName();
  }

  virtual char const *GetDisplayNameForUserName(char const *username) {
    return username;
  }
  virtual char const *GetUserNameForDisplayName(char const *display) {
    return display;
  }

  virtual int GetSeverityCount() { return severities_.Count(); }
  virtual char const *GetSeverity(int index) {
    if (index < 0 || index >= severities_.Count()) return "<<Invalid>>";

    return bug_strings_.String(severities_[index]);
  }

  virtual int GetPriorityCount() { return 0; }
  virtual char const *GetPriority(int index) { return "<<Invalid>>"; }

  virtual int GetAreaCount() { return 0; }
  virtual char const *GetArea(int index) { return "<<Invalid>>"; }

  virtual int GetAreaMapCount() { return 0; }
  virtual char const *GetAreaMap(int index) { return "<<Invalid>>"; }

  virtual int GetMapNumberCount() { return 0; }
  virtual char const *GetMapNumber(int index) { return "<<Invalid>>"; }

  virtual int GetReportTypeCount() { return report_types_.Count(); }
  virtual char const *GetReportType(int index) {
    if (index < 0 || index >= report_types_.Count()) return "<<Invalid>>";

    return bug_strings_.String(report_types_[index]);
  }

  virtual char const *GetRepositoryURL(void) { return NULL; }
  virtual char const *GetSubmissionURL(void) { return NULL; }

  virtual int GetLevelCount(int area) { return 0; }
  virtual char const *GetLevel(int area, int index) { return ""; }

  // Submission API
  virtual void StartNewBugReport() {
    if (!bug_) {
      bug_ = new PublicBug();
    } else {
      bug_->Clear();
    }
  }
  virtual void CancelNewBugReport() {
    if (!bug_) return;

    bug_->Clear();
  }
  virtual bool CommitBugReport(int &bugSubmissionId) {
    bugSubmissionId = -1;

    if (!bug_) return false;

    CUtlBuffer buf(0, 0, CUtlBuffer::TEXT_BUFFER);

    buf.Printf("%s\n\n", bug_->desc);

    buf.Printf("level:  %s\nbuild:  %s\nposition:  setpos %s; setang %s\n",
               bug_->level, bug_->build, bug_->position, bug_->orientation);

    if (bug_->screenshotUnc[0]) {
      buf.Printf("screenshot:  %s\n", bug_->screenshotUnc);
    }
    if (bug_->savegameUnc[0]) {
      buf.Printf("savegame:  %s\n", bug_->savegameUnc);
    }
    /*
    if ( m_pBug->bsp_unc[ 0 ] )
    {
    buf.Printf( "bsp:  %s\n", m_pBug->bsp_unc );
    }
    if ( m_pBug->vmf_unc[ 0 ] )
    {
    buf.Printf( "vmf:  %s\n", m_pBug->vmf_unc );
    }
    if ( m_pBug->includedfiles.Count() > 0 )
    {
    int c = m_pBug->includedfiles.Count();
    for ( int i = 0 ; i < c; ++i )
    {
    buf.Printf( "include:  %s\n", m_pBug->includedfiles[ i ].name
    );
    }
    }
    */
    if (bug_->driverInfo[0]) {
      buf.Printf("%s\n", bug_->driverInfo);
    }
    if (bug_->misc[0]) {
      buf.Printf("%s\n", bug_->misc);
    }

    buf.PutChar(0);

    // Store desc

    bugSubmissionId = 0;

    int attachedfilesize =
        (bug_->zip[0] == 0) ? 0 : file_system_->Size(bug_->zip);

    if (!UploadBugReport(m_cserIP, m_SteamID, atoi(bug_->build), bug_->title,
                         (char const *)buf.Base(), bug_->exename, bug_->gamedir,
                         bug_->level, bug_->reportType, bug_->owner,
                         bug_->submitter, bug_->ram, bug_->cpu, bug_->processor,
                         bug_->dxversionhigh, bug_->dxversionlow,
                         bug_->dxvendor, bug_->dxdevice, bug_->osversion,
                         bug_->zip, attachedfilesize)) {
      Msg("Unable to upload bug...\n");
      return false;
    }

    bug_->Clear();

    return true;
  }

  virtual void SetTitle(char const *title) {
    Assert(bug_);
    Q_strncpy(bug_->title, title, sizeof(bug_->title));
  }
  virtual void SetDescription(char const *description) {
    Assert(bug_);
    Q_strncpy(bug_->desc, description, sizeof(bug_->desc));
  }

  // NULL for current user
  virtual void SetSubmitter(char const *username = 0) {
    user_name_ = username;
    /*
    if ( !username )
    {
    username = GetUserName();
    }
    */

    Assert(bug_);
    Q_strncpy(bug_->submitter, username ? username : "",
              sizeof(bug_->submitter));
  }
  virtual void SetOwner(char const *username) {
    Q_strncpy(bug_->owner, username, sizeof(bug_->owner));
  }
  virtual void SetSeverity(char const *severity) {}
  virtual void SetPriority(char const *priority) {}
  virtual void SetArea(char const *area) {}
  virtual void SetMapNumber(char const *mapnumber) {}
  virtual void SetReportType(char const *reporttype) {
    Q_strncpy(bug_->reportType, reporttype, sizeof(bug_->reportType));
  }

  virtual void SetLevel(char const *levelnamne) {
    Assert(bug_);
    Q_strncpy(bug_->level, levelnamne, sizeof(bug_->level));
  }
  virtual void SetPosition(char const *position) {
    Assert(bug_);
    Q_strncpy(bug_->position, position, sizeof(bug_->position));
  }
  virtual void SetOrientation(char const *pitch_yaw_roll) {
    Assert(bug_);
    Q_strncpy(bug_->orientation, pitch_yaw_roll, sizeof(bug_->orientation));
  }
  virtual void SetBuildNumber(char const *build_num) {
    Assert(bug_);
    Q_strncpy(bug_->build, build_num, sizeof(bug_->build));
  }

  virtual void SetScreenShot(char const *screenshot_unc_address) {
    Assert(bug_);
    Q_strncpy(bug_->screenshotUnc, screenshot_unc_address,
              sizeof(bug_->screenshotUnc));
  }
  virtual void SetSaveGame(char const *savegame_unc_address) {
    Assert(bug_);
    Q_strncpy(bug_->savegameUnc, savegame_unc_address,
              sizeof(bug_->savegameUnc));
  }

  virtual void SetBSPName(char const *bsp_unc_address) {}
  virtual void SetVMFName(char const *vmf_unc_address) {}

  virtual void AddIncludedFile(char const *filename) {}
  virtual void ResetIncludedFiles() {}

  virtual void SetDriverInfo(char const *info) {
    Assert(bug_);
    Q_strncpy(bug_->driverInfo, info, sizeof(bug_->driverInfo));
  }

  virtual void SetZipAttachmentName(char const *zipfilename) {
    Assert(bug_);

    Q_strncpy(bug_->zip, zipfilename, sizeof(bug_->zip));
  }

  virtual void SetMiscInfo(char const *info) {
    Assert(bug_);
    Q_strncpy(bug_->misc, info, sizeof(bug_->misc));
  }

  virtual void SetCSERAddress(const struct netadr_s &adr) { m_cserIP = adr; }
  virtual void SetExeName(char const *exename) {
    Assert(bug_);
    Q_strncpy(bug_->exename, exename, sizeof(bug_->exename));
  }
  virtual void SetGameDirectory(char const *game_dir) {
    Assert(bug_);

    Q_FileBase(game_dir, bug_->gamedir, sizeof(bug_->gamedir));
  }
  virtual void SetRAM(int ram) {
    Assert(bug_);
    bug_->ram = ram;
  }
  virtual void SetCPU(int cpu) {
    Assert(bug_);
    bug_->cpu = cpu;
  }
  virtual void SetProcessor(char const *processor) {
    Assert(bug_);
    Q_strncpy(bug_->processor, processor, sizeof(bug_->processor));
  }
  virtual void SetDXVersion(unsigned int high, unsigned int low,
                            unsigned int vendor, unsigned int device) {
    Assert(bug_);
    bug_->dxversionhigh = high;
    bug_->dxversionlow = low;
    bug_->dxvendor = vendor;
    bug_->dxdevice = device;
  }
  virtual void SetOSVersion(char const *osversion) {
    Assert(bug_);
    Q_strncpy(bug_->osversion, osversion, sizeof(bug_->osversion));
  }
  virtual void SetSteamUserID(void *steamid, int idsize) {
    Assert(idsize == sizeof(TSteamGlobalUserID));
    m_SteamID = *(TSteamGlobalUserID *)steamid;
  }

 private:
  void SubstituteBugId(int bugid, char *out, int outlen, CUtlBuffer &src) {
    out[0] = 0;

    char *dest = out;

    src.SeekGet(CUtlBuffer::SEEK_HEAD, 0);

    char const *replace = "\\BugId\\";
    int replace_len = Q_strlen(replace);

    for (int pos = 0; pos <= src.TellPut() && ((dest - out) < outlen);) {
      char const *str = (char const *)src.PeekGet(pos);
      if (!Q_strnicmp(str, replace, replace_len)) {
        *dest++ = '\\';

        char num[32];
        Q_snprintf(num, sizeof(num), "%i", bugid);
        char *pnum = num;
        while (*pnum) {
          *dest++ = *pnum++;
        }

        *dest++ = '\\';
        pos += replace_len;
        continue;
      }

      *dest++ = *str;
      ++pos;
    }
    *dest = 0;
  }

 private:
  CUtlSymbolTable bug_strings_;

  CUtlVector<CUtlSymbol> severities_;
  CUtlVector<CUtlSymbol> areas_;
  CUtlVector<CUtlSymbol> map_numbers_;
  CUtlVector<CUtlSymbol> report_types_;

  CUtlSymbol user_name_;

  PublicBug *bug_;
  netadr_t m_cserIP;
  TSteamGlobalUserID m_SteamID;

  IBaseFileSystem *file_system_;
};

EXPOSE_SINGLE_INTERFACE(BugReporter, IBugReporter,
                        INTERFACEVERSION_BUGREPORTER);
