// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "sv_uploaddata.h"

#if defined(_WIN32)
#include "base/include/windows/windows_light.h"

#include <winsock.h>
#elif _LINUX
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#include "blockingudpsocket.h"
#include "cserserverprotocol_engine.h"
#include "host.h"
#include "mathlib/IceKey.H"
#include "net.h"
#include "tier1/bitbuf.h"
#include "tier1/keyvalues.h"

#include "tier0/include/memdbgon.h"

static int CountFields(KeyValues *fields) {
  int c = 0;
  KeyValues *kv = fields->GetFirstSubKey();
  while (kv) {
    c++;
    kv = kv->GetNextKey();
  }
  return c;
}

//-----------------------------------------------------------------------------
// Purpose: encrypts an 8-uint8_t sequence
//-----------------------------------------------------------------------------
static inline void Encrypt8ByteSequence(IceKey &cipher,
                                        const unsigned char *plainText,
                                        unsigned char *cipherText) {
  cipher.encrypt(plainText, cipherText);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
static void EncryptBuffer(IceKey &cipher, unsigned char *bufData,
                          uint32_t bufferSize) {
  unsigned char *cipherText = bufData;
  unsigned char *plainText = bufData;
  uint32_t bytesEncrypted = 0;

  while (bytesEncrypted < bufferSize) {
    // encrypt 8 uint8_t section
    Encrypt8ByteSequence(cipher, plainText, cipherText);
    bytesEncrypted += 8;
    cipherText += 8;
    plainText += 8;
  }
}

static void BuildUploadDataMessage(bf_write &buf, char const *tablename,
                                   KeyValues *fields) {
  bf_write encrypted;
  uint8_t encrypted_data[2048];

  buf.WriteByte(C2M_UPLOADDATA);
  buf.WriteByte('\n');
  buf.WriteByte(C2M_UPLOADDATA_PROTOCOL_VERSION);

  // encryption object
  IceKey cipher(1); /* medium encryption level */
  unsigned char ucEncryptionKey[8] = {54, 175, 165, 5, 76, 251, 29, 113};
  cipher.set(ucEncryptionKey);

  encrypted.StartWriting(encrypted_data, sizeof(encrypted_data));

  uint8_t corruption_identifier = 0x01;

  encrypted.WriteByte(corruption_identifier);

  // Data version protocol
  encrypted.WriteByte(C2M_UPLOADDATA_DATA_VERSION);

  encrypted.WriteString(tablename);

  int fieldCount = CountFields(fields);

  if (fieldCount > 255) {
    Host_Error("Too many fields in uploaddata (%i max = 255)\n", fieldCount);
  }

  encrypted.WriteByte((uint8_t)fieldCount);

  KeyValues *kv = fields->GetFirstSubKey();
  while (kv) {
    encrypted.WriteString(kv->GetName());
    encrypted.WriteString(kv->GetString());

    kv = kv->GetNextKey();
  }

  // Round to multiple of 8 for encrypted
  while (encrypted.GetNumBytesWritten() % 8) {
    encrypted.WriteByte(0);
  }

  EncryptBuffer(cipher, (unsigned char *)encrypted.GetData(),
                encrypted.GetNumBytesWritten());

  buf.WriteShort((int)encrypted.GetNumBytesWritten());
  buf.WriteBytes((unsigned char *)encrypted.GetData(),
                 encrypted.GetNumBytesWritten());
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *cserIP -
//			*tablename -
//			*fields -
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool UploadData(char const *cserIP, char const *tablename, KeyValues *fields) {
#ifndef _XBOX
  bf_write buf;
  uint8_t data[2048];

  buf.StartWriting(data, sizeof(data));

  BuildUploadDataMessage(buf, tablename, fields);

  netadr_t cseradr;

  if (NET_StringToAdr(cserIP, &cseradr)) {
    CBlockingUDPSocket *socket = new CBlockingUDPSocket();
    if (socket) {
      struct sockaddr_in sa;
      cseradr.ToSockadr((struct sockaddr *)&sa);

      // Don't bother waiting for response here
      socket->SendSocketMessage(sa, (const uint8_t *)buf.GetData(),
                                buf.GetNumBytesWritten());
      delete socket;

      return true;
    }
  }

  return false;
#else
  return true;
#endif
}

/*
CON_COMMAND( datatest, "" )
{
        KeyValues *kv = new KeyValues( "data" );
        kv->SetString( "IDHash", "abcdefg" );
        kv->SetString( "Time", "DATETIME" );
        kv->SetString( "DXDeviceID", "1001" );
        kv->SetString( "DXVendorID", "1001" );
        kv->SetString( "Framerate", "999" );
        kv->SetString( "BuildNumber", va( "%i", build_number() ) );

        bool bret = UploadData( "127.0.0.1:27013", "benchmark", kv );

        kv->deleteThis();
}
*/
