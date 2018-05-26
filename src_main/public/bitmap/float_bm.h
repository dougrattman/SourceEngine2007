// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef FLOAT_BM_H
#define FLOAT_BM_H

#include "base/include/base_types.h"
#include "mathlib/mathlib.h"
#include "tier0/include/dbg.h"
#include "tier0/include/platform.h"

struct PixRGBAF {
  f32 Red;
  f32 Green;
  f32 Blue;
  f32 Alpha;
};

struct PixRGBA8 {
  u8 Red;
  u8 Green;
  u8 Blue;
  u8 Alpha;
};

constexpr inline PixRGBAF PixRGBA8_to_F(PixRGBA8 const &x) {
  return PixRGBAF{x.Red / 255.0f, x.Green / 255.0f, x.Blue / 255.0f,
                  x.Alpha / 255.0f};
}

constexpr inline PixRGBA8 PixRGBAF_to_8(PixRGBAF const &f) {
  return PixRGBA8{std::max((u8)0, (u8)std::min(255.f, 255 * f.Red)),
                  std::max((u8)0, (u8)std::min(255.f, 255 * f.Green)),
                  std::max((u8)0, (u8)std::min(255.f, 255 * f.Blue)),
                  std::max((u8)0, (u8)std::min(255.f, 255 * f.Alpha))};
}

#define SPFLAGS_MAXGRADIENT 1

// Bit flag options for ComputeSelfShadowedBumpmapFromHeightInAlphaChannel:
// Generate ambient occlusion only
#define SSBUMP_OPTION_NONDIRECTIONAL 1
// Scale so that a flat unshadowed value is 0.5, and bake rgb luminance in.
#define SSBUMP_MOD2X_DETAIL_TEXTURE 2

class FloatBitMap_t {
 public:
  int Width, Height;  // bitmap dimensions
  f32 *RGBAData;      // actual data

  FloatBitMap_t() : Width{0}, Height{0}, RGBAData{0} {}
  FloatBitMap_t(int width, int height);  // make one and allocate space
  FloatBitMap_t(char const *filename);   // read one from a file (tga or pfm)
  FloatBitMap_t(FloatBitMap_t const *orig);

  ~FloatBitMap_t();

  // Quantize one to 8 bits
  bool WriteTGAFile(char const *filename) const;

  // Load from floating point pixmap (.pfm) file
  bool LoadFromPFM(char const *filename);
  // Save to floating point pixmap (.pfm) file
  bool WritePFM(char const *filename);

  void InitializeWithRandomPixelsFromAnotherFloatBM(FloatBitMap_t const &other);

  inline f32 &Pixel(int x, int y, int comp) const {
    Assert((x >= 0) && (x < Width));
    Assert((y >= 0) && (y < Height));
    return RGBAData[4 * (x + Width * y) + comp];
  }

  inline f32 &PixelWrapped(int x, int y, int comp) const {
    // like Pixel except wraps around to other side
    if (x < 0)
      x += Width;
    else if (x >= Width)
      x -= Width;

    if (y < 0)
      y += Height;
    else if (y >= Height)
      y -= Height;

    return RGBAData[4 * (x + Width * y) + comp];
  }

  inline f32 &PixelClamped(int x, int y, int comp) const {
    // Like Pixel except wraps around to other side
    x = std::clamp(x, 0, Width - 1);
    y = std::clamp(y, 0, Height - 1);
    return RGBAData[4 * (x + Width * y) + comp];
  }

  inline f32 &Alpha(int x, int y) const {
    Assert((x >= 0) && (x < Width));
    Assert((y >= 0) && (y < Height));
    return RGBAData[3 + 4 * (x + Width * y)];
  }

  // Look up a pixel value with bilinear interpolation
  f32 InterpolatedPixel(f32 x, f32 y, int comp) const;

  inline PixRGBAF PixelRGBAF(int x, int y) const {
    Assert((x >= 0) && (x < Width));
    Assert((y >= 0) && (y < Height));

    int RGBoffset = 4 * (x + Width * y);
    PixRGBAF RetPix;
    RetPix.Red = RGBAData[RGBoffset + 0];
    RetPix.Green = RGBAData[RGBoffset + 1];
    RetPix.Blue = RGBAData[RGBoffset + 2];
    RetPix.Alpha = RGBAData[RGBoffset + 3];

    return RetPix;
  }

  inline void WritePixelRGBAF(int x, int y, const PixRGBAF &value) const {
    Assert((x >= 0) && (x < Width));
    Assert((y >= 0) && (y < Height));

    int RGBoffset = 4 * (x + Width * y);
    RGBAData[RGBoffset + 0] = value.Red;
    RGBAData[RGBoffset + 1] = value.Green;
    RGBAData[RGBoffset + 2] = value.Blue;
    RGBAData[RGBoffset + 3] = value.Alpha;
  }

  inline void WritePixel(int x, int y, int comp, f32 value) {
    Assert((x >= 0) && (x < Width));
    Assert((y >= 0) && (y < Height));
    RGBAData[4 * (x + Width * y) + comp] = value;
  }

  // Paste, performing boundary matching. Alpha channel can be used to make
  // brush shape irregular
  void SmartPaste(FloatBitMap_t const &brush, int xofs, int yofs, u32 flags);

  // Force to be tileable using poisson formula
  void MakeTileable();

  void ReSize(int NewXSize, int NewYSize);

  // Find the bounds of the area that has non-zero alpha.
  void GetAlphaBounds(int &minx, int &miny, int &maxx, int &maxy);

  // Solve the poisson equation for an image. The alpha channel of the image
  // controls which pixels are "modifiable", and can be used to set boundary
  // conditions. Alpha=0 means the pixel is locked.  deltas are in the order
  // [(x,y)-(x,y-1),(x,y)-(x-1,y),(x,y)-(x+1,y),(x,y)-(x,y+1)
  void Poisson(FloatBitMap_t *deltas[4], int n_iters,
               u32 flags  // SPF_xxx
  );

  FloatBitMap_t *QuarterSize() const;        // get a new one downsampled
  FloatBitMap_t *QuarterSizeBlocky() const;  // get a new one downsampled

  FloatBitMap_t *QuarterSizeWithGaussian(
      void) const;  // downsample 2x using a gaussian

  void RaiseToPower(f32 pow);
  void ScaleGradients();
  void Logize();    // pix=log(1+pix)
  void UnLogize();  // pix=exp(pix)-1

  // Compress to 8 bits converts the hdr texture to an 8 bit texture, encoding
  // a scale factor in the alpha channel. upon return, the original pixel can
  // be (approximately) recovered by the formula rgb*alpha*overbright. this
  // function performs special numerical optimization on the texture to
  // minimize the error when using bilinear filtering to read the texture.
  void CompressTo8Bits(f32 overbright);
  // Decompress a bitmap converted by CompressTo8Bits
  void Uncompress(f32 overbright);

  Vector AverageColor() const;  // average rgb value of all pixels
  f32 BrightestColor() const;   // highest vector magnitude

  void Clear(f32 r, f32 g, f32 b,
             f32 alpha);  // set all pixels to speicifed values (0..1 nominal)

  void ScaleRGB(f32 scale_factor);  // for all pixels, r,g,b*=scale_factor

  // Given a bitmap with height stored in the alpha channel, generate vector
  // positions and normals
  void ComputeVertexPositionsAndNormals(f32 flHeightScale, Vector **ppPosOut,
                                        Vector **ppNormalOut) const;

  // Generate a normal map with height stored in alpha.  uses hl2 tangent
  // basis to support baked self shadowing.  the bump scale maps the height of
  // a pixel relative to the edges of the pixel. This function may take a
  // while - many millions of rays may be traced.  applications using this
  // method need to link w/ raytrace.lib
  FloatBitMap_t *ComputeSelfShadowedBumpmapFromHeightInAlphaChannel(
      f32 bump_scale, int nrays_to_trace_per_pixel = 100,
      u32 nOptionFlags = 0  // SSBUMP_OPTION_XXX
      ) const;

  // Generate a conventional normal map from a source with height stored in
  // alpha.
  FloatBitMap_t *ComputeBumpmapFromHeightInAlphaChannel(f32 bump_scale) const;

  // Bilateral (edge preserving) smoothing filter. edge_threshold_value
  // defines the difference in values over which filtering will not occur.
  // Each channel is filtered independently. large radii will run slow, since
  // the bilateral filter is neither separable, nor is it a convolution that
  // can be done via fft.
  void TileableBilateralFilter(int radius_in_pixels, f32 edge_threshold_value);

  void AllocateRGB(int w, int h) {
    delete[] RGBAData;
    RGBAData = new f32[w * h * 4];
    Width = w;
    Height = h;
  }
};

// FloatCubeMap_t holds the floating point bitmaps for 6 faces of a cube map.
class FloatCubeMap_t {
 public:
  FloatBitMap_t face_maps[6];

  FloatCubeMap_t(int xfsize, int yfsize) {
    // Make an empty one with face dimensions xfsize x yfsize
    for (int f = 0; f < 6; f++) face_maps[f].AllocateRGB(xfsize, yfsize);
  }

  // Load basenamebk,pfm, basenamedn.pfm, basenameft.pfm, ...
  FloatCubeMap_t(char const *basename);

  // Save basenamebk,pfm, basenamedn.pfm, basenameft.pfm, ...
  void WritePFMs(char const *basename);

  Vector AverageColor() const {
    Vector ret{0, 0, 0};
    int nfaces = 0;

    for (int f = 0; f < 6; f++) {
      if (face_maps[f].RGBAData) {
        nfaces++;
        ret += face_maps[f].AverageColor();
      }
    }

    if (nfaces) ret *= (1.0f / nfaces);

    return ret;
  }

  f32 BrightestColor() const {
    f32 ret = 0.0f;
    int nfaces = 0;
    for (int f = 0; f < 6; f++) {
      if (face_maps[f].RGBAData) {
        nfaces++;
        ret = std::max(ret, face_maps[f].BrightestColor());
      }
    }
    return ret;
  }

  // Resample a cubemap to one of possibly a lower resolution, using a given
  // phong exponent. dot-product weighting will be used for the filtering
  // operation.
  void Resample(FloatCubeMap_t &dest, f32 flPhongExponent);

  // Returns the normalized direciton vector through a given pixel of a given
  // face
  Vector PixelDirection(int face, int x, int y);

  // Returns the direction vector throught the center of a cubemap face
  Vector FaceNormal(int nFaceNumber);
};

static constexpr inline f32 FLerp(f32 f1, f32 f2, f32 t) {
  return f1 + (f2 - f1) * t;
}

// Image Pyramid class.
#define MAX_IMAGE_PYRAMID_LEVELS 16  // up to 64kx64k

enum ImagePyramidMode_t {
  PYRAMID_MODE_GAUSSIAN,
};

class FloatImagePyramid_t {
 public:
  int m_nLevels;
  FloatBitMap_t *m_pLevels[MAX_IMAGE_PYRAMID_LEVELS];  // level 0 is highest res

  FloatImagePyramid_t() : m_nLevels{0} {
    memset(m_pLevels, 0, sizeof(m_pLevels));
  }

  // Build one. clones data from src for level 0.
  FloatImagePyramid_t(FloatBitMap_t const &src, ImagePyramidMode_t mode);

  ~FloatImagePyramid_t();

  // read or write a Pixel from a given level. All coordinates are specified
  // in the same domain as the base level.
  f32 &Pixel(int x, int y, int component, int level) const;

  FloatBitMap_t *Level(int lvl) const {
    Assert(lvl < m_nLevels);
    Assert(lvl < std::size(m_pLevels));

    return m_pLevels[lvl];
  }

  // Rebuild all levels above the specified level
  void ReconstructLowerResolutionLevels(int starting_level);

  // Outputs name_00.tga, name_01.tga,...
  void WriteTGAs(char const *basename) const;
};

#endif
