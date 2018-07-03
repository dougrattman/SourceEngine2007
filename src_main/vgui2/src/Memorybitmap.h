// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_VGUI_MEMORYBITMAP_H_
#define SOURCE_VGUI_MEMORYBITMAP_H_

#ifdef _WIN32
#pragma once
#endif

#include "Color.h"
#include "base/include/base_types.h"
#include "vgui/IImage.h"
#include "vgui/VGUI.h"

namespace vgui {
using HTexture = unsigned long;

// Purpose: Holds a single image created from a chunk of memory, internal to
// vgui only
class MemoryBitmap : public IImage {
 public:
  MemoryBitmap(u8 *texture, int wide, int tall);
  ~MemoryBitmap();

  // IImage implementation
  void Paint() override;
  void GetSize(int &wide, int &tall) override;
  void GetContentSize(int &wide, int &tall) override;
  void SetPos(int x, int y) override;
  void SetSize(int x, int y) override;
  void SetColor(Color col) override;

  // methods
  void ForceUpload(u8 *texture, int wide,
                   int tall);  // ensures the bitmap has been uploaded
  HTexture GetID();            // returns the texture id
  const char *GetName();
  bool IsValid() const { return _valid; }

 private:
  HTexture _id;
  bool _uploaded;
  bool _valid;
  u8 *_texture;
  int _pos[2];
  Color _color;
  int _w, _h;  // size of the texture
};
}  // namespace vgui

#endif  // SOURCE_VGUI_MEMORYBITMAP_H_
