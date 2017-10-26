// Copyright © 1996-2007, Valve Corporation, All rights reserved.
//
//	Purpose: LZMA Glue. Designed for Tool time Encoding/Decoding.
//
//	LZMA SDK 4.43 Copyright (c) 1999-2006 Igor Pavlov (2006-05-01)
//
//	http://www.7-zip.org/
//
//	LZMA SDK is licensed under two licenses:
//
//	1) GNU Lesser General Public License (GNU LGPL)
//	2) Common Public License (CPL)
//
//	It means that you can select one of these two licenses and
//	follow rules of that license.
//
//	SPECIAL EXCEPTION:
//
//	Igor Pavlov, as the author of this Code, expressly permits you to
//	statically or dynamically link your Code (or bind by name) to the
//	interfaces of this file without subjecting your linked Code to the
//	terms of the CPL or GNU LGPL. Any modifications or additions
//	to this file, however, are subject to the LGPL or CPL terms.

#ifndef LZMA_H
#define LZMA_H

#include <cstdint>

// power of two, 256k
#define LZMA_DEFAULT_DICTIONARY 18

// These routines are designed for TOOL TIME encoding/decoding on the PC!
// They have not been made to encode/decode on the PPC and lack big endian
// awarnesss. Lightweight GAME TIME Decoding is part of tier1.lib, via CLZMA.

// Encoding glue. Returns non-null Compressed buffer if successful.
// Caller must free.
unsigned char *LZMA_Compress(uint8_t *input, size_t input_size,
                             size_t *output_size,
                             size_t dinctionary_size = LZMA_DEFAULT_DICTIONARY);

// Decoding glue. Returns true if successful.
bool LZMA_Uncompress(uint8_t *input, uint8_t **output, size_t *output_size);

// Decoding helper, returns TRUE if buffer is LZMA compressed.
bool LZMA_IsCompressed(uint8_t *input);

// Decoding helper, returns non-zero size of data when uncompressed, otherwise
// 0.
unsigned int LZMA_GetActualSize(uint8_t *input);

#endif
