// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#include "bitbuf_errorhandler.h"

#include "tier0/include/dbg.h"
#include "tier1/bitbuf.h"
#include "tier1/utlsymbol.h"

#include "tier0/include/memdbgon.h"

void EngineBitBufErrorHandler(BitBufErrorType errorType,
                              const char *pDebugName) {
  if (!pDebugName) {
    pDebugName = "(unknown)";
  }

  static CUtlSymbolTable errorNames[BITBUFERROR_NUM_ERRORS];

  // Only print an error a couple times.
  CUtlSymbol sym = errorNames[errorType].Find(pDebugName);
  if (UTL_INVAL_SYMBOL == sym) {
    errorNames[errorType].AddString(pDebugName);
    if (errorType == BITBUFERROR_VALUE_OUT_OF_RANGE) {
      Warning(
          "Error in bitbuf [%s]: out of range value. Debug in "
          "bitbuf_errorhandler.cpp\n",
          pDebugName);
    } else if (errorType == BITBUFERROR_BUFFER_OVERRUN) {
      Warning(
          "Error in bitbuf [%s]: buffer overrun. Debug in "
          "bitbuf_errorhandler.cpp\n",
          pDebugName);
    }
  }

  Assert(0);
}

void InstallBitBufErrorHandler() {
  SetBitBufErrorHandler(EngineBitBufErrorHandler);
}
