// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#ifndef SOURCE_TIER3_SCENETOKENPROCESSOR_H_
#define SOURCE_TIER3_SCENETOKENPROCESSOR_H_

class ISceneTokenProcessor;

ISceneTokenProcessor *GetTokenProcessor();
void SetTokenProcessorBuffer(const char *buf);

#endif  // SOURCE_TIER3_SCENETOKENPROCESSOR_H_
