// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "tier0/include/tslist.h"

namespace TSListTests {
i32 NUM_TEST = 10000;
i32 NUM_THREADS;
i32 MAX_THREADS = 8;
i32 NUM_PROCESSORS = 1;

CInterlockedInt g_nTested;
CInterlockedInt g_nThreads;
CInterlockedInt g_nPushThreads;
CInterlockedInt g_nPopThreads;
CInterlockedInt g_nPushes;
CInterlockedInt g_nPops;
CTSQueue<i32, true> g_TestQueue;
CTSList<i32> g_TestList;
volatile bool g_bStart;
i32 *g_pTestBuckets;

CTSListBase g_Test;
TSLNodeBase_t **g_nodes;
i32 idx = 0;

const ch *g_pListType;

class CTestOps {
 public:
  virtual void Push(i32 item) = 0;
  virtual bool Pop(i32 *pResult) = 0;
  virtual bool Validate() { return true; }
  virtual bool IsEmpty() = 0;
};

class CQueueOps : public CTestOps {
  void Push(i32 item) {
    g_TestQueue.PushItem(item);
    ++g_nPushes;
  }
  bool Pop(i32 *pResult) {
    if (g_TestQueue.PopItem(pResult)) {
      ++g_nPops;
      return true;
    }
    return false;
  }
  bool Validate() { return g_TestQueue.Validate(); }
  bool IsEmpty() { return (g_TestQueue.Count() == 0); }
} g_QueueOps;

class CListOps : public CTestOps {
  void Push(i32 item) {
    g_TestList.PushItem(item);
    ++g_nPushes;
  }
  bool Pop(i32 *pResult) {
    if (g_TestList.PopItem(pResult)) {
      ++g_nPops;
      return true;
    }
    return false;
  }
  bool Validate() { return true; }
  bool IsEmpty() { return (g_TestList.Count() == 0); }
} g_ListOps;

CTestOps *g_pTestOps;

void ClearBuckets(i32 *pTestBuckets) {
  memset(pTestBuckets, 0, sizeof(i32) * NUM_TEST);
}

void IncBucket(i32 *pTestBuckets, i32 i) {
  if (i < NUM_TEST)  // tests can slop over a bit
  {
    ThreadInterlockedIncrement(&pTestBuckets[i]);
  }
}

void DecBucket(i32 *pTestBuckets, i32 i) {
  if (i < NUM_TEST)  // tests can slop over a bit
  {
    ThreadInterlockedDecrement(&pTestBuckets[i]);
  }
}

void ValidateBuckets(i32 *pTestBuckets) {
  for (i32 i = 0; i < NUM_TEST; i++) {
    if (pTestBuckets[i] != 0) {
      Msg("Test bucket %d has an invalid value %d\n", i, pTestBuckets[i]);
      DebuggerBreakIfDebugging();
      return;
    }
  }
}

u32 PopThreadFunc(void *) {
  ThreadSetDebugName("PopThread");
  ++g_nPopThreads;
  ++g_nThreads;
  while (!g_bStart) {
    ThreadSleep(0);
  }
  i32 ignored;
  for (;;) {
    if (!g_pTestOps->Pop(&ignored)) {
      if (g_nPushThreads == 0) {
        // Pop the rest
        while (g_pTestOps->Pop(&ignored)) {
          ThreadSleep(0);
        }
        break;
      }
    }
  }
  --g_nThreads;
  --g_nPopThreads;
  return 0;
}

u32 PushThreadFunc(void *) {
  ThreadSetDebugName("PushThread");
  ++g_nPushThreads;
  ++g_nThreads;
  while (!g_bStart) {
    ThreadSleep(0);
  }

  while (g_nTested < NUM_TEST) {
    g_pTestOps->Push(g_nTested);
    ++g_nTested;
  }
  --g_nThreads;
  --g_nPushThreads;
  return 0;
}

void TestStart(i32 *pTestBuckets) {
  g_nTested = 0;
  g_nThreads = 0;
  g_nPushThreads = 0;
  g_nPopThreads = 0;
  g_bStart = false;
  g_nPops = g_nPushes = 0;
  ClearBuckets(pTestBuckets);
}

void TestWait() {
  while (g_nThreads < NUM_THREADS) {
    ThreadSleep(0);
  }
  g_bStart = true;
  while (g_nThreads > 0) {
    ThreadSleep(50);
  }
}

void TestEnd(i32 *pTestBuckets, bool bExpectEmpty = true) {
  ValidateBuckets(pTestBuckets);

  if (g_nPops != g_nPushes) {
    Msg("FAIL: Not all items popped\n");
    return;
  }

  if (g_pTestOps->Validate()) {
    if (!bExpectEmpty || g_pTestOps->IsEmpty()) {
      Msg("pass\n");
    } else {
      Msg("FAIL: !IsEmpty()\n");
    }
  } else {
    Msg("FAIL: !Validate()\n");
  }
}

//--------------------------------------------------
//
//	Shared Tests for CTSQueue and CTSList
//
//--------------------------------------------------
void PushPopTest(i32 *pTestBuckets) {
  Msg("%s test: single thread push/pop, in order... ", g_pListType);
  ClearBuckets(pTestBuckets);
  g_nTested = 0;
  i32 value;
  while (g_nTested < NUM_TEST) {
    value = g_nTested++;
    g_pTestOps->Push(value);
    IncBucket(pTestBuckets, value);
  }

  g_pTestOps->Validate();

  while (g_pTestOps->Pop(&value)) {
    DecBucket(pTestBuckets, value);
  }
  TestEnd(pTestBuckets);
}

void PushPopInterleavedTestGuts(i32 *pTestBuckets) {
  i32 value;
  for (;;) {
    bool bPush = (rand() % 2 == 0);
    if (bPush && (value = g_nTested++) < NUM_TEST) {
      g_pTestOps->Push(value);
      IncBucket(pTestBuckets, value);
    } else if (g_pTestOps->Pop(&value)) {
      DecBucket(pTestBuckets, value);
    } else {
      if (g_nTested >= NUM_TEST) {
        break;
      }
    }
  }
}

void PushPopInterleavedTest(i32 *pTestBuckets) {
  Msg("%s test: single thread push/pop, interleaved... ", g_pListType);
  srand(Plat_MSTime());
  g_nTested = 0;
  ClearBuckets(pTestBuckets);
  PushPopInterleavedTestGuts(pTestBuckets);
  TestEnd(pTestBuckets);
}

u32 PushPopInterleavedTestThreadFunc(void *) {
  ThreadSetDebugName("PushPopThread");
  ++g_nThreads;
  while (!g_bStart) {
    ThreadSleep(0);
  }
  PushPopInterleavedTestGuts(g_pTestBuckets);
  --g_nThreads;
  return 0;
}

void STPushMTPop(i32 *pTestBuckets, bool bDistribute) {
  Msg("%s test: single thread push, multithread pop, %s", g_pListType,
      bDistribute ? "distributed..." : "no affinity...");
  TestStart(pTestBuckets);
  CreateSimpleThread(&PushThreadFunc, nullptr);
  for (i32 i = 0; i < NUM_THREADS - 1; i++) {
    ThreadHandle_t hThread = CreateSimpleThread(&PopThreadFunc, nullptr);
    if (bDistribute) {
      i32 mask = 1 << (i % NUM_PROCESSORS);
      ThreadSetAffinity(hThread, mask);
    }
  }

  TestWait();
  TestEnd(pTestBuckets);
}

void MTPushSTPop(i32 *pTestBuckets, bool bDistribute) {
  Msg("%s test: multithread push, single thread pop, %s", g_pListType,
      bDistribute ? "distributed..." : "no affinity...");
  TestStart(pTestBuckets);
  CreateSimpleThread(&PopThreadFunc, nullptr);
  for (i32 i = 0; i < NUM_THREADS - 1; i++) {
    ThreadHandle_t hThread = CreateSimpleThread(&PushThreadFunc, nullptr);
    if (bDistribute) {
      i32 mask = 1 << (i % NUM_PROCESSORS);
      ThreadSetAffinity(hThread, mask);
    }
  }

  TestWait();
  TestEnd(pTestBuckets);
}

void MTPushMTPop(i32 *pTestBuckets, bool bDistribute) {
  Msg("%s test: multithread push, multithread pop, %s", g_pListType,
      bDistribute ? "distributed..." : "no affinity...");
  TestStart(pTestBuckets);
  i32 ct = 0;
  for (i32 i = 0; i < NUM_THREADS / 2; i++) {
    ThreadHandle_t hThread = CreateSimpleThread(&PopThreadFunc, nullptr);
    if (bDistribute) {
      i32 mask = 1 << (ct++ % NUM_PROCESSORS);
      ThreadSetAffinity(hThread, mask);
    }
  }
  for (i32 i = 0; i < NUM_THREADS / 2; i++) {
    ThreadHandle_t hThread = CreateSimpleThread(&PushThreadFunc, nullptr);
    if (bDistribute) {
      i32 mask = 1 << (ct++ % NUM_PROCESSORS);
      ThreadSetAffinity(hThread, mask);
    }
  }

  TestWait();
  TestEnd(pTestBuckets);
}

void MTPushPopPopInterleaved(i32 *pTestBuckets, bool bDistribute) {
  Msg("%s test: multithread interleaved push/pop, %s", g_pListType,
      bDistribute ? "distributed..." : "no affinity...");
  srand(Plat_MSTime());
  TestStart(pTestBuckets);
  for (i32 i = 0; i < NUM_THREADS; i++) {
    ThreadHandle_t hThread =
        CreateSimpleThread(&PushPopInterleavedTestThreadFunc, nullptr);
    if (bDistribute) {
      i32 mask = 1 << (i % NUM_PROCESSORS);
      ThreadSetAffinity(hThread, mask);
    }
  }
  TestWait();
  TestEnd(pTestBuckets);
}

void MTPushSeqPop(i32 *pTestBuckets, bool bDistribute) {
  Msg("%s test: multithread push, sequential pop, %s", g_pListType,
      bDistribute ? "distributed..." : "no affinity...");
  TestStart(pTestBuckets);
  for (i32 i = 0; i < NUM_THREADS; i++) {
    ThreadHandle_t hThread = CreateSimpleThread(&PushThreadFunc, nullptr);
    if (bDistribute) {
      i32 mask = 1 << (i % NUM_PROCESSORS);
      ThreadSetAffinity(hThread, mask);
    }
  }

  TestWait();
  i32 ignored;
  g_pTestOps->Validate();
  while (g_pTestOps->Pop(&ignored)) {
  }
  TestEnd(pTestBuckets);
}

void SeqPushMTPop(i32 *pTestBuckets, bool bDistribute) {
  Msg("%s test: sequential push, multithread pop, %s", g_pListType,
      bDistribute ? "distributed..." : "no affinity...");
  TestStart(pTestBuckets);
  while (g_nTested++ < NUM_TEST) {
    g_pTestOps->Push(g_nTested);
  }
  for (i32 i = 0; i < NUM_THREADS; i++) {
    ThreadHandle_t hThread = CreateSimpleThread(&PopThreadFunc, nullptr);
    if (bDistribute) {
      i32 mask = 1 << (i % NUM_PROCESSORS);
      ThreadSetAffinity(hThread, mask);
    }
  }

  TestWait();
  TestEnd(pTestBuckets);
}

}  // namespace TSListTests
void RunSharedTests(i32 nTests) {
  using namespace TSListTests;

  const CPUInformation &pi = GetCPUInformation();
  NUM_PROCESSORS = pi.m_nLogicalProcessors;
  MAX_THREADS = NUM_PROCESSORS * 2;
  i32 *pTestBuckets = g_pTestBuckets = new i32[NUM_TEST];  //-V799
  while (nTests--) {
    for (NUM_THREADS = 2; NUM_THREADS <= MAX_THREADS; NUM_THREADS *= 2) {
      Msg("\nTesting %d threads:\n", NUM_THREADS);
      PushPopTest(pTestBuckets);
      PushPopInterleavedTest(pTestBuckets);
      SeqPushMTPop(pTestBuckets, false);
      STPushMTPop(pTestBuckets, false);
      MTPushSeqPop(pTestBuckets, false);
      MTPushSTPop(pTestBuckets, false);
      MTPushMTPop(pTestBuckets, false);
      MTPushPopPopInterleaved(pTestBuckets, false);
      if (NUM_PROCESSORS > 1) {
        SeqPushMTPop(pTestBuckets, true);
        STPushMTPop(pTestBuckets, true);
        MTPushSeqPop(pTestBuckets, true);
        MTPushSTPop(pTestBuckets, true);
        MTPushMTPop(pTestBuckets, true);
        MTPushPopPopInterleaved(pTestBuckets, true);
      }
    }
  }
  delete[] pTestBuckets;
}

bool RunTSListTests(i32 nListSize, i32 nTests) {
  using namespace TSListTests;
  NUM_TEST = nListSize;

  TSLHead_t foo;
#ifdef USE_NATIVE_SLIST
  i32 maxSize = (1 << (sizeof(decltype(foo.HeaderX64.Depth)))) - 1;
#else
  i32 maxSize = (1 << (sizeof(foo.value.Depth) * 8)) - 1;
#endif
  if (NUM_TEST > maxSize) {
    Msg("TSList cannot hold more that %d nodes\n", maxSize);
    return false;
  }

  g_pTestOps = &g_ListOps;
  g_pListType = "CTSList";

  RunSharedTests(nTests);

  Msg("Tests done, purging test memory...");
  g_TestList.Purge();
  Msg("done\n");
  return true;
}

bool RunTSQueueTests(i32 nListSize, i32 nTests) {
  using namespace TSListTests;
  NUM_TEST = nListSize;

  g_pTestOps = &g_QueueOps;
  g_pListType = "CTSQueue";

  RunSharedTests(nTests);

  Msg("Tests done, purging test memory...");
  g_TestQueue.Purge();
  Msg("done\n");
  return true;
}
