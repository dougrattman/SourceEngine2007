// Copyright © 1996-2006, Valve Corporation, All rights reserved.

#include "resource.h"

#include "wmi.h"

#define _WIN32_DCOM
#include <Wbemidl.h>
#include <comdef.h>
#include <comip.h>
#include <cassert>

#include "base/include/windows/com_ptr.h"
#include "base/include/windows/scoped_com_initializer.h"
#include "tier0/include/commonmacros.h"
#include "tier0/include/dbg.h"

#pragma comment(lib, "wbemuuid.lib")

#ifdef NDEBUG
#pragma comment(lib, "comsuppw.lib")
#else
#pragma comment(lib, "comsuppwd.lib")
#endif

// TODO: Do not ignore adapter idx.
std::tuple<u64, HRESULT> GetVidMemBytes([[maybe_unused]] u32 adapter_idx) {
  static bool has_value = false;
  static u64 video_memory_bytes = 0;

  if (has_value) {
    return {video_memory_bytes, S_OK};
  }

  has_value = true;

  // COM is needed for WMI calls.
  source::windows::ScopedComInitializer scoped_com_initializer{
      static_cast<COINIT>(COINIT_APARTMENTTHREADED | COINIT_SPEED_OVER_MEMORY |
                          COINIT_DISABLE_OLE1DDE)};
  HRESULT hr{scoped_com_initializer.hr()};
  if (FAILED(hr)) {
    Error("GetVidMemBytes: COM initialization failure, hr 0x%0.8x.\n", hr);
    return {0, hr};
  }

  // Set general COM security levels.
  hr = CoInitializeSecurity(
      nullptr,
      -1,                           // COM authentication
      nullptr,                      // Authentication services
      nullptr,                      // Reserved
      RPC_C_AUTHN_LEVEL_DEFAULT,    // Default authentication
      RPC_C_IMP_LEVEL_IMPERSONATE,  // Default Impersonation
      nullptr,                      // Authentication info
      EOAC_NONE,                    // Additional capabilities
      nullptr);
  if (FAILED(hr)) {
    Error("GetVidMemBytes: COM security initialization failure, hr 0x%0.8x.\n",
          hr);
    return {0, hr};
  }

  // Obtain the initial locator to WMI.
  source::windows::com_ptr<IWbemLocator> wbem_locator;
  hr = wbem_locator.CreateInstance(CLSID_WbemLocator, nullptr,
                                   CLSCTX_INPROC_SERVER);
  if (FAILED(hr)) {
    Error("GetVidMemBytes: Failed to create IWbemLocator object, hr 0x%0.8x.\n",
          hr);
    return {0, hr};
  }

  // Connect to the root\cimv2 namespace with the current user and obtain
  // pointer wbem_services to make IWbemServices calls.
  source::windows::com_ptr<IWbemServices> wbem_services;
  hr = wbem_locator->ConnectServer(
      bstr_t(L"ROOT\\CIMV2"),  // Object path of WMI namespace
      nullptr,                 // User name. nullptr = current user
      nullptr,                 // User password. nullptr = current
      nullptr,                 // Locale. nullptr indicates current
      0,                       // Security flags.
      nullptr,                 // Authority (e.g. Kerberos)
      nullptr,                 // Context object
      &wbem_services);         // pointer to IWbemServices proxy
  if (FAILED(hr)) {
    Error(
        "GetVidMemBytes: Could not connect to ROOT\\CIMV2 server, hr "
        "0x%0.8x.\n",
        hr);
    return {0, hr};
  }

  // Set security levels on the proxy.
  hr = CoSetProxyBlanket(wbem_services,           // Indicates the proxy to set
                         RPC_C_AUTHN_WINNT,       // RPC_C_AUTHN_xxx
                         RPC_C_AUTHZ_NONE,        // RPC_C_AUTHZ_xxx
                         nullptr,                 // Server principal name
                         RPC_C_AUTHN_LEVEL_CALL,  // RPC_C_AUTHN_LEVEL_xxx
                         RPC_C_IMP_LEVEL_IMPERSONATE,  // RPC_C_IMP_LEVEL_xxx
                         nullptr,                      // client identity
                         EOAC_NONE);                   // proxy capabilities
  if (FAILED(hr)) {
    Error("GetVidMemBytes: Could not set proxy blanket, hr 0x%0.8x.\n", hr);
    return {0, hr};
  }

  // Use the IWbemServices pointer to make requests of WMI.
  source::windows::com_ptr<IEnumWbemClassObject> wbem_enumerator;
  hr = wbem_services->ExecQuery(
      bstr_t("WQL"), bstr_t("SELECT * FROM Win32_VideoController"),
      WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr,
      &wbem_enumerator);
  if (FAILED(hr)) {
    Error(
        "GetVidMemBytes: Could not query Win32_VideoController, hr "
        "0x%0.8x.\n",
        hr);
    return {0, hr};
  }

  // Get the data from the above query
  source::windows::com_ptr<IWbemClassObject> wbem_class_object;
  ULONG wbem_class_object_count = 0;

  while (true) {
    hr = wbem_enumerator->Next(WBEM_INFINITE, 1, &wbem_class_object,
                               &wbem_class_object_count);
    if (FAILED(hr) || 0 == wbem_class_object_count) break;

    variant_t property;
    hr = wbem_class_object->Get(L"AdapterRAM", 0, &property, nullptr, nullptr);

    if (SUCCEEDED(hr)) {
      video_memory_bytes = implicit_cast<u32>(property);
    }

    wbem_class_object.Release();
  }

  // TODO: wbem_class_object.Release();

  return {video_memory_bytes, hr};
}
