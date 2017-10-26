// Copyright © 1996-2017, Valve Corporation, All rights reserved.
//
// Purpose: Baked Bitmap fonts.

#ifndef SOURCE_VGUI2_VGUI_SURFACE_LIB_BITMAPFONT_H_
#define SOURCE_VGUI2_VGUI_SURFACE_LIB_BITMAPFONT_H_

#include "BitmapFontFile.h"
#include "Win32Font.h"

class ITexture;

// Purpose: encapsulates a windows font
class CBitmapFont : public CWin32Font {
 public:
  CBitmapFont();
  ~CBitmapFont();

  // creates the font.  returns false if the compiled font does not exist.
  virtual bool Create(const char *windowsFontName, float scalex, float scaley,
                      int flags);

  // returns true if the font is equivalent to that specified
  virtual bool IsEqualTo(const char *windowsFontName, float scalex,
                         float scaley, int flags);

  // gets the abc widths for a character
  virtual void GetCharABCWidths(int ch, int &a, int &b, int &c);

  // gets the texture coords in the compiled texture page
  void GetCharCoords(int ch, float *left, float *top, float *right,
                     float *bottom);

  // sets the scale of the font.
  void SetScale(float sx, float sy);

  // gets the compiled texture page
  ITexture *GetTexturePage();

 private:
  int m_bitmapFontHandle;
  float m_scalex;
  float m_scaley;
};

#endif  // SOURCE_VGUI2_VGUI_SURFACE_LIB_BITMAPFONT_H_
