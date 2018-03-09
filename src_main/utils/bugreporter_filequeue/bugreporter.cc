// Copyright © 1996-2018, Valve Corporation, All rights reserved.

//#define PROTECTED_THINGS_DISABLE
#undef PROTECT_FILEIO_FUNCTIONS
#undef fopen

#include "base/include/windows/windows_light.h"

#include <direct.h>
#include <io.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <cstdio>
#include <ctime>
#include "bugreporter/bugreporter.h"
#include "filesystem_tools.h"
#include "tier0/include/basetypes.h"
#include "tier1/keyvalues.h"
#include "tier1/utlbuffer.h"
#include "tier1/utldict.h"
#include "tier1/utlmap.h"
#include "tier1/utlsymbol.h"
#include "tier1/utlvector.h"
#include "vstdlib/random.h"

#define BUGSUB_CONFIG "\\\\bugbait\\bugsub\\config.txt"

class BugReporter *g_bugreporter = NULL;

class BugReporter : public IBugReporter {
 public:
  BugReporter();
  virtual ~BugReporter();

  // Initialize and login with default username/password for this computer (from
  // resource/bugreporter.res)
  virtual bool Init(CreateInterfaceFn engineFactory);
  virtual void Shutdown();

  virtual bool IsPublicUI() { return false; }

  virtual char const *GetUserName();
  virtual char const *GetUserName_Display();

  virtual int GetNameCount();
  virtual char const *GetName(int index);

  virtual int GetDisplayNameCount();
  virtual char const *GetDisplayName(int index);
  virtual char const *GetUserNameForIndex(int index);

  virtual char const *GetDisplayNameForUserName(char const *username);
  virtual char const *GetUserNameForDisplayName(char const *display);
  virtual char const *GetAreaMapForArea(char const *area);

  virtual int GetSeverityCount();
  virtual char const *GetSeverity(int index);

  virtual int GetPriorityCount();
  virtual char const *GetPriority(int index);

  virtual int GetAreaCount();
  virtual char const *GetArea(int index);

  virtual int GetAreaMapCount();
  virtual char const *GetAreaMap(int index);

  virtual int GetMapNumberCount();
  virtual char const *GetMapNumber(int index);

  virtual int GetReportTypeCount();
  virtual char const *GetReportType(int index);

  virtual char const *GetRepositoryURL(void);
  virtual char const *GetSubmissionURL(void);

  virtual int GetLevelCount(int area);
  virtual char const *GetLevel(int area, int index);

  // Submission API
  virtual void StartNewBugReport();
  virtual void CancelNewBugReport();
  virtual bool CommitBugReport(int &bugSubmissionId);

  virtual void SetTitle(char const *title);
  virtual void SetDescription(char const *description);

  // NULL for current user
  virtual void SetSubmitter(char const *username = 0);
  virtual void SetOwner(char const *username);
  virtual void SetSeverity(char const *severity);
  virtual void SetPriority(char const *priority);
  virtual void SetArea(char const *area);
  virtual void SetMapNumber(char const *mapnumber);
  virtual void SetReportType(char const *reporttype);

  virtual void SetLevel(char const *levelnamne);
  virtual void SetPosition(char const *position);
  virtual void SetOrientation(char const *pitch_yaw_roll);
  virtual void SetBuildNumber(char const *build_num);

  virtual void SetScreenShot(char const *screenshot_unc_address);
  virtual void SetSaveGame(char const *savegame_unc_address);

  virtual void SetBSPName(char const *bsp_unc_address);
  virtual void SetVMFName(char const *vmf_unc_address);

  virtual void AddIncludedFile(char const *filename);
  virtual void ResetIncludedFiles();

  virtual void SetZipAttachmentName(char const *zipfilename) {
  }  // only used by public bug reporter

  virtual void SetDriverInfo(char const *info);

  virtual void SetMiscInfo(char const *info);

  // These are stubbed here, but are used by the public version...
  virtual void SetCSERAddress(const struct netadr_s &adr) {}
  virtual void SetExeName(char const *exename) {}
  virtual void SetGameDirectory(char const *game_dir) {}
  virtual void SetRAM(int ram) {}
  virtual void SetCPU(int cpu) {}
  virtual void SetProcessor(char const *processor) {}
  virtual void SetDXVersion(unsigned int high, unsigned int low,
                            unsigned int vendor, unsigned int device) {}
  virtual void SetOSVersion(char const *osversion) {}
  virtual void SetSteamUserID(void *steamid, int idsize){};
  bool SymbolLessThan(const CUtlSymbol &sym1, const CUtlSymbol &sym2);

 private:
  bool PopulateLists();
  bool PopulateChoiceList(char const *listname, CUtlVector<CUtlSymbol> &list);

  CUtlSymbolTable bug_strings_;

  CUtlVector<CUtlSymbol> severities_;
  CUtlVector<CUtlSymbol> sorted_display_names_;
  CUtlVector<CUtlSymbol> m_SortedUserNames;
  CUtlVector<CUtlSymbol> priorities_;
  CUtlVector<CUtlSymbol> areas_;
  CUtlVector<CUtlSymbol> area_maps_;
  CUtlVector<CUtlSymbol> map_numbers_;
  CUtlVector<CUtlSymbol> report_types_;
  CUtlMap<CUtlSymbol, CUtlVector<CUtlSymbol> *> m_LevelMap;

  CUtlSymbol user_name_;

  Bug *bug_;

  char m_BugRootDirectory[512];
  KeyValues *m_OptionsFile;

  int m_CurrentBugID;
  char m_CurrentBugDirectory[512];
};

bool CUtlSymbol_LessThan(const CUtlSymbol &sym1, const CUtlSymbol &sym2) {
  return g_bugreporter->SymbolLessThan(sym1, sym2);
}

BugReporter::BugReporter() {
  bug_ = NULL;
  m_CurrentBugID = 0;
  m_LevelMap.SetLessFunc(&CUtlSymbol_LessThan);
  g_bugreporter = this;
}

BugReporter::~BugReporter() {
  bug_strings_.RemoveAll();
  severities_.Purge();
  sorted_display_names_.Purge();
  priorities_.Purge();
  areas_.Purge();
  map_numbers_.Purge();
  report_types_.Purge();
  m_LevelMap.RemoveAll();

  delete bug_;
}

//-----------------------------------------------------------------------------
// Purpose: Initialize and login with default username/password for this
// computer (from resource/bugreporter.res) Output : Returns true on success,
// false on failure.
//-----------------------------------------------------------------------------
bool BugReporter::Init(CreateInterfaceFn engineFactory) {
  // Load our bugreporter_text options file
  m_OptionsFile = new KeyValues("OptionsFile");

  // open file old skool way to avoid steam filesystem restrictions
  struct _stat cfg_info;
  if (_stat(BUGSUB_CONFIG, &cfg_info)) {
    AssertMsg(0, "Failed to find bugreporter options file.");
    return false;
  }

  int buf_size = (cfg_info.st_size + 1);
  char *buf = new char[buf_size];
  FILE *fp = fopen(BUGSUB_CONFIG, "rb");
  if (!fp) {
    AssertMsg(0, "failed to open bugreporter options file");
    return false;
  }

  fread(buf, cfg_info.st_size, 1, fp);
  fclose(fp);
  buf[cfg_info.st_size] = 0;
  if (!m_OptionsFile->LoadFromBuffer(BUGSUB_CONFIG, buf)) {
    AssertMsg(0, "Failed to load bugreporter_text options file.");
    delete buf;
    return false;
  }
  strncpy(m_BugRootDirectory, m_OptionsFile->GetString("bug_directory", "."),
          sizeof(m_BugRootDirectory));

  PopulateLists();

  user_name_ = bug_strings_.AddString(getenv("username"));
  delete buf;
  return true;
}

void BugReporter::Shutdown() {}

char const *BugReporter::GetUserName() {
  return bug_strings_.String(user_name_);
}

char const *BugReporter::GetUserName_Display() {
  return GetDisplayNameForUserName(GetUserName());
}

int BugReporter::GetNameCount() { return GetDisplayNameCount(); }

char const *BugReporter::GetName(int index) { return GetDisplayName(index); }

int BugReporter::GetDisplayNameCount() { return sorted_display_names_.Count(); }

char const *BugReporter::GetDisplayName(int index) {
  if (index < 0 || index >= (int)sorted_display_names_.Count())
    return "<<Invalid>>";

  return bug_strings_.String(sorted_display_names_[index]);
}

char const *BugReporter::GetUserNameForIndex(int index) {
  if (index < 0 || index >= (int)m_SortedUserNames.Count())
    return "<<Invalid>>";

  return bug_strings_.String(m_SortedUserNames[index]);
}

char const *BugReporter::GetDisplayNameForUserName(char const *username) {
  CUtlSymbol username_sym = bug_strings_.Find(username);
  FOR_EACH_VEC(m_SortedUserNames, index) {
    if (m_SortedUserNames[index] == username_sym) {
      return GetDisplayName(index);
    }
  }
  return username;
}

char const *BugReporter::GetUserNameForDisplayName(char const *display) {
  CUtlSymbol display_sym = bug_strings_.Find(display);
  FOR_EACH_VEC(sorted_display_names_, index) {
    if (sorted_display_names_[index] == display_sym) {
      return GetUserNameForIndex(index);
    }
  }

  return display;
}

char const *BugReporter::GetAreaMapForArea(char const *area) {
  CUtlSymbol area_sym = bug_strings_.Find(area);
  FOR_EACH_VEC(areas_, index) {
    if (areas_[index] == area_sym && index > 0) {
      char const *areamap = bug_strings_.String(area_maps_[index - 1]);
      return areamap + 1;
    }
  }

  return "<<Invalid>>";
}

int BugReporter::GetSeverityCount() { return severities_.Count(); }

char const *BugReporter::GetSeverity(int index) {
  if (index < 0 || index >= severities_.Count()) return "<<Invalid>>";

  return bug_strings_.String(severities_[index]);
}

int BugReporter::GetPriorityCount() { return priorities_.Count(); }

char const *BugReporter::GetPriority(int index) {
  if (index < 0 || index >= priorities_.Count()) return "<<Invalid>>";

  return bug_strings_.String(priorities_[index]);
}

int BugReporter::GetAreaCount() { return areas_.Count(); }

char const *BugReporter::GetArea(int index) {
  if (index < 0 || index >= areas_.Count()) return "<<Invalid>>";

  return bug_strings_.String(areas_[index]);
}

int BugReporter::GetAreaMapCount() { return area_maps_.Count(); }

char const *BugReporter::GetAreaMap(int index) {
  if (index < 0 || index >= area_maps_.Count()) return "<<Invalid>>";

  return bug_strings_.String(area_maps_[index]);
}

int BugReporter::GetMapNumberCount() { return map_numbers_.Count(); }

char const *BugReporter::GetMapNumber(int index) {
  if (index < 0 || index >= map_numbers_.Count()) return "<<Invalid>>";

  return bug_strings_.String(map_numbers_[index]);
}

int BugReporter::GetReportTypeCount() { return report_types_.Count(); }

char const *BugReporter::GetReportType(int index) {
  if (index < 0 || index >= report_types_.Count()) return "<<Invalid>>";

  return bug_strings_.String(report_types_[index]);
}

char const *BugReporter::GetRepositoryURL(void) { return m_BugRootDirectory; }

// only valid after calling CBugReporter::StartNewBugReport()
char const *BugReporter::GetSubmissionURL(void) {
  return m_CurrentBugDirectory;
}

int BugReporter::GetLevelCount(int area) {
  CUtlSymbol area_sym = area_maps_[area - 1];
  int area_index = m_LevelMap.Find(area_sym);
  if (m_LevelMap.IsValidIndex(area_index)) {
    CUtlVector<CUtlSymbol> *levels = m_LevelMap[area_index];
    return levels->Count();
  }
  return 0;
}

char const *BugReporter::GetLevel(int area, int index) {
  CUtlSymbol area_sym = area_maps_[area - 1];
  int area_index = m_LevelMap.Find(area_sym);
  if (m_LevelMap.IsValidIndex(area_index)) {
    CUtlVector<CUtlSymbol> *levels = m_LevelMap[area_index];
    if (index >= 0 && index < levels->Count()) {
      return bug_strings_.String((*levels)[index]);
    }
  }

  return "";
}

void BugReporter::StartNewBugReport() {
  if (!bug_) {
    bug_ = new Bug();
  } else {
    bug_->Clear();
  }

  // Find the first available bug number by looking
  // for the next directory that doesn't exist.
  m_CurrentBugID = 0;
  struct tm t;

  do {
    VCRHook_LocalTime(&t);

    Q_snprintf(m_CurrentBugDirectory, sizeof(m_CurrentBugDirectory),
               "%s\\%04i%02i%02i-%02i%02i%02i-%s", m_BugRootDirectory,
               t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min,
               t.tm_sec, bug_strings_.String(user_name_));
    if (_access(m_CurrentBugDirectory, 0) != 0) break;

    // sleep for a second or two then try again
    Sleep(RandomInt(1000, 2000));
  } while (1);
  _mkdir(m_CurrentBugDirectory);
}

void BugReporter::CancelNewBugReport() {
  if (!bug_) return;

  bug_->Clear();
  m_CurrentBugID = 0;
}

static void OutputField(CUtlBuffer &outbuf, char const *pFieldName,
                        char const *pFieldValue) {
  if (pFieldValue && pFieldValue[0]) {
    char const *pTmpString = V_AddBackSlashesToSpecialChars(pFieldValue);
    outbuf.Printf("%s=\"%s\"\n", pFieldName, pTmpString);
    delete[] pTmpString;
  }
}

bool BugReporter::CommitBugReport(int &bugSubmissionId) {
  bugSubmissionId = m_CurrentBugID;

  if (!bug_) return false;

  // Now create the bug descriptor file, and dump all the text keys in it
  CUtlBuffer buf(0, 0, CUtlBuffer::TEXT_BUFFER);

  OutputField(buf, "Title", bug_->title);
  OutputField(buf, "Owner", bug_->owner);
  OutputField(buf, "Submitter", bug_->submitter);
  OutputField(buf, "Severity", bug_->severity);
  //	OutputField( buf, "Type", m_pBug->reporttype );
  //	OutputField( buf, "Priority", m_pBug->priority );
  OutputField(buf, "Area", bug_->area);
  OutputField(buf, "Level", bug_->mapNumber);
  OutputField(buf, "Description", bug_->desc);

  // OutputField( buf, "Level", m_pBug->level );
  OutputField(buf, "Build", bug_->build);
  OutputField(buf, "Position", bug_->position);
  OutputField(buf, "Orientation", bug_->orientation);

  OutputField(buf, "Screenshot", bug_->screenshotUnc);
  OutputField(buf, "Savegame", bug_->savegameUnc);
  OutputField(buf, "Bsp", bug_->bspUnc);
  OutputField(buf, "vmf", bug_->vmfUnc);

  // 	if ( m_pBug->includedfiles.Count() > 0 )
  // 	{
  // 		int c = m_pBug->includedfiles.Count();
  // 		for ( int i = 0 ; i < c; ++i )
  // 		{
  // 			buf.Printf( "include:  %s\n", m_pBug->includedfiles[ i
  // ].name
  // );
  // 		}
  // 	}
  OutputField(buf, "DriverInfo", bug_->driverInfo);

  OutputField(buf, "Misc", bug_->misc);

  // Write it out to the file
  // Need to use native calls to bypass steam filesystem
  char szBugFileName[1024];
  Q_snprintf(szBugFileName, sizeof(szBugFileName), "%s\\bug.txt",
             m_CurrentBugDirectory);
  FILE *fp = fopen(szBugFileName, "wb");
  if (!fp) return false;

  fprintf(fp, "%s", (char *)buf.Base());
  fclose(fp);

  // Clear the bug
  bug_->Clear();

  return true;
}

void BugReporter::SetTitle(char const *title) {
  Assert(bug_);
  Q_strncpy(bug_->title, title, sizeof(bug_->title));
}

void BugReporter::SetDescription(char const *description) {
  Assert(bug_);
  Q_strncpy(bug_->desc, description, sizeof(bug_->desc));
}

void BugReporter::SetSubmitter(char const *username /* = 0 */) {
  if (!username) {
    username = GetUserName();
  }

  Assert(bug_);
  Q_strncpy(bug_->submitter, username, sizeof(bug_->submitter));
}

void BugReporter::SetOwner(char const *username) {
  Assert(bug_);
  Q_strncpy(bug_->owner, username, sizeof(bug_->owner));
}

void BugReporter::SetSeverity(char const *severity) {
  Assert(bug_);
  Q_strncpy(bug_->severity, severity, sizeof(bug_->severity));
}

void BugReporter::SetPriority(char const *priority) {
  Assert(bug_);
  Q_strncpy(bug_->priority, priority, sizeof(bug_->priority));
}

void BugReporter::SetArea(char const *area) {
  Assert(bug_);
  char const *game = GetAreaMapForArea(area);
  Q_strncpy(bug_->area, game, sizeof(bug_->area));
}

void BugReporter::SetMapNumber(char const *mapnumber) {
  Assert(bug_);
  Q_strncpy(bug_->mapNumber, mapnumber, sizeof(bug_->mapNumber));
}

void BugReporter::SetReportType(char const *reporttype) {
  Assert(bug_);
  Q_strncpy(bug_->reportType, reporttype, sizeof(bug_->reportType));
}

void BugReporter::SetLevel(char const *levelnamne) {
  Assert(bug_);
  Q_strncpy(bug_->level, levelnamne, sizeof(bug_->level));
}

void BugReporter::SetDriverInfo(char const *info) {
  Assert(bug_);
  Q_strncpy(bug_->driverInfo, info, sizeof(bug_->driverInfo));
}

void BugReporter::SetMiscInfo(char const *info) {
  Assert(bug_);
  Q_strncpy(bug_->misc, info, sizeof(bug_->misc));
}

void BugReporter::SetPosition(char const *position) {
  Assert(bug_);
  Q_strncpy(bug_->position, position, sizeof(bug_->position));
}

void BugReporter::SetOrientation(char const *pitch_yaw_roll) {
  Assert(bug_);
  Q_strncpy(bug_->orientation, pitch_yaw_roll, sizeof(bug_->orientation));
}

void BugReporter::SetBuildNumber(char const *build_num) {
  Assert(bug_);
  Q_strncpy(bug_->build, build_num, sizeof(bug_->build));
}

void BugReporter::SetScreenShot(char const *screenshot_unc_address) {
  Assert(bug_);
  Q_strncpy(bug_->screenshotUnc, screenshot_unc_address,
            sizeof(bug_->screenshotUnc));
}

void BugReporter::SetSaveGame(char const *savegame_unc_address) {
  Assert(bug_);
  Q_strncpy(bug_->savegameUnc, savegame_unc_address, sizeof(bug_->savegameUnc));
}

void BugReporter::SetBSPName(char const *bsp_unc_address) {
  Assert(bug_);
  Q_strncpy(bug_->bspUnc, bsp_unc_address, sizeof(bug_->bspUnc));
}

void BugReporter::SetVMFName(char const *vmf_unc_address) {
  Assert(bug_);
  Q_strncpy(bug_->vmfUnc, vmf_unc_address, sizeof(bug_->vmfUnc));
}

void BugReporter::AddIncludedFile(char const *filename) {
  Bug::IncludeFile includedfile;
  Q_strncpy(includedfile.name, filename, sizeof(includedfile.name));
  bug_->includedFiles.AddToTail(includedfile);
}

void BugReporter::ResetIncludedFiles() { bug_->includedFiles.Purge(); }

bool BugReporter::PopulateChoiceList(char const *listname,
                                     CUtlVector<CUtlSymbol> &list) {
  // Read choice lists from text file
  KeyValues *pKV = m_OptionsFile->FindKey(listname);
  pKV = pKV->GetFirstSubKey();
  while (pKV) {
    CUtlSymbol sym = bug_strings_.AddString(pKV->GetName());
    list.AddToTail(sym);

    pKV = pKV->GetNextKey();
  }

  return true;
}

bool BugReporter::SymbolLessThan(const CUtlSymbol &sym1,
                                 const CUtlSymbol &sym2) {
  const char *string1 = bug_strings_.String(sym1);
  const char *string2 = bug_strings_.String(sym2);

  return Q_stricmp(string1, string2) < 0;
}

// owner, not required <<Unassigned>>
// area <<None>>

bool BugReporter::PopulateLists() {
  CUtlSymbol none = bug_strings_.AddString("<<None>>");
  areas_.AddToTail(none);

  PopulateChoiceList("Severity", severities_);

  // Get developer names from text file
  KeyValues *pKV = m_OptionsFile->FindKey("Names");
  pKV = pKV->GetFirstSubKey();
  while (pKV) {
    // Fill in lookup table
    CUtlSymbol display_name_sym = bug_strings_.AddString(pKV->GetName());
    CUtlSymbol user_name_sym = bug_strings_.AddString(pKV->GetString());
    sorted_display_names_.AddToTail(display_name_sym);
    m_SortedUserNames.AddToTail(user_name_sym);
    pKV = pKV->GetNextKey();
  }

  pKV = m_OptionsFile->FindKey("Area");
  pKV = pKV->GetFirstSubKey();
  while (pKV) {
    char const *area = pKV->GetName();
    char const *game = pKV->GetString();
    char areamap[256];
    Q_snprintf(areamap, sizeof(areamap), "@%s", game);
    CUtlSymbol area_sym = bug_strings_.AddString(area);
    CUtlSymbol area_map_sym = bug_strings_.AddString(areamap);
    areas_.AddToTail(area_sym);
    area_maps_.AddToTail(area_map_sym);
    pKV = pKV->GetNextKey();
  }

  pKV = m_OptionsFile->FindKey("Level");
  pKV = pKV->GetFirstSubKey();
  while (pKV) {
    char const *level = pKV->GetName();
    char const *area = pKV->GetString();
    char areamap[256];
    Q_snprintf(areamap, sizeof(areamap), "@%s", area);

    CUtlSymbol level_sym = bug_strings_.AddString(level);
    CUtlSymbol area_sym = bug_strings_.Find(areamap);
    if (!area_sym.IsValid()) {
      area_sym = bug_strings_.AddString(areamap);
    }

    unsigned index = m_LevelMap.Find(area_sym);
    CUtlVector<CUtlSymbol> *levels = 0;

    if (m_LevelMap.IsValidIndex(index)) {
      levels = m_LevelMap[index];
    } else {
      levels = new CUtlVector<CUtlSymbol>;
      Assert(levels);
      m_LevelMap.Insert(area_sym, levels);
    }

    if (level) {
      levels->AddToTail(level_sym);
    }

    pKV = pKV->GetNextKey();
  }

  return true;
}

EXPOSE_SINGLE_INTERFACE(BugReporter, IBugReporter,
                        INTERFACEVERSION_BUGREPORTER);
