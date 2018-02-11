// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#include "tier1/diff.h"

#include "mathlib/mathlib.h"
#include "tier0/include/dbg.h"
#include "tier0/include/platform.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/include/memdbgon.h"

// format of diff output:
// 0NN (N=1..127)  copy next N literaly
//
// 1NN (N=1..127) ofs (-128..127) copy next N bytes from original, changin
// offset by N bytes from last copy end 100 N ofs(-32768..32767) copy next N,
// with larger delta offset 00 NNNN(1..65535) ofs(-32768..32767) big copy from
// old 80 00 NN NN NN big raw copy
//
// available codes (could be used for additonal compression ops)
// long offset form whose offset could have fit in short offset

// note - this algorithm uses storage equal to 8* the old buffer size. 64k=.5mb

#define MIN_MATCH_LEN 8
#define ACCEPTABLE_MATCH_LEN 4096

struct BlockPtr {
  BlockPtr *Next;
  uint8_t const *dataptr;
};

template <class T, class V>
static inline void AddToHead(T *&head, V *node) {
  node->Next = head;
  head = node;
}

void Fail(char const *msg) { Assert(0); }

void ApplyDiffs(uint8_t const *OldBlock, uint8_t const *DiffList, int OldSize,
                int DiffListSize, int &ResultListSize, uint8_t *Output,
                uint32_t OutSize) {
  uint8_t const *copy_src = OldBlock;
  uint8_t const *end_of_diff_list = DiffList + DiffListSize;
  uint8_t const *obuf = Output;
  while (DiffList < end_of_diff_list) {
    uint8_t op = *(DiffList++);
    if (op == 0) {
      uint16_t copy_sz = DiffList[0] + 256 * DiffList[1];
      int copy_ofs = DiffList[2] + DiffList[3] * 256;
      if (copy_ofs > 32767) copy_ofs |= 0xffff0000;

      memcpy(Output, copy_src + copy_ofs, copy_sz);
      Output += copy_sz;
      copy_src = copy_src + copy_ofs + copy_sz;
      DiffList += 4;
    } else {
      if (op & 0x80) {
        int copy_sz = op & 0x7f;
        int copy_ofs;
        if (copy_sz == 0) {
          copy_sz = DiffList[0];
          if (copy_sz == 0) {
            // big raw copy
            copy_sz = DiffList[1] + 256 * DiffList[2] + 65536 * DiffList[3];
            memcpy(Output, DiffList + 4, copy_sz);

            DiffList += copy_sz + 4;
            Output += copy_sz;
          } else {
            copy_ofs = DiffList[1] + (DiffList[2] * 256);
            if (copy_ofs > 32767) copy_ofs |= 0xffff0000;

            memcpy(Output, copy_src + copy_ofs, copy_sz);
            Output += copy_sz;
            copy_src = copy_src + copy_ofs + copy_sz;
            DiffList += 3;
          }
        } else {
          copy_ofs = DiffList[0];
          if (copy_ofs > 127) copy_ofs |= 0xffffff80;

          memcpy(Output, copy_src + copy_ofs, copy_sz);
          Output += copy_sz;
          copy_src = copy_src + copy_ofs + copy_sz;
          DiffList++;
        }
      } else {
        memcpy(Output, DiffList, op & 127);
        Output += op & 127;
        DiffList += (op & 127);
      }
    }
  }
  ResultListSize = Output - obuf;
}

static void CopyPending(int len, uint8_t const *rawbytes, uint8_t *&outbuf,
                        uint8_t const *limit) {
  if (len < 128) {
    if (limit - outbuf < len + 1) Fail("diff buffer overrun");
    *(outbuf++) = len;
    memcpy(outbuf, rawbytes, len);
    outbuf += len;
  } else {
    if (limit - outbuf < len + 5) Fail("diff buffer overrun");
    *(outbuf++) = 0x80;
    *(outbuf++) = 0x00;
    *(outbuf++) = (len & 255);
    *(outbuf++) = ((len >> 8) & 255);
    *(outbuf++) = ((len >> 16) & 255);
    memcpy(outbuf, rawbytes, len);
    outbuf += len;
  }
}

static uint32_t hasher(uint8_t const *mdata) {
  // attempt to scramble the bits of h1 and h2 together
  uint32_t ret = 0;
  for (int i = 0; i < MIN_MATCH_LEN; i++) {
    ret = ret << 4;
    ret += (*mdata++);
  }
  return ret;
}

int FindDiffsForLargeFiles(uint8_t const *NewBlock, uint8_t const *OldBlock,
                           int NewSize, int OldSize, int &DiffListSize,
                           uint8_t *Output, uint32_t OutSize, int hashsize) {
  int ret = 0;
  if (OldSize != NewSize) ret = 1;
  // first, build the hash table
  BlockPtr **HashedMatches = new BlockPtr *[hashsize];
  memset(HashedMatches, 0, sizeof(HashedMatches[0]) * hashsize);
  BlockPtr *Blocks = 0;
  if (OldSize) Blocks = new BlockPtr[OldSize];
  BlockPtr *FreeList = Blocks;
  // now, build the hash table
  uint8_t const *walk = OldBlock;
  if (OldBlock && OldSize)
    while (walk < OldBlock + OldSize - MIN_MATCH_LEN) {
      uint32_t hash1 = hasher(walk);
      hash1 &= (hashsize - 1);
      BlockPtr *newnode = FreeList;
      FreeList++;
      newnode->dataptr = walk;
      AddToHead(HashedMatches[hash1], newnode);
      walk++;
    }
  else
    ret = 1;
  // now, we have the hash table which may be used to search. begin the output
  // step
  int pending_raw_len = 0;
  walk = NewBlock;
  uint8_t *outbuf = Output;
  uint8_t const *lastmatchend = OldBlock;
  while (walk < NewBlock + NewSize) {
    int longest = 0;
    BlockPtr *longest_block = 0;
    if (walk < NewBlock + NewSize - MIN_MATCH_LEN) {
      // check for a match
      uint32_t hash1 = hasher(walk);
      hash1 &= (hashsize - 1);
      // now, find the longest match in the hash table. If we find one
      // >MIN_MATCH_LEN, take it
      for (BlockPtr *b = HashedMatches[hash1]; b; b = b->Next) {
        // find the match length
        int match_of = b->dataptr - lastmatchend;
        if ((match_of > -32768) && (match_of < 32767)) {
          int max_mlength = min(65535, OldBlock + OldSize - b->dataptr);
          max_mlength = min(max_mlength, NewBlock + NewSize - walk);
          int i;
          for (i = 0; i < max_mlength; i++)
            if (walk[i] != b->dataptr[i]) break;
          if ((i > MIN_MATCH_LEN) && (i > longest)) {
            longest = i;
            longest_block = b;
            if (longest > ACCEPTABLE_MATCH_LEN) break;
          }
        }
      }
    }
    // now, we have a match maybe
    if (longest_block) {
      if (pending_raw_len)  // must output
      {
        ret = 1;
        CopyPending(pending_raw_len, walk - pending_raw_len, outbuf,
                    Output + OutSize);
        pending_raw_len = 0;
      }
      // now, output copy block
      int match_of = longest_block->dataptr - lastmatchend;
      int nremaining = OutSize - (outbuf - Output);

      if (match_of) ret = 1;
      if (longest > 127) {
        // use really long encoding
        if (nremaining < 5) Fail("diff buff needs increase");
        *(outbuf++) = 00;
        *(outbuf++) = (longest & 255);
        *(outbuf++) = ((longest >> 8) & 255);
        *(outbuf++) = (match_of & 255);
        *(outbuf++) = ((match_of >> 8) & 255);

      } else {
        if ((match_of >= -128) && (match_of < 128)) {
          if (nremaining < 2) Fail("diff buff needs increase");
          *(outbuf++) = 128 + longest;
          *(outbuf++) = (match_of & 255);
        } else {
          // use long encoding
          if (nremaining < 4) Fail("diff buff needs increase");
          *(outbuf++) = 0x80;
          *(outbuf++) = longest;
          *(outbuf++) = (match_of & 255);
          *(outbuf++) = ((match_of >> 8) & 255);
        }
      }
      lastmatchend = longest_block->dataptr + longest;
      walk += longest;
    } else {
      walk++;
      pending_raw_len++;
    }
  }
  // now, flush pending raw copy
  if (pending_raw_len)  // must output
  {
    ret = 1;
    CopyPending(pending_raw_len, walk - pending_raw_len, outbuf,
                Output + OutSize);
    pending_raw_len = 0;
  }
  delete[] HashedMatches;
  delete[] Blocks;
  DiffListSize = outbuf - Output;
  return ret;
}

int FindDiffs(uint8_t const *NewBlock, uint8_t const *OldBlock, int NewSize,
              int OldSize, int &DiffListSize, uint8_t *Output,
              uint32_t OutSize) {
  int ret = 0;
  if (OldSize != NewSize) ret = 1;
  // first, build the hash table
  BlockPtr *HashedMatches[65536];
  memset(HashedMatches, 0, sizeof(HashedMatches));
  BlockPtr *Blocks = 0;
  if (OldSize) Blocks = new BlockPtr[OldSize];
  BlockPtr *FreeList = Blocks;
  // now, build the hash table
  uint8_t const *walk = OldBlock;
  if (OldBlock && OldSize)
    while (walk < OldBlock + OldSize - MIN_MATCH_LEN) {
      uint16_t hash1 =
          *((uint16_t const *)walk) + *((uint16_t const *)walk + 2);
      BlockPtr *newnode = FreeList;
      FreeList++;
      newnode->dataptr = walk;
      AddToHead(HashedMatches[hash1], newnode);
      walk++;
    }
  else
    ret = 1;
  // now, we have the hash table which may be used to search. begin the output
  // step
  int pending_raw_len = 0;
  walk = NewBlock;
  uint8_t *outbuf = Output;
  uint8_t const *lastmatchend = OldBlock;
  while (walk < NewBlock + NewSize) {
    int longest = 0;
    BlockPtr *longest_block = 0;
    if (walk < NewBlock + NewSize - MIN_MATCH_LEN) {
      // check for a match
      uint16_t hash1 =
          *((uint16_t const *)walk) + *((uint16_t const *)walk + 2);
      // now, find the longest match in the hash table. If we find one
      // >MIN_MATCH_LEN, take it
      for (BlockPtr *b = HashedMatches[hash1]; b; b = b->Next) {
        // find the match length
        int match_of = b->dataptr - lastmatchend;
        if ((match_of > -32768) && (match_of < 32767)) {
          int max_mlength = min(65535, OldBlock + OldSize - b->dataptr);
          max_mlength = min(max_mlength, NewBlock + NewSize - walk);
          int i;
          for (i = 0; i < max_mlength; i++)
            if (walk[i] != b->dataptr[i]) break;
          if ((i > MIN_MATCH_LEN) && (i > longest)) {
            longest = i;
            longest_block = b;
          }
        }
      }
    }
    // now, we have a match maybe
    if (longest_block) {
      if (pending_raw_len)  // must output
      {
        ret = 1;
        CopyPending(pending_raw_len, walk - pending_raw_len, outbuf,
                    Output + OutSize);
        pending_raw_len = 0;
      }
      // now, output copy block
      int match_of = longest_block->dataptr - lastmatchend;
      int nremaining = OutSize - (outbuf - Output);
      if (match_of) ret = 1;
      if (longest > 127) {
        // use really long encoding
        if (nremaining < 5) Fail("diff buff needs increase");
        *(outbuf++) = 00;
        *(outbuf++) = (longest & 255);
        *(outbuf++) = ((longest >> 8) & 255);
        *(outbuf++) = (match_of & 255);
        *(outbuf++) = ((match_of >> 8) & 255);
      } else {
        if ((match_of >= -128) && (match_of < 128)) {
          if (nremaining < 2) Fail("diff buff needs increase");
          *(outbuf++) = 128 + longest;
          *(outbuf++) = (match_of & 255);
        } else {
          // use long encoding
          if (nremaining < 4) Fail("diff buff needs increase");
          *(outbuf++) = 0x80;
          *(outbuf++) = longest;
          *(outbuf++) = (match_of & 255);
          *(outbuf++) = ((match_of >> 8) & 255);
        }
      }
      lastmatchend = longest_block->dataptr + longest;
      walk += longest;
    } else {
      walk++;
      pending_raw_len++;
    }
  }
  // now, flush pending raw copy
  if (pending_raw_len)  // must output
  {
    ret = 1;
    CopyPending(pending_raw_len, walk - pending_raw_len, outbuf,
                Output + OutSize);
    pending_raw_len = 0;
  }
  delete[] Blocks;
  DiffListSize = outbuf - Output;
  return ret;
}

int FindDiffsLowMemory(uint8_t const *NewBlock, uint8_t const *OldBlock,
                       int NewSize, int OldSize, int &DiffListSize,
                       uint8_t *Output, uint32_t OutSize) {
  int ret = 0;
  if (OldSize != NewSize) ret = 1;
  uint8_t const *old_data_hash[256];
  memset(old_data_hash, 0, sizeof(old_data_hash));
  int pending_raw_len = 0;
  uint8_t const *walk = NewBlock;
  uint8_t const *oldptr = OldBlock;
  uint8_t *outbuf = Output;
  uint8_t const *lastmatchend = OldBlock;
  while (walk < NewBlock + NewSize) {
    while ((oldptr - OldBlock < walk - NewBlock + 40) &&
           (oldptr - OldBlock < OldSize - MIN_MATCH_LEN)) {
      uint16_t hash1 = (oldptr[0] + oldptr[1] + oldptr[2] + oldptr[3]) &
                       (NELEMS(old_data_hash) - 1);
      old_data_hash[hash1] = oldptr;
      oldptr++;
    }
    int longest = 0;
    uint8_t const *longest_block = 0;
    if (walk < NewBlock + NewSize - MIN_MATCH_LEN) {
      // check for a match
      uint16_t hash1 =
          (walk[0] + walk[1] + walk[2] + walk[3]) & (NELEMS(old_data_hash) - 1);
      if (old_data_hash[hash1]) {
        int max_bytes_to_compare =
            min(NewBlock + NewSize - walk,
                OldBlock + OldSize - old_data_hash[hash1]);
        int nmatches;
        for (nmatches = 0; nmatches < max_bytes_to_compare; nmatches++)
          if (walk[nmatches] != old_data_hash[hash1][nmatches]) break;
        if (nmatches > MIN_MATCH_LEN) {
          longest_block = old_data_hash[hash1];
          longest = nmatches;
        }
      }
    }
    // now, we have a match maybe
    if (longest_block) {
      if (pending_raw_len)  // must output
      {
        ret = 1;
        CopyPending(pending_raw_len, walk - pending_raw_len, outbuf,
                    Output + OutSize);
        pending_raw_len = 0;
      }
      // now, output copy block
      int match_of = longest_block - lastmatchend;
      int nremaining = OutSize - (outbuf - Output);
      if (match_of) ret = 1;
      if (longest > 127) {
        // use really long encoding
        if (nremaining < 5) Fail("diff buff needs increase");
        *(outbuf++) = 00;
        *(outbuf++) = (longest & 255);
        *(outbuf++) = ((longest >> 8) & 255);
        *(outbuf++) = (match_of & 255);
        *(outbuf++) = ((match_of >> 8) & 255);
      } else {
        if ((match_of >= -128) && (match_of < 128)) {
          if (nremaining < 2) Fail("diff buff needs increase");
          *(outbuf++) = 128 + longest;
          *(outbuf++) = (match_of & 255);
        } else {
          // use long encoding
          if (nremaining < 4) Fail("diff buff needs increase");
          *(outbuf++) = 0x80;
          *(outbuf++) = longest;
          *(outbuf++) = (match_of & 255);
          *(outbuf++) = ((match_of >> 8) & 255);
        }
      }
      lastmatchend = longest_block + longest;
      walk += longest;
    } else {
      walk++;
      pending_raw_len++;
    }
  }
  // now, flush pending raw copy
  if (pending_raw_len)  // must output
  {
    ret = 1;
    CopyPending(pending_raw_len, walk - pending_raw_len, outbuf,
                Output + OutSize);
    pending_raw_len = 0;
  }
  DiffListSize = outbuf - Output;
  return ret;
}
