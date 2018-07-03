// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_VGUI_IMESSAGELISTENER_H_
#define SOURCE_VGUI_IMESSAGELISTENER_H_

#ifdef _WIN32
#pragma once
#endif

#include "base/include/compiler_specific.h"
#include "vgui/VGUI.h"

class KeyValues;

namespace vgui {
enum MessageSendType_t { MESSAGE_SENT = 0, MESSAGE_POSTED, MESSAGE_RECEIVED };

class VPanel;

the_interface IMessageListener {
 public:
  virtual void Message(VPanel* pSender, VPanel* pReceiver,
                       KeyValues* pKeyValues, MessageSendType_t type) = 0;
};

IMessageListener* MessageListener();
}  // namespace vgui

#endif  // SOURCE_VGUI_IMESSAGELISTENER_H_
