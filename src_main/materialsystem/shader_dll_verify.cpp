// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "shader_dll_verify.h"
#include "base/include/windows/windows_light.h"

static unsigned char *g_pLastInputData = nullptr;
static HANDLE g_hDLLInst = nullptr;

BOOL WINAPI DllMain(_In_ HINSTANCE instance, _In_ DWORD call_reason,
                    _In_ LPVOID) {
  switch (call_reason) {
    case DLL_PROCESS_ATTACH:
      ::DisableThreadLibraryCalls(instance);
      g_hDLLInst = instance;
      break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
      break;
    default:
      assert(false);
  }

  return TRUE;
}

class CShaderDLLVerification : public IShaderDLLVerification {
 public:
  virtual CRC32_t Function1(unsigned char *pData);
  virtual void Function2(int a, int b, int c);
  virtual void Function3(int a, int b, int c);
  virtual void Function4(int a, int b, int c);
  virtual CRC32_t Function5();
};

static CShaderDLLVerification g_Blah;

// The main exported function.. return a pointer to g_Blah.
extern "C" void __declspec(dllexport) _ftol3(char *pData) {
  pData += SHADER_DLL_VERIFY_DATA_PTR_OFFSET;
  char *pToFillIn = (char *)&g_Blah;
  memcpy(pData, &pToFillIn, 4);
}

CRC32_t CShaderDLLVerification::Function1(unsigned char *pData) {
  pData += SHADER_DLL_VERIFY_DATA_PTR_OFFSET;
  g_pLastInputData = (unsigned char *)pData;

  void *pVerifyPtr1 = &g_Blah;

  CRC32_t testCRC;
  CRC32_Init(&testCRC);
  CRC32_ProcessBuffer(&testCRC, pData, SHADER_DLL_VERIFY_DATA_LEN1);
  CRC32_ProcessBuffer(&testCRC, &g_hDLLInst, 4);
  CRC32_ProcessBuffer(&testCRC, &pVerifyPtr1, 4);
  CRC32_Final(&testCRC);

  return testCRC;
}

void CShaderDLLVerification::Function2(int a, int b, int c) {
  a = b = c;
  MD5Context_t md5Context;
  MD5Init(&md5Context);
  MD5Update(&md5Context, g_pLastInputData + SHADER_DLL_VERIFY_DATA_PTR_OFFSET,
            SHADER_DLL_VERIFY_DATA_LEN1 - SHADER_DLL_VERIFY_DATA_PTR_OFFSET);
  MD5Final(g_pLastInputData, &md5Context);
}

void CShaderDLLVerification::Function3(int a, int b, int c) { a = b = c; }

void CShaderDLLVerification::Function4(int a, int b, int c) { a = b = c; }

CRC32_t CShaderDLLVerification::Function5() { return 32423; }
