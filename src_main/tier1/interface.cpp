// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "tier1/interface.h"

#include "build/include/build_config.h"

#ifdef OS_WIN
#include <direct.h>  // _getcwd
#include "base/include/windows/windows_light.h"
#elif OS_POSIX
#define _getcwd getcwd
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>

#if !defined(DONT_PROTECT_FILEIO_FUNCTIONS)
#define DONT_PROTECT_FILEIO_FUNCTIONS  // for protected_things.h
#endif

#if defined(PROTECTED_THINGS_ENABLE)
#undef PROTECTED_THINGS_ENABLE  // from protected_things.h
#endif

#include "tier0/include/basetypes.h"
#include "tier0/include/dbg.h"
#include "tier0/include/icommandline.h"
#include "tier0/include/threadtools.h"
#include "tier1/strtools.h"

 
#include "tier0/include/memdbgon.h"

// InterfaceReg.
InterfaceReg *InterfaceReg::s_pInterfaceRegs = nullptr;

InterfaceReg::InterfaceReg(InstantiateInterfaceFn instantiate_interface_func,
                           const char *interface_name)
    : m_CreateFn{instantiate_interface_func},
      m_pName{interface_name},
      m_pNext{s_pInterfaceRegs} {
  s_pInterfaceRegs = this;
}

// CreateInterface. This is the primary exported function by a dll, referenced
// by name via dynamic binding that exposes an opqaue function pointer to the
// interface.
void *CreateInterface(const char *interface_name, int *return_code) {
  for (auto *it = InterfaceReg::s_pInterfaceRegs; it; it = it->m_pNext) {
    if (strcmp(it->m_pName, interface_name) == 0) {
      if (return_code) {
        *return_code = IFACE_OK;
      }
      return it->m_CreateFn();
    }
  }

  if (return_code) {
    *return_code = IFACE_FAILED;
  }

  return nullptr;
}

#ifdef OS_POSIX
// Linux doesn't have this function so this emulates its functionality
void *GetModuleHandle(const char *name) {
  if (name == nullptr) {
    // hmm, how can this be handled under linux....
    // is it even needed?
    return nullptr;
  }

  void *handle;
  if ((handle = dlopen(name, RTLD_NOW)) == nullptr) {
    fprintf(stderr, "dlopen %s error:%s\n", name, dlerror());
    // couldn't open this file
    return nullptr;
  }

  // read "man dlopen" for details in short dlopen() inc a ref count so dec the
  // ref count by performing the close
  dlclose(handle);
  return handle;
}
#endif

static void *Sys_GetProcAddress(HMODULE module, const char *proc_name) {
  return GetProcAddress(module, proc_name);
}

// Purpose: returns a pointer to a function, given a module
static void *Sys_GetProcAddress(const char *module_name,
                                const char *proc_name) {
  const HMODULE module = GetModuleHandle(module_name);
  return Sys_GetProcAddress(module, proc_name);
}

bool Sys_IsDebuggerPresent() { return Plat_IsInDebugSession(); }

struct ThreadedLoadLibraryContext_t {
  ThreadedLoadLibraryContext_t(const char *the_library_name,
                               HMODULE the_library_module)
      : library_name{the_library_name}, library_module{the_library_module} {}

  const char *library_name;
  HMODULE library_module;
};

#ifdef OS_WIN
static HMODULE InternalLoadLibrary(const char *library_path) {
  return LoadLibraryEx(library_path, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
}
unsigned int ThreadedLoadLibraryFunc(void *parameter) {
  auto *const context = static_cast<ThreadedLoadLibraryContext_t *>(parameter);
  context->library_module = InternalLoadLibrary(context->library_name);
  return 0;
}
#endif

HMODULE Sys_LoadLibrary(const char *library_path) {
#ifdef OS_WIN
  constexpr char const *module_extension = ".dll";
  constexpr char const *module_addition = module_extension;
#elif OS_POSIX
  constexpr char const *module_extension = ".so";
  constexpr char const *module_addition = module_extension;
#endif

  char fixed_library_path[1024];
  Q_strncpy(fixed_library_path, library_path, SOURCE_ARRAYSIZE(fixed_library_path));

  if (!Q_stristr(fixed_library_path, module_extension))
    Q_strncat(fixed_library_path, module_addition,
              SOURCE_ARRAYSIZE(fixed_library_path));

  Q_FixSlashes(fixed_library_path);

#ifdef OS_WIN

  const ThreadedLoadLibraryFunc_t thread_func = GetThreadedLoadLibraryFunc();
  if (!thread_func) return InternalLoadLibrary(fixed_library_path);

  ThreadedLoadLibraryContext_t context{fixed_library_path, nullptr};
  ThreadHandle_t thread_handle =
      CreateSimpleThread(ThreadedLoadLibraryFunc, &context);

  unsigned int timeout = 0;
  while (ThreadWaitForObject(thread_handle, true, timeout) == TW_TIMEOUT) {
    timeout = thread_func();
  }

  ReleaseThreadHandle(thread_handle);
  return context.library_module;

#elif OS_POSIX

  const HMODULE module = dlopen(fixed_library_path, RTLD_NOW);
  if (!module) {
    const char *error_msg = dlerror();
    if (error_msg && (strstr(error_msg, "No such file") == 0)) {
      Msg("Failed to dlopen %s, error: %s\n", fixed_library_path, pError);
    }
  }

  return module;

#endif
}

static HMODULE LoadModuleByRelativePath(const char *module_name) {
  char current_directory[1024];

  if (!Q_IsAbsolutePath(module_name) &&
      // Full path wasn't passed in, using the current working dir.
      _getcwd(current_directory, SOURCE_ARRAYSIZE(current_directory))) {
    size_t current_directory_length = strlen(current_directory) - 1;

    if (current_directory[current_directory_length] == '/' ||
        current_directory[current_directory_length] == '\\') {
      current_directory[current_directory_length] = '\0';
    }

    char absolute_module_name[1024];
    if (strstr(module_name, "bin/") == module_name) {
      // Don't make bin/bin path.
      Q_snprintf(absolute_module_name, SOURCE_ARRAYSIZE(absolute_module_name), "%s/%s",
                 current_directory, module_name);
    } else {
      Q_snprintf(absolute_module_name, SOURCE_ARRAYSIZE(absolute_module_name),
                 "%s/bin/%s", current_directory, module_name);
    }

    return Sys_LoadLibrary(absolute_module_name);
  }

  return nullptr;
}

#ifdef OS_WIN
static DWORD SpewModuleLoadError(_In_z_ const char *module_name,
                                 _In_ DWORD error_code = GetLastError()) {
  char *system_error;

  if (!FormatMessageA(
          FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
              FORMAT_MESSAGE_IGNORE_INSERTS,
          nullptr, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
          (LPSTR)&system_error, 0, nullptr)) {
    Warning("Module %s load error: N/A.", module_name);
    return error_code;
  }

  Warning("Module %s load error: %s", module_name, system_error);
  LocalFree(system_error);

  return error_code;
}
#endif

// Purpose: Loads a DLL/component from disk and returns a handle to it
CSysModule *Sys_LoadModule(const char *module_name) {
  // If using the Steam file system, either the DLL must be a minimum footprint
  // file in the depot (MFP) or a file system GetLocalCopy() call must be made
  // prior to the call to this routine.
  HMODULE module = LoadModuleByRelativePath(module_name);

  if (!module) {
    // Full path failed, let LoadLibrary() try to search the PATH now
    module = Sys_LoadLibrary(module_name);

    if (!module) {
#ifdef OS_WIN
      SpewModuleLoadError(module_name);
#else
      Error("Failed to load %s: %s\n", module_name, dlerror());
#endif  // OS_WIN
    }
  }

  // If running in the debugger, assume debug binaries are okay, otherwise they
  // must run with -allowdebug
  if (module && !CommandLine()->FindParm("-allowdebug") &&
      !Sys_IsDebuggerPresent()) {
    if (Sys_GetProcAddress(module, "BuiltDebug")) {
      Error("Module %s is a debug build\n", module_name);
    }
  }

  return reinterpret_cast<CSysModule *>(module);
}

// Purpose: Unloads a DLL/component from
BOOL Sys_UnloadModule(CSysModule *module_handle) {
#ifdef OS_WIN
  const HMODULE module = reinterpret_cast<HMODULE>(module_handle);
  return module ? FreeLibrary(module) : TRUE;
#elif OS_POSIX
  if (module_handle) {
    dlclose((void *)module_handle);
  }

  return TRUE;
#endif
}

// Purpose: returns a pointer to a function, given a module
CreateInterfaceFn Sys_GetFactory(CSysModule *module_handle) {
#ifdef OS_WIN
  const HMODULE module = reinterpret_cast<HMODULE>(module_handle);
  return module ? reinterpret_cast<CreateInterfaceFn>(
                      GetProcAddress(module, CREATEINTERFACE_PROCNAME))
                : nullptr;
#elif OS_POSIX
  return module_handle ? reinterpret_cast<CreateInterfaceFn>(GetProcAddress(
                             module_handle, CREATEINTERFACE_PROCNAME))
                       : nullptr;
#endif
}

// Purpose: returns the instance of this module
CreateInterfaceFn Sys_GetFactoryThis() { return CreateInterface; }

// Purpose: returns the instance of the named module
CreateInterfaceFn Sys_GetFactory(const char *module_name) {
  return static_cast<CreateInterfaceFn>(
      Sys_GetProcAddress(module_name, CREATEINTERFACE_PROCNAME));
}

// Purpose: get the interface for the specified module and version.
bool Sys_LoadInterface(const char *module_name,
                       const char *interface_version_name,
                       CSysModule **out_module, void **out_interface) {
  CSysModule *module = Sys_LoadModule(module_name);
  if (!module) return false;

  CreateInterfaceFn create_interface_func = Sys_GetFactory(module);
  if (!create_interface_func) {
    Sys_UnloadModule(module);
    return false;
  }

  *out_interface = (*create_interface_func)(interface_version_name, nullptr);
  if (!*out_interface) {
    Sys_UnloadModule(module);
    return false;
  }

  if (out_module) *out_module = module;

  return true;
}

// Purpose: Place this as a singleton at module scope (e.g.) and use it to get
// the factory from the specified module name.
CDllDemandLoader::CDllDemandLoader(char const *module_path)
    : module_path_{module_path}, module_{nullptr}, is_load_attempted_{false} {}

CDllDemandLoader::~CDllDemandLoader() {
  if (module_) {
    Sys_UnloadModule(module_);
    module_ = nullptr;
  }
}

CreateInterfaceFn CDllDemandLoader::GetFactory() {
  if (!module_ && !is_load_attempted_) {
    is_load_attempted_ = true;
    module_ = Sys_LoadModule(module_path_);
  }

  return module_ ? Sys_GetFactory(module_) : nullptr;
}
