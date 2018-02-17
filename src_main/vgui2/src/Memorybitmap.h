// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_VGUI_MEMORYBITMAP_H_
#define SOURCE_VGUI_MEMORYBITMAP_H_

#include "Color.h"
#include "vgui/IImage.h"
#include "vgui/VGUI.h"

namespace vgui {
typedef unsigned long HTexture;

// Purpose: Holds a single image created from a chunk of memory, internal to
// vgui only
class MemoryBitmap : public IImage {
 public:
  MemoryBitmap(unsigned char *texture, int wide, int tall);
  ~MemoryBitmap();

  // IImage implementation
  virtual void Paint();
  virtual void GetSize(int &wide, int &tall);
  virtual void GetContentSize(int &wide, int &tall);
  virtual void SetPos(int x, int y);
  virtual void SetSize(int x, int y);
  virtual void SetColor(Color col);

  // methods
  void ForceUpload(unsigned char *texture, int wide,
                   int tall);  // ensures the bitmap has been uploaded
  HTexture GetID();            // returns the texture id
  const char *GetName();
  bool IsValid() { return _valid; }

 private:
  HTexture _id;
  bool _uploaded;
  bool _valid;
  unsigned char *_texture;
  int _pos[2];
  Color _color;
  int _w, _h;  // size of the texture
};
}  // namespace vgui

#endif  // SOURCE_VGUI_MEMORYBITMAP_H_
