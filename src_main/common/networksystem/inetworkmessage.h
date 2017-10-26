// Copyright © 1996-2005, Valve Corporation, All rights reserved.
//
// Purpose: INetworkMessage interface

#ifndef INETWORKMESSAGE_H
#define INETWORKMESSAGE_H

#include "tier0/dbg.h"
#include "tier0/platform.h"

// Forward declarations
class bf_read;
class bf_write;
class INetMsgHandler;
class INetMessage;
class INetChannel;

// First valid group number users of the network system can use
enum {
  NETWORKSYSTEM_FIRST_GROUP = 1,
};

// A network message
abstract_class INetworkMessage {
 public:
  // Use these to setup who can hear whose voice.
  // Pass in client indices (which are their ent indices - 1).
  // netchannel this message is from/for
  virtual void SetNetChannel(INetChannel * netchan) = 0;
  // set to true if it's a reliable message
  virtual void SetReliable(bool state) = 0;
  // returns true if parsing was OK
  virtual bool ReadFromBuffer(bf_read & buffer) = 0;
  // returns true if writing was OK
  virtual bool WriteToBuffer(bf_write & buffer) = 0;
  // true, if message needs reliable handling
  virtual bool IsReliable() const = 0;
  // returns net message group of this message
  virtual int GetGroup() const = 0;
  // returns module specific header tag eg svc_serverinfo
  virtual int GetType() const = 0;
  // returns network message group name
  virtual const char *GetGroupName() const = 0;
  // returns network message name, eg "svc_serverinfo"
  virtual const char *GetName() const = 0;
  virtual INetChannel *GetNetChannel() const = 0;
  // returns a human readable string about message content
  virtual const char *ToString() const = 0;

  virtual void Release() = 0;

 protected:
  virtual ~INetworkMessage(){};
};

// Helper utilities for clients to create messages
#define DECLARE_BASE_MESSAGE(group, msgtype, description)      \
 public:                                                       \
  virtual bool ReadFromBuffer(bf_read &buffer);                \
  virtual bool WriteToBuffer(bf_write &buffer);                \
  virtual const char *ToString() const { return description; } \
  virtual int GetGroup() const { return group; }               \
  virtual const char *GetGroupName() const { return #group; }  \
  virtual int GetType() const { return msgtype; }              \
  virtual const char *GetName() const { return #msgtype; }

// Default empty base class for net messages
class CNetworkMessage : public INetworkMessage {
 public:
  CNetworkMessage() : m_bReliable{true}, m_pNetChannel{nullptr} {}

  virtual void Release() { delete this; }

  virtual ~CNetworkMessage(){};

  virtual void SetReliable(bool state) { m_bReliable = state; }

  virtual bool IsReliable() const { return m_bReliable; }

  virtual void SetNetChannel(INetChannel *netchan) { m_pNetChannel = netchan; }

  virtual bool Process() {
    // no handler set
    Assert(0);
    return false;
  }

  INetChannel *GetNetChannel() const { return m_pNetChannel; }

 protected:
  bool m_bReliable;            // true if message should be send reliable
  INetChannel *m_pNetChannel;  // netchannel this message is from/for
};

#endif  // INETWORKMESSAGE_H
