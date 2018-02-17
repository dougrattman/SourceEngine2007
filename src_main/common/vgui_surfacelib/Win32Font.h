// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_VGUI2_VGUI_SURFACE_LIB_WIN32FONT_H_
#define SOURCE_VGUI2_VGUI_SURFACE_LIB_WIN32FONT_H_

#define OEMRESOURCE
#include "base/include/windows/windows_light.h"

#ifdef GetCharABCWidths
#undef GetCharABCWidths
#endif

#include "UtlRBTree.h"
#include "tier1/UtlSymbol.h"

// Structure passed to CWin32Font::GetCharsRGBA
struct newChar_t {
  wchar_t wch;   // A new character to generate texture data for
  int fontWide;  // Texel width of the character
  int fontTall;  // Texel height of the character
  int offset;    // Offset into the buffer given to GetCharsRGBA
};

// Purpose: encapsulates a windows font
class CWin32Font {
 public:
  CWin32Font();
  ~CWin32Font();

  // creates the font from windows.  returns false if font does not exist in the
  // OS.
  virtual bool Create(const char *windowsFontName, int tall, int weight,
                      int blur, int scanlines, int flags);

  // writes the char into the specified 32bpp texture
  virtual void GetCharRGBA(wchar_t ch, int rgbaWide, int rgbaTall,
                           unsigned char *rgba);

  // returns true if the font is equivalent to that specified
  virtual bool IsEqualTo(const char *windowsFontName, int tall, int weight,
                         int blur, int scanlines, int flags);

  // returns true only if this font is valid for use
  virtual bool IsValid();

  // gets the abc widths for a character
  virtual void GetCharABCWidths(int ch, int &a, int &b, int &c);

  // set the font to be the one to currently draw with in the gdi
  virtual void SetAsActiveFont(HDC hdc);

  // returns the height of the font, in pixels
  virtual int GetHeight();

  // returns the ascent of the font, in pixels (ascent=units above the base
  // line)
  virtual int GetAscent();

  // returns the maximum width of a character, in pixels
  virtual int GetMaxCharWidth();

  // returns the flags used to make this font
  virtual int GetFlags();

  // returns true if this font is underlined
  bool GetUnderlined() { return m_bUnderlined; }

  // gets the name of this font
  const char *GetName() { return m_szName.String(); }

 private:
  HFONT m_hFont;
  HDC m_hDC;
  HBITMAP m_hDIB;

  // pointer to buffer for use when generated bitmap versions of a texture
  unsigned char *m_pBuf;

 protected:
  CUtlSymbol m_szName;

  short m_iTall;
  unsigned short m_iWeight;
  unsigned short m_iFlags;
  unsigned short m_iScanLines;
  unsigned short m_iBlur;
  unsigned short m_rgiBitmapSize[2];
  bool m_bUnderlined;

  unsigned int m_iHeight : 8;
  unsigned int m_iMaxCharWidth : 8;
  unsigned int m_iAscent : 8;
  unsigned int m_iDropShadowOffset : 1;
  unsigned int m_iOutlineSize : 1;
  unsigned int m_bAntiAliased : 1;
  unsigned int m_bRotary : 1;
  unsigned int m_bAdditive : 1;  // 29

 private:
  // abc widths
  struct abc_t {
    short b;
    char a;
    char c;
  };

  // On PC we cache char widths on demand when actually requested to minimize
  // our use of the kernels paged pool (GDI may cache information about glyphs
  // we have requested and take up lots of paged pool)
  struct abc_cache_t {
    wchar_t wch;
    abc_t abc;
  };
  CUtlRBTree<abc_cache_t, unsigned short> m_ExtendedABCWidthsCache;
  static bool ExtendedABCWidthsCacheLessFunc(const abc_cache_t &lhs,
                                             const abc_cache_t &rhs);
};

#endif  // SOURCE_VGUI2_VGUI_SURFACE_LIB_WIN32FONT_H_
