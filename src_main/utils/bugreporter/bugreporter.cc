// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "bugreporter/bugreporter.h"

#define PROTECTED_THINGS_DISABLE
#undef PROTECT_FILEIO_FUNCTIONS
#undef fopen
#include "base/include/windows/windows_light.h"

#include "deps/trktool/trktool.h"
#include "filesystem_tools.h"
#include "tier0/include/basetypes.h"
#include "tier1/keyvalues.h"
#include "tier1/utlbuffer.h"
#include "tier1/utldict.h"
#include "tier1/utlsymbol.h"
#include "tier1/utlvector.h"

#undef GetUserName

class BugReporter : public IBugReporter {
 public:
  BugReporter()
      : bug_{nullptr}, trk_handle_{nullptr}, trk_record_handle_{nullptr} {}

  virtual ~BugReporter() {
    bug_strings_.RemoveAll();
    severities_.Purge();
    names_.Purge();
    sorted_display_names_.Purge();
    name_mapping_.Purge();
    priorities_.Purge();
    areas_.Purge();
    map_numbers_.Purge();
    report_types_.Purge();

    delete bug_;
    bug_ = nullptr;
  }

  // Initialize and login with default username/password for this computer (from
  // resource/bugreporter.res).
  virtual bool Init(CreateInterfaceFn engine_factory) {
    if (engine_factory) {
      file_system_ =
          (IFileSystem *)engine_factory(FILESYSTEM_INTERFACE_VERSION, nullptr);
      if (!file_system_) {
        Warning("BugReporter: Failed to create/get file system interface %s.",
                FILESYSTEM_INTERFACE_VERSION);
        return false;
      }
    }

    TRK_UINT rc = TrkHandleAlloc(TRK_VERSION_ID, &trk_handle_);
    if (rc != TRK_SUCCESS) {
      ReportError(rc, "TrkHandleAlloc",
                  "Failed to allocate bug tracker handle.");
      return false;
    }

    // Login to default project out of INI file.
    rc = Login(&trk_handle_);
    if (rc != TRK_SUCCESS) {
      Warning("BugReporter: Login failed.");
      return false;
    }

    rc = TrkRecordHandleAlloc(trk_handle_, &trk_record_handle_);
    if (rc != TRK_SUCCESS) {
      ReportError(rc, "TrkRecordHandleAlloc",
                  "Failed to allocate bug tracker record handle.");
      return false;
    }

    PopulateLists();

    return true;
  }

  virtual void Shutdown() {
    TRK_UINT rc;

    if (trk_record_handle_) {
      rc = TrkRecordHandleFree(&trk_record_handle_);
      if (rc != TRK_SUCCESS) {
        ReportError(rc, "TrkRecordHandleFree",
                    "Failed to free bug tracker record handle.");
      }
    }

    if (trk_handle_) {
      rc = TrkProjectLogout(trk_handle_);
      if (rc != TRK_SUCCESS) {
        ReportError(rc, "TrkProjectLogout", "Failed to logout of project.");
      } else {
        rc = TrkHandleFree(&trk_handle_);
        if (rc != TRK_SUCCESS) {
          ReportError(rc, "TrkHandleFree",
                      "Failed to free bug tracker handle.");
        }
      }
    }
  }

  virtual bool IsPublicUI() { return false; }

  virtual char const *GetUserName() { return bug_strings_.String(user_name_); }
  virtual char const *GetUserName_Display() {
    return GetDisplayNameForUserName(GetUserName());
  }

  virtual int GetNameCount() { return names_.Count(); }
  virtual char const *GetName(int index) {
    if (index < 0 || index >= names_.Count()) return "<<Invalid>>";

    return bug_strings_.String(names_[index]);
  }

  virtual int GetDisplayNameCount() { return sorted_display_names_.Count(); }
  virtual char const *GetDisplayName(int index) {
    if (index < 0 || index >= (int)sorted_display_names_.Count())
      return "<<Invalid>>";

    return bug_strings_.String(sorted_display_names_[index]);
  }

  virtual char const *GetDisplayNameForUserName(char const *username) {
    int c = GetDisplayNameCount();
    for (int i = 0; i < c; i++) {
      CUtlSymbol sym = name_mapping_[i];
      char const *testname = bug_strings_.String(sym);
      if (!Q_stricmp(testname, username)) {
        return name_mapping_.GetElementName(i);
      }
    }

    return "<<Invalid>>";
  }
  virtual char const *GetUserNameForDisplayName(char const *display) {
    int idx = name_mapping_.Find(display);
    if (idx == name_mapping_.InvalidIndex()) return "<<Invalid>>";

    CUtlSymbol sym;
    sym = name_mapping_[idx];
    return bug_strings_.String(sym);
  }

  virtual int GetSeverityCount() { return severities_.Count(); }
  virtual char const *GetSeverity(int index) {
    if (index < 0 || index >= severities_.Count()) return "<<Invalid>>";

    return bug_strings_.String(severities_[index]);
  }

  virtual int GetPriorityCount() { return priorities_.Count(); }
  virtual char const *GetPriority(int index) {
    if (index < 0 || index >= priorities_.Count()) return "<<Invalid>>";

    return bug_strings_.String(priorities_[index]);
  }

  virtual int GetAreaCount() { return areas_.Count(); }
  virtual char const *GetArea(int index) {
    if (index < 0 || index >= areas_.Count()) return "<<Invalid>>";

    return bug_strings_.String(areas_[index]);
  }

  virtual int GetAreaMapCount() { return area_maps_.Count(); }
  virtual char const *GetAreaMap(int index) {
    if (index < 0 || index >= area_maps_.Count()) return "<<Invalid>>";

    return bug_strings_.String(area_maps_[index]);
  }

  virtual int GetMapNumberCount() { return map_numbers_.Count(); }
  virtual char const *GetMapNumber(int index) {
    if (index < 0 || index >= map_numbers_.Count()) return "<<Invalid>>";

    return bug_strings_.String(map_numbers_[index]);
  }

  virtual int GetReportTypeCount() { return report_types_.Count(); }
  virtual char const *GetReportType(int index) {
    if (index < 0 || index >= report_types_.Count()) return "<<Invalid>>";

    return bug_strings_.String(report_types_[index]);
  }

  virtual char const *GetRepositoryURL() { return nullptr; }
  virtual char const *GetSubmissionURL() { return nullptr; }

  virtual int GetLevelCount(int area) { return 0; }
  virtual char const *GetLevel(int area, int index) { return ""; }

  // Submission API
  virtual void StartNewBugReport() {
    if (!bug_) {
      bug_ = new Bug();
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

    TRK_UINT rc = TrkNewRecordBegin(trk_record_handle_, kTrkRecordType);
    if (rc != TRK_SUCCESS) {
      ReportError(rc, "TrkNewRecordBegin", "Failed to TrkNewRecordBegin!");
      return false;
    }

    // Populate fields
    rc = TrkSetStringFieldValue(trk_record_handle_, "Title", bug_->title);
    if (rc != TRK_SUCCESS) {
      ReportError(rc, "TrkSetStringFieldValue", "Failed to add title!");
      return false;
    }
    rc = TrkSetStringFieldValue(trk_record_handle_, "Submitter",
                                bug_->submitter);
    if (rc != TRK_SUCCESS) {
      ReportError(rc, "TrkSetStringFieldValue", "Failed to set submitter!");
      return false;
    }
    rc = TrkSetStringFieldValue(trk_record_handle_, "Owner", bug_->owner);
    if (rc != TRK_SUCCESS) {
      ReportError(rc, "TrkSetStringFieldValue", "Failed to set owner!");
      return false;
    }
    rc = TrkSetStringFieldValue(trk_record_handle_, "Severity", bug_->severity);
    if (rc != TRK_SUCCESS) {
      ReportError(rc, "TrkSetStringFieldValue", "Failed to set severity!");
      // return false;
    }
    rc = TrkSetStringFieldValue(trk_record_handle_, "Report Type",
                                bug_->reportType);
    if (rc != TRK_SUCCESS) {
      ReportError(rc, "TrkSetStringFieldValue", "Failed to set report type!");
      // return false;
    }
    rc = TrkSetStringFieldValue(trk_record_handle_, "Priority", bug_->priority);
    if (rc != TRK_SUCCESS) {
      ReportError(rc, "TrkSetStringFieldValue", "Failed to set priority!");
      // return false;
    }
    rc = TrkSetStringFieldValue(trk_record_handle_, "Area", bug_->area);
    if (rc != TRK_SUCCESS) {
      ReportError(rc, "TrkSetStringFieldValue", "Failed to set area!");
      // return false;
    }
    rc = TrkSetStringFieldValue(trk_record_handle_, "Map Number",
                                bug_->mapNumber);
    if (rc != TRK_SUCCESS) {
      ReportError(rc, "TrkSetStringFieldValue", "Failed to set map area!");
      // return false;
    }

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
    if (bug_->bspUnc[0]) {
      buf.Printf("bsp:  %s\n", bug_->bspUnc);
    }
    if (bug_->vmfUnc[0]) {
      buf.Printf("vmf:  %s\n", bug_->vmfUnc);
    }
    if (bug_->includedFiles.Count() > 0) {
      int c = bug_->includedFiles.Count();
      for (int i = 0; i < c; ++i) {
        buf.Printf("include:  %s\n", bug_->includedFiles[i].name);
      }
    }
    if (bug_->driverInfo[0]) {
      buf.Printf("%s\n", bug_->driverInfo);
    }
    if (bug_->misc[0]) {
      buf.Printf("%s\n", bug_->misc);
    }

    buf.PutChar(0);

    rc = TrkSetDescriptionData(trk_record_handle_, buf.TellPut(),
                               (const char *)buf.Base(), 0);
    if (rc != TRK_SUCCESS) {
      ReportError(rc, "TrkSetDescriptionData",
                  "Failed to set description data!");
      return false;
    }

    TRK_TRANSACTION_ID id;
    rc = TrkNewRecordCommit(trk_record_handle_, &id);
    if (rc != TRK_SUCCESS) {
      ReportError(rc, "TrkNewRecordCommit", "Failed to TrkNewRecordCommit!");
      return false;
    }

    TRK_UINT bugId;
    rc = TrkGetNumericFieldValue(trk_record_handle_, "Id", &bugId);
    if (rc != TRK_SUCCESS) {
      ReportError(rc, "TrkGetNumericFieldValue",
                  "Failed to TrkGetNumericFieldValue for bug Id #!");
    } else {
      bugSubmissionId = (int)bugId;
    }

    rc = TrkGetSingleRecord(trk_record_handle_, bugId, kTrkRecordType);
    if (rc != TRK_SUCCESS) {
      ReportError(rc, "TrkGetSingleRecord", "Failed to open bug id for update");
      return false;
    }

    rc = TrkUpdateRecordBegin(trk_record_handle_);
    if (rc != TRK_SUCCESS) {
      ReportError(rc, "TrkUpdateRecordBegin",
                  "Failed to open bug id for update");
      return false;
    } else {
      int textbuflen = 2 * buf.TellPut() + 1;

      char *textbuf = new char[textbuflen];
      Q_memset(textbuf, 0, textbuflen);

      SubstituteBugId((int)bugId, textbuf, textbuflen, buf);

      // Update the description with the substituted text!!!
      rc = TrkSetDescriptionData(trk_record_handle_, Q_strlen(textbuf) + 1,
                                 (const char *)textbuf, 0);

      delete[] textbuf;

      if (rc != TRK_SUCCESS) {
        ReportError(rc, "TrkSetDescriptionData(update)",
                    "Failed to set description data!");
        return false;
      }

      rc = TrkUpdateRecordCommit(trk_record_handle_, &id);
      if (rc != TRK_SUCCESS) {
        ReportError(rc, "TrkUpdateRecordCommit",
                    "Failed to TrkUpdateRecordCommit for bug Id #!");
        return false;
      }
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

  // nullptr for current user
  virtual void SetSubmitter(char const *username = 0) {
    if (!username) {
      username = GetUserName();
    }

    Assert(bug_);
    Q_strncpy(bug_->submitter, username, sizeof(bug_->submitter));
  }
  virtual void SetOwner(char const *username) {
    Assert(bug_);
    Q_strncpy(bug_->owner, username, sizeof(bug_->owner));
  }
  virtual void SetSeverity(char const *severity) {
    Assert(bug_);
    Q_strncpy(bug_->severity, severity, sizeof(bug_->severity));
  }
  virtual void SetPriority(char const *priority) {
    Assert(bug_);
    Q_strncpy(bug_->priority, priority, sizeof(bug_->priority));
  }
  virtual void SetArea(char const *area) {
    Assert(bug_);
    Q_strncpy(bug_->area, area, sizeof(bug_->area));
  }
  virtual void SetMapNumber(char const *mapnumber) {
    Assert(bug_);
    Q_strncpy(bug_->mapNumber, mapnumber, sizeof(bug_->mapNumber));
  }
  virtual void SetReportType(char const *reporttype) {
    Assert(bug_);
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

  virtual void SetBSPName(char const *bsp_unc_address) {
    Assert(bug_);
    Q_strncpy(bug_->bspUnc, bsp_unc_address, sizeof(bug_->bspUnc));
  }
  virtual void SetVMFName(char const *vmf_unc_address) {
    Assert(bug_);
    Q_strncpy(bug_->vmfUnc, vmf_unc_address, sizeof(bug_->vmfUnc));
  }

  virtual void AddIncludedFile(char const *filename) {
    Bug::IncludeFile includedfile;
    Q_strncpy(includedfile.name, filename, sizeof(includedfile.name));
    bug_->includedFiles.AddToTail(includedfile);
  }
  virtual void ResetIncludedFiles() { bug_->includedFiles.Purge(); }

  virtual void SetZipAttachmentName(char const *zipfilename) {
  }  // only used by public bug reporter

  virtual void SetDriverInfo(char const *info) {
    Assert(bug_);
    Q_strncpy(bug_->driverInfo, info, sizeof(bug_->driverInfo));
  }

  virtual void SetMiscInfo(char const *info) {
    Assert(bug_);
    Q_strncpy(bug_->misc, info, sizeof(bug_->misc));
  }

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

 private:
  void ReportError(TRK_UINT rc, char const *func, char const *msg) {
    if (rc != TRK_SUCCESS) {
      switch (rc) {
        case TRK_E_ITEM_NOT_FOUND:
          Msg("%s %s was not found!\n", func, msg);
          break;
        case TRK_E_INVALID_FIELD:
          Msg("%s %s Invalid field!\n", func, msg);
          break;
        default:
          size_t i = 0;
          for (i; i < std::size(trk_error_id_name_map_); ++i) {
            if (trk_error_id_name_map_[i].id == rc) {
              Msg("%s returned %i - %s!\n", func, rc,
                  trk_error_id_name_map_[i].str);
              break;
            }
          }

          if (i >= std::size(trk_error_id_name_map_)) {
            Msg("%s returned %i - %s!\n", func, rc, "???");
          }
          break;
      }
    }
  }

  TRK_UINT Login(TRK_HANDLE *trk_handle) {
    char database_server[50];
    Q_strcpy(database_server, kDefaultDatabaseServerName);

    char project[50];
    Q_strcpy(project, kDefaultProjectName);

    char user_name[50], password[50];

    GetPrivateProfileStringA("login", "userid1",
                             kDefaultUserName,  // default
                             user_name, std::size(user_name),
                             "PVCSTRK.ini");

    // if userid1 didn't have a valid name in it try userid0
    if (!Q_stricmp(user_name, kDefaultUserName) ||
        !Q_stricmp(user_name, "BELMAPNTKY")) {
      GetPrivateProfileStringA("login", "userid0",
                               kDefaultUserName,  // default
                               user_name, std::size(user_name),
                               "PVCSTRK.ini");
    }

    // TODO(d.rattman): Exceptionally insecure, rewrite.
    Q_strncpy(password, user_name, std::size(password));

    if (file_system_) {
      KeyValues *kv = new KeyValues("tracker_login");
      if (kv) {  //-V668
        if (kv->LoadFromFile(file_system_, kTrackerSettingsFilePath)) {
          Q_strncpy(
              database_server,
              kv->GetString("database_server", kDefaultDatabaseServerName),
              std::size(database_server));
          Q_strncpy(project, kv->GetString("project_name", kDefaultProjectName),
                    std::size(project));
        }

        kv->Clear();

        // Load optional login info
        if (file_system_->FileExists(kTrackerLoginFilePath, "GAME")) {
          if (kv->LoadFromFile(file_system_, kTrackerLoginFilePath)) {
            Q_strncpy(user_name, kv->GetString("username", user_name),
                      sizeof(user_name));
            Q_strncpy(password, kv->GetString("password", password),
                      sizeof(password));
          }
        }

        kv->deleteThis();
      }
    }  //-V773

    bool maybe_no_pvcs_install{false};

    // We still don't know the username. . try to get it from the environment
    if (user_name[0] == '\0') {
      usize username_size;
      char user_name_env[50];

      if (!getenv_s(&username_size, user_name_env, "username") &&
          username_size > 0) {
        Q_strncpy(user_name, user_name_env, std::size(user_name));
        maybe_no_pvcs_install = true;
      }
    }

    user_name_ = bug_strings_.AddString(user_name);

    TRK_UINT rc =
        TrkProjectLogin(*trk_handle, user_name, password, project, nullptr,
                        nullptr, nullptr, nullptr, TRK_USE_INI_FILE_DBMS_LOGIN);

    if (rc != TRK_SUCCESS) {
      rc =
          TrkProjectLogin(*trk_handle, user_name, "", project, nullptr, nullptr,
                          nullptr, nullptr, TRK_USE_INI_FILE_DBMS_LOGIN);
      if (rc != TRK_SUCCESS) {
        if (maybe_no_pvcs_install) {
          Msg("Bug reporter failed: Make sure you have PVCS installed and that "
              "you have logged into it successfully at least once.\n");
        } else {
          Msg("Bug reporter init failed: Your tracker password must be your "
              "user name or blank.\n");
        }
        return rc;
      }
    }

    rc = TrkGetLoginDBMSName(*trk_handle, std::size(database_server),
                             database_server);
    if (rc != TRK_SUCCESS) {
      ReportError(rc, "TrkGetLoginDBMSName",
                  "Login failed (TrkGetLoginDBMSName).");
      return rc;
    }
    rc =
        TrkGetLoginProjectName(*trk_handle, std::size(project), project);
    if (rc != TRK_SUCCESS) {
      ReportError(rc, "TrkGetLoginProjectName",
                  "Login failed (TrkGetLoginProjectName).");
      return rc;
    }

    Msg("Project:  %s\n", project);
    Msg("Server:  %s\n", database_server);

    return rc;
  }

  bool PopulateLists() {
    CUtlSymbol unassigned = bug_strings_.AddString("<<Unassigned>>");
    CUtlSymbol none = bug_strings_.AddString("<<None>>");

    areas_.AddToTail(none);
    map_numbers_.AddToTail(none);
    names_.AddToTail(unassigned);
    name_mapping_.Insert("<<Unassigned>>", unassigned);
    sorted_display_names_.AddToTail(unassigned);

    PopulateChoiceList("Severity", severities_);
    PopulateChoiceList("Report Type", report_types_);
    PopulateChoiceList("Area", areas_);
    PopulateChoiceList("Area@Dir%Map", area_maps_);
    PopulateChoiceList("Map Number", map_numbers_);
    PopulateChoiceList("Priority", priorities_);

    // Need to gather  names, too
    TRK_UINT rc = TrkInitUserList(trk_handle_);
    if (TRK_SUCCESS != rc) {
      ReportError(rc, "TrkInitUserList", "Couldn't get userlist");
      return false;
    }

    char sz[256];

    while (TrkGetNextUser(trk_handle_, sizeof(sz), sz) != TRK_E_END_OF_LIST) {
      // Now lookup display name for user
      char displayname[256];

      rc =
          TrkGetUserFullName(trk_handle_, sz, sizeof(displayname), displayname);
      if (TRK_SUCCESS == rc) {
        // Fill in lookup table
        CUtlSymbol internal_name_sym = bug_strings_.AddString(sz);
        CUtlSymbol display_name_sym = bug_strings_.AddString(displayname);

        names_.AddToTail(internal_name_sym);

        name_mapping_.Insert(displayname, internal_name_sym);

        sorted_display_names_.AddToTail(display_name_sym);
      }
    }

    // Now sort display names
    int c = sorted_display_names_.Count();
    for (int i = 1; i < c; i++) {
      for (int j = i + 1; j < c; j++) {
        char const *p1 = bug_strings_.String(sorted_display_names_[i]);
        char const *p2 = bug_strings_.String(sorted_display_names_[j]);

        int cmp = Q_stricmp(p1, p2);
        if (cmp > 0) {
          CUtlSymbol t = sorted_display_names_[i];
          sorted_display_names_[i] = sorted_display_names_[j];
          sorted_display_names_[j] = t;
        }
      }
    }

    return true;
  }

  bool PopulateChoiceList(char const *listname, CUtlVector<CUtlSymbol> &list) {
    TRK_UINT rc = TrkInitChoiceList(trk_handle_, listname, kTrkRecordType);
    if (TRK_SUCCESS != rc) {
      ReportError(rc, "TrkInitChoiceList", listname);
      return false;
    }

    char sz[256];
    while (TrkGetNextChoice(trk_handle_, sizeof(sz), sz) != TRK_E_END_OF_LIST) {
      CUtlSymbol sym = bug_strings_.AddString(sz);
      list.AddToTail(sym);
    }

    return true;
  }

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
  Bug *bug_;

  CUtlSymbolTable bug_strings_;

  CUtlVector<CUtlSymbol> severities_;
  CUtlVector<CUtlSymbol> names_;
  CUtlVector<CUtlSymbol> sorted_display_names_;
  CUtlDict<CUtlSymbol, int> name_mapping_;
  CUtlVector<CUtlSymbol> priorities_;
  CUtlVector<CUtlSymbol> areas_;
  CUtlVector<CUtlSymbol> area_maps_;
  CUtlVector<CUtlSymbol> map_numbers_;
  CUtlVector<CUtlSymbol> report_types_;

  TRK_HANDLE trk_handle_;
  TRK_RECORD_HANDLE trk_record_handle_;

  CUtlSymbol user_name_;

  IBaseFileSystem *file_system_;

#define TRKERROR(id) \
  { id, #id }

  struct TRKELookup {
    unsigned int id;
    char const *str;
  };

  static const TRKELookup trk_error_id_name_map_[50];

  inline static const int kTrkRecordType{1};
  inline static const char *kTrackerSettingsFilePath{
      "resource/bugreporter.res"};
  inline static const char *kTrackerLoginFilePath{"cfg/bugreporter_login.res"};
  inline static const char *kDefaultDatabaseServerName{"tracker"};
  inline static const char *kDefaultProjectName{"Half-Life 2"};
  inline static const char *kDefaultUserName{""};
  inline static const char *kDefaultPassword{kDefaultUserName};
};

const BugReporter::TRKELookup BugReporter::trk_error_id_name_map_[50] = {
    TRKERROR(TRK_SUCCESS),
    TRKERROR(TRK_E_VERSION_MISMATCH),
    TRKERROR(TRK_E_OUT_OF_MEMORY),
    TRKERROR(TRK_E_BAD_HANDLE),
    TRKERROR(TRK_E_BAD_INPUT_POINTER),
    TRKERROR(TRK_E_BAD_INPUT_VALUE),
    TRKERROR(TRK_E_DATA_TRUNCATED),
    TRKERROR(TRK_E_NO_MORE_DATA),
    TRKERROR(TRK_E_LIST_NOT_INITIALIZED),
    TRKERROR(TRK_E_END_OF_LIST),
    TRKERROR(TRK_E_NOT_LOGGED_IN),
    TRKERROR(TRK_E_SERVER_NOT_PREPARED),
    TRKERROR(TRK_E_BAD_DATABASE_VERSION),
    TRKERROR(TRK_E_UNABLE_TO_CONNECT),
    TRKERROR(TRK_E_UNABLE_TO_DISCONNECT),
    TRKERROR(TRK_E_UNABLE_TO_START_TIMER),
    TRKERROR(TRK_E_NO_DATA_SOURCES),
    TRKERROR(TRK_E_NO_PROJECTS),
    TRKERROR(TRK_E_WRITE_FAILED),
    TRKERROR(TRK_E_PERMISSION_DENIED),
    TRKERROR(TRK_E_SET_FIELD_DENIED),
    TRKERROR(TRK_E_ITEM_NOT_FOUND),
    TRKERROR(TRK_E_CANNOT_ACCESS_DATABASE),
    TRKERROR(TRK_E_CANNOT_ACCESS_QUERY),
    TRKERROR(TRK_E_CANNOT_ACCESS_INTRAY),
    TRKERROR(TRK_E_CANNOT_OPEN_FILE),
    TRKERROR(TRK_E_INVALID_DBMS_TYPE),
    TRKERROR(TRK_E_INVALID_RECORD_TYPE),
    TRKERROR(TRK_E_INVALID_FIELD),
    TRKERROR(TRK_E_INVALID_CHOICE),
    TRKERROR(TRK_E_INVALID_USER),
    TRKERROR(TRK_E_INVALID_SUBMITTER),
    TRKERROR(TRK_E_INVALID_OWNER),
    TRKERROR(TRK_E_INVALID_DATE),
    TRKERROR(TRK_E_INVALID_STORED_QUERY),
    TRKERROR(TRK_E_INVALID_MODE),
    TRKERROR(TRK_E_INVALID_MESSAGE),
    TRKERROR(TRK_E_VALUE_OUT_OF_RANGE),
    TRKERROR(TRK_E_WRONG_FIELD_TYPE),
    TRKERROR(TRK_E_NO_CURRENT_RECORD),
    TRKERROR(TRK_E_NO_CURRENT_NOTE),
    TRKERROR(TRK_E_NO_CURRENT_ATTACHED_FILE),
    TRKERROR(TRK_E_NO_CURRENT_ASSOCIATION),
    TRKERROR(TRK_E_NO_RECORD_BEGIN),
    TRKERROR(TRK_E_NO_MODULE),
    TRKERROR(TRK_E_USER_CANCELLED),
    TRKERROR(TRK_E_SEMAPHORE_TIMEOUT),
    TRKERROR(TRK_E_SEMAPHORE_ERROR),
    TRKERROR(TRK_E_INVALID_SERVER_NAME),
    TRKERROR(TRK_E_NOT_LICENSED)};

EXPOSE_SINGLE_INTERFACE(BugReporter, IBugReporter,
                        INTERFACEVERSION_BUGREPORTER);
