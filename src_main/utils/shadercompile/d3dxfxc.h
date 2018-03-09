// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: D3DX command implementation.

#ifndef D3DXFXC_H
#define D3DXFXC_H

#include "cmdsink.h"

namespace InterceptFxc {
bool TryExecuteCommand(const char *pCommand, CmdSink::IResponse **ppResponse);
};  // namespace InterceptFxc

#endif  // D3DXFXC_H
