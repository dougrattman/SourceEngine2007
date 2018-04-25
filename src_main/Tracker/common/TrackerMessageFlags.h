// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef TRACKERMESSAGEFLAGS_H
#define TRACKERMESSAGEFLAGS_H

// Purpose: the set of flags that can modify a message
enum TrackerMessageFlags_e {
  MESSAGEFLAG_SYSTEM = 1 << 2,     // message is from the tracker system
  MESSAGEFLAG_MARKETING = 1 << 3,  // message is from the tracker system
};

#endif  // TRACKERMESSAGEFLAGS_H
