// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef BITMAP_BITMAP_H_
#define BITMAP_BITMAP_H_

#include "base/include/base_types.h"
#include "bitmap/imageformat.h"

// A Bitmap
struct Bitmap_t {
  Bitmap_t()
      : m_nWidth{0},
        m_nHeight{0},
        m_ImageFormat{IMAGE_FORMAT_UNKNOWN},
        m_pBits{nullptr} {}

  ~Bitmap_t() {
    delete[] m_pBits;
    m_pBits = nullptr;
  }

  void Init(int width, int height, ImageFormat image_format) {
    delete[] m_pBits;

    m_nWidth = width;
    m_nHeight = height;
    m_ImageFormat = image_format;
    m_pBits = new u8[width * height * ImageLoader::SizeInBytes(m_ImageFormat)];
  }

  u8 *GetPixel(int x, int y) const {
    if (m_pBits) {
      int nPixelSize = ImageLoader::SizeInBytes(m_ImageFormat);
      return &m_pBits[(m_nWidth * y + x) * nPixelSize];
    }

    return nullptr;
  }

  int m_nWidth;
  int m_nHeight;
  ImageFormat m_ImageFormat;
  u8 *m_pBits;
};

#endif  // BITMAP_BITMAP_H_
