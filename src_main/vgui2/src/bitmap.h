// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_VGUI_BITMAP_H_
#define SOURCE_VGUI_BITMAP_H_

#ifdef _WIN32
#pragma once
#endif

#include "Color.h"
#include "vgui/IImage.h"

namespace vgui {
using HTexture = unsigned long;

// Purpose: Holds a single image, internal to vgui only
class Bitmap : public IImage {
 public:
  Bitmap(const char *filename, bool hardwareFiltered);
  ~Bitmap();

  // IImage implementation
  void Paint() override;
  void GetSize(int &wide, int &tall) override;
  void GetContentSize(int &wide, int &tall) override;
  void SetSize(int x, int y) override;
  void SetPos(int x, int y) override;
  void SetColor(Color col) override;

  // methods
  void ForceUpload();  // ensures the bitmap has been uploaded
  HTexture GetID();    // returns the texture id
  const char *GetName();
  bool IsValid() const { return _valid; }

 private:
  HTexture _id;
  bool _uploaded;
  bool _valid;
  char *_filename;
  int _pos[2];
  Color _color;
  bool _filtered;
  int _wide, _tall;
  bool _bProcedural;
};
}  // namespace vgui

#endif  // SOURCE_VGUI_BITMAP_H_
