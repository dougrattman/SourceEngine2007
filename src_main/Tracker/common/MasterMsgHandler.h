// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef MASTERMSGHANDLER_H
#define MASTERMSGHANDLER_H

#include "Socket.h"

// Purpose: Socket handler for lan broadcast pings
class CMasterMsgHandler : public CMsgHandler {
 public:
  CMasterMsgHandler(IGameList *baseobject, HANDLERTYPE type,
                    void *typeinfo = NULL);

  virtual bool Process(netadr_t *from, CMsgBuffer *msg);

 private:
  IGameList *m_pGameList;
};

#endif  // MASTERMSGHANDLER_H
