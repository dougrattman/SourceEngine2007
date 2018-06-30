// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Utilities for setting vproject settings.

#include "tier2/vconfig.h"

#include "tier0/include/platform.h"

#ifdef OS_WIN
#include "base/include/windows/windows_light.h"

#include <direct.h>
#include <io.h>  // _chmod
#include <process.h>
#endif

#ifdef OS_WIN
// Returns the string value of a registry key.
bool GetVConfigRegistrySetting(const char *pName, char *pReturn, int size) {
  // Open the key
  HKEY hregkey;
  if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, VPROJECT_REG_KEY, 0, KEY_QUERY_VALUE,
                   &hregkey) != ERROR_SUCCESS)
    return false;

  // Get the value
  DWORD dwSize = size;
  if (RegQueryValueEx(hregkey, pName, nullptr, nullptr, (LPBYTE)pReturn,
                      &dwSize) != ERROR_SUCCESS)
    return false;

  // Close the key
  RegCloseKey(hregkey);

  return true;
}

// Sends a global system message to alert programs to a changed environment
// variable.
void NotifyVConfigRegistrySettingChanged() {
  DWORD_PTR dwReturnValue = 0;

  // Propagate changes so that environment variables takes immediate effect!
  SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0,
                      (LPARAM)L"Environment", SMTO_ABORTIFHUNG, 5000,
                      &dwReturnValue);
}

// Set the registry entry to a string value, under the given subKey.
void SetVConfigRegistrySetting(const char *pName, const char *pValue,
                               bool bNotify) {
  HKEY hregkey;

  // Open the key
  if (RegCreateKeyEx(
          HKEY_LOCAL_MACHINE,      // base key
          VPROJECT_REG_KEY,        // subkey
          0,                       // reserved
          0,                       // lpClass
          0,                       // options
          (REGSAM)KEY_ALL_ACCESS,  // access desired
          nullptr,                 // security attributes
          &hregkey,                // result
          nullptr  // tells if it created the key or not (which we don't care)
          ) != ERROR_SUCCESS) {
    return;
  }

  // Set the value to the string passed in
  int nType = strchr(pValue, '%') ? REG_EXPAND_SZ : REG_SZ;
  RegSetValueEx(hregkey, pName, 0, nType, (const unsigned char *)pValue,
                strlen(pValue));

  // Notify other programs
  if (bNotify) NotifyVConfigRegistrySettingChanged();

  // Close the key
  RegCloseKey(hregkey);
}

// Removes the obsolete user keyvalue.
bool RemoveObsoleteVConfigRegistrySetting(const char *pValueName,
                                          char *pOldValue, int size) {
  // Open the key
  HKEY hregkey;
  if (RegOpenKeyEx(HKEY_CURRENT_USER, "Environment", 0, (REGSAM)KEY_ALL_ACCESS,
                   &hregkey) != ERROR_SUCCESS)
    return false;

  // Return the old state if they've requested it
  if (pOldValue != nullptr) {
    DWORD dwSize = size;

    // Get the value
    if (RegQueryValueEx(hregkey, pValueName, nullptr, nullptr,
                        (LPBYTE)pOldValue, &dwSize) != ERROR_SUCCESS)
      return false;
  }

  // Remove the value
  if (RegDeleteValue(hregkey, pValueName) != ERROR_SUCCESS) return false;

  // Close the key
  RegCloseKey(hregkey);

  // Notify other programs
  NotifyVConfigRegistrySettingChanged();

  return true;
}

// Take a user-defined environment variable and swap it out for the internally
// used one.
bool ConvertObsoleteVConfigRegistrySetting(const char *pValueName) {
  char szValue[SOURCE_MAX_PATH];
  if (RemoveObsoleteVConfigRegistrySetting(pValueName, szValue,
                                           sizeof(szValue))) {
    // Set it up the correct way
    SetVConfigRegistrySetting(pValueName, szValue);
    return true;
  }

  return false;
}
#endif
