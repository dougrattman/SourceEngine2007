// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "traceinit.h"

#include "common.h"
#include "sys.h"
#include "tier0/include/basetypes.h"
#include "tier1/UtlVector.h"

#include "tier0/include/memdbgon.h"

namespace {
// Tracks init/shutdown sequence.
class InitializationTracker {
 public:
  static InitializationTracker &Instance() {
    static InitializationTracker initialization_tracker;
    return initialization_tracker;
  }

  ~InitializationTracker() {
    usize l = 0;
    for (auto &func : init_function_counts_) {
      // assert( m_nNumFuncs[l] == 0 );
      for (usize i = 0; i < func; i++) {
        InitFunction *f = init_functions_[l][i];
        if (f->RefCount) {
          Msg("Missing shutdown function for %s : %s.\n", f->InitName,
              f->ShutdownName);
        }
        delete f;
      }
      init_functions_[l++].RemoveAll();
      func = 0;
    }
  }

  bool Init(const ch *init, const ch *shutdown, usize listnum) {
    auto *f = new InitFunction{init, shutdown, 1, false, 0.0f, 0.0f};
    if (f) {
      init_functions_[listnum].AddToHead(f);
      init_function_counts_[listnum]++;
      return true;
    }

    return false;
  }
  bool Shutdown(const ch *shutdown, usize listnum) {
    if (!init_function_counts_[listnum]) {
      Msg("Mismatched shutdown function %s.\n", shutdown);
      return false;
    }

    InitFunction *f = nullptr;
    for (usize i = 0; i < init_function_counts_[listnum]; i++) {
      f = init_functions_[listnum][i];
      if (f->RefCount) break;
    }

    if (f && f->RefCount && _stricmp(f->ShutdownName, shutdown)) {
      if (!f->IsWarningPrinted) {
        f->IsWarningPrinted = true;
        Msg("Shutdown function %s called out of order, expecting %s.\n",
            shutdown, f->ShutdownName);
      }
    }

    for (usize i = 0; i < init_function_counts_[listnum]; i++) {
      InitFunction *ff = init_functions_[listnum][i];

      if (!_stricmp(ff->ShutdownName, shutdown)) {
        Assert(ff->RefCount);
        ff->RefCount--;
        return true;
      }
    }

    Msg("Shutdown function %s not in list.\n", shutdown);
    return false;
  }

 private:
  struct InitFunction {
    const ch *InitName, *ShutdownName;
    usize RefCount;
    bool IsWarningPrinted;
    // Profiling data
    f32 InitStamp, ShutdownStamp;
  };

  InitializationTracker() {
    for (auto &func : init_function_counts_) func = 0;
  }

  usize init_function_counts_[4];
  CUtlVector<InitFunction *> init_functions_[4];
};
}  // namespace

bool TraceInit(const ch *i, const ch *s, usize listnum) {
  Plat_TimestampedLog("TraceInit: %s", i);
  return InitializationTracker::Instance().Init(i, s, listnum);
}

bool TraceShutdown(const ch *s, usize listnum) {
  Plat_TimestampedLog("TraceShutdown: %s", s);
  return InitializationTracker::Instance().Shutdown(s, listnum);
}
