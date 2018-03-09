// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef BUGREPORTER_H
#define BUGREPORTER_H

#include "tier1/interface.h"
#include "tier1/strtools.h"
#include "tier1/utlvector.h"

#undef GetUserName

the_interface IBugReporter : public IBaseInterface {
 public:
  // Initialize and login with default username/password for this computer (from
  // resource/bugreporter.res)
  virtual bool Init(CreateInterfaceFn engineFactory) = 0;
  virtual void Shutdown() = 0;

  virtual bool IsPublicUI() = 0;

  virtual char const *GetUserName() = 0;
  virtual char const *GetUserName_Display() = 0;

  virtual int GetNameCount() = 0;
  virtual char const *GetName(int index) = 0;

  virtual int GetDisplayNameCount() = 0;
  virtual char const *GetDisplayName(int index) = 0;

  virtual char const *GetDisplayNameForUserName(char const *username) = 0;
  virtual char const *GetUserNameForDisplayName(char const *display) = 0;

  virtual int GetSeverityCount() = 0;
  virtual char const *GetSeverity(int index) = 0;

  virtual int GetPriorityCount() = 0;
  virtual char const *GetPriority(int index) = 0;

  virtual int GetAreaCount() = 0;
  virtual char const *GetArea(int index) = 0;

  virtual int GetAreaMapCount() = 0;
  virtual char const *GetAreaMap(int index) = 0;

  virtual int GetMapNumberCount() = 0;
  virtual char const *GetMapNumber(int index) = 0;

  virtual int GetReportTypeCount() = 0;
  virtual char const *GetReportType(int index) = 0;

  virtual char const *GetRepositoryURL() = 0;
  virtual char const *GetSubmissionURL() = 0;

  virtual int GetLevelCount(int area) = 0;
  virtual char const *GetLevel(int area, int index) = 0;

  // Submission API
  virtual void StartNewBugReport() = 0;
  virtual void CancelNewBugReport() = 0;
  virtual bool CommitBugReport(int &bugSubmissionId) = 0;

  virtual void SetTitle(char const *title) = 0;
  virtual void SetDescription(char const *description) = 0;

  // NULL for current user
  virtual void SetSubmitter(char const *username = 0) = 0;
  virtual void SetOwner(char const *username) = 0;
  virtual void SetSeverity(char const *severity) = 0;
  virtual void SetPriority(char const *priority) = 0;
  virtual void SetArea(char const *area) = 0;
  virtual void SetMapNumber(char const *area) = 0;
  virtual void SetReportType(char const *reporttype) = 0;

  virtual void SetLevel(char const *levelnamne) = 0;
  virtual void SetPosition(char const *position) = 0;
  virtual void SetOrientation(char const *pitch_yaw_roll) = 0;
  virtual void SetBuildNumber(char const *build_num) = 0;

  virtual void SetScreenShot(char const *screenshot_unc_address) = 0;
  virtual void SetSaveGame(char const *savegame_unc_address) = 0;
  virtual void SetBSPName(char const *bsp_unc_address) = 0;
  virtual void SetVMFName(char const *vmf_unc_address) = 0;
  virtual void AddIncludedFile(char const *filename) = 0;
  virtual void ResetIncludedFiles() = 0;

  virtual void SetZipAttachmentName(char const *zipfilename) = 0;

  virtual void SetDriverInfo(char const *info) = 0;
  virtual void SetMiscInfo(char const *info) = 0;

  virtual void SetCSERAddress(const struct netadr_s &adr) = 0;
  virtual void SetExeName(char const *exename) = 0;
  virtual void SetGameDirectory(char const *gamedir) = 0;
  virtual void SetRAM(int ram) = 0;
  virtual void SetCPU(int cpu) = 0;
  virtual void SetProcessor(char const *processor) = 0;
  virtual void SetDXVersion(unsigned int high, unsigned int low,
                            unsigned int vendor, unsigned int device) = 0;
  virtual void SetOSVersion(char const *osversion) = 0;

  virtual void SetSteamUserID(void *steamid, int idsize) = 0;
};

#define INTERFACEVERSION_BUGREPORTER "BugReporter004"

struct Bug {
  Bug() { Clear(); }

  void Clear() {
    Q_memset(title, 0, sizeof(title));
    Q_memset(desc, 0, sizeof(desc));
    Q_memset(submitter, 0, sizeof(submitter));
    Q_memset(owner, 0, sizeof(owner));
    Q_memset(severity, 0, sizeof(severity));
    Q_memset(priority, 0, sizeof(priority));
    Q_memset(area, 0, sizeof(area));
    Q_memset(mapNumber, 0, sizeof(mapNumber));
    Q_memset(reportType, 0, sizeof(reportType));
    Q_memset(level, 0, sizeof(level));
    Q_memset(build, 0, sizeof(build));
    Q_memset(position, 0, sizeof(position));
    Q_memset(orientation, 0, sizeof(orientation));
    Q_memset(screenshotUnc, 0, sizeof(screenshotUnc));
    Q_memset(savegameUnc, 0, sizeof(savegameUnc));
    Q_memset(bspUnc, 0, sizeof(bspUnc));
    Q_memset(vmfUnc, 0, sizeof(vmfUnc));
    Q_memset(driverInfo, 0, sizeof(driverInfo));
    Q_memset(misc, 0, sizeof(misc));

    includedFiles.Purge();
  }

  char title[256];
  char desc[8192];
  char owner[256];
  char submitter[256];
  char severity[256];
  char priority[256];
  char area[256];
  char mapNumber[256];
  char reportType[256];
  char level[256];
  char build[256];
  char position[256];
  char orientation[256];
  char screenshotUnc[256];
  char savegameUnc[256];
  char bspUnc[256];
  char vmfUnc[256];
  char driverInfo[2048];
  char misc[1024];

  struct IncludeFile {
    char name[256];
  };

  CUtlVector<IncludeFile> includedFiles;
};

#endif  // BUGREPORTER_H
