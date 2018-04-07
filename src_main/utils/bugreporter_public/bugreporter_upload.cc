// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Authors:
//  Yahn Bernier
//
// Description and general notes:
//  Handles uploading bug report data blobs to the CSERServer (Client Stats &
//  Error Reporting Server).

#include "base/include/windows/windows_light.h"

#include <winsock.h>
#include <cinttypes>
#include <cstdarg>
#include <cstdio>
#include <memory>
#include "blockingudpsocket.h"
#include "cserserverprotocol_engine.h"
#include "deps/libice/IceKey.h"
#include "filesystem_tools.h"
#include "shared_file_system.h"
#include "steamcommon.h"
#include "tier0/include/basetypes.h"
#include "tier1/bitbuf.h"
#include "tier1/netadr.h"
#include "tier1/strtools.h"
#include "tier1/utlbuffer.h"
#include "tier1/utlvector.h"

enum class FileType {
  kBugReport,

  // Count number of legal values.
  kMaxFileType
};

enum class SendMethod {
  kWholeRawFileNoBlocks,
  // TODO: Reenable compressed sending of minidumps.
  kCompressedBlocks,

  // Count number of legal values.
  kMaxSendMethod
};

// TODO: cut protocol version down to u8 if possible, to reduce bandwidth usage
// for very frequent but tiny commands.
using ProtocolVersion = u32;
using ProtocolAcceptanceFlag = u8;
using ProtocolUnacceptableAck = u8;

using MessageSequenceId = u32;
using NetworkTransactionId = u32;
using ServerSessionHandle = u32;
using ClientSessionHandle = u32;

// Command codes are intentionally as small as possible to minimize bandwidth
// usage for very frequent but tiny commands (e.g. GDS 'FindServer' commands).
using Command = u8;
// ... likewise response codes are as small as possible - we use this when we
// ... can and revert to large types on a case by case basis.
using CommandResponse_t = u8;
// This is for mapping requests back to error ids for placing into the database
// appropriately.
using ContextID = u32;

// This define our standard type for length prefix for variable length messages
// in wire protocols.
// This is specifically used by
// CWSABUFWrapper::PrepareToReceiveLengthPrefixedMessage() and its supporting
// functions. It is defined here for generic (portable) network code to use when
// constructing messages to be sent to peers that use the above function. e.g.
// SteamValidateUserIDTickets.dll uses this for that purpose.

// We support u16 or u32 (obviously switching between them breaks existing
// protocols unless all components are switched simultaneously).
using NetworkMessageLengthPrefix = u32;
// Similarly, strings should be preceeded by their length.
using StringLengthPrefix = u16;

constexpr ProtocolAcceptanceFlag cuProtocolIsNotAcceptable{
    static_cast<ProtocolAcceptanceFlag>(0)};
constexpr ProtocolAcceptanceFlag cuProtocolIsAcceptable{
    static_cast<ProtocolAcceptanceFlag>(1)};

constexpr Command cuMaxCommand{static_cast<Command>(255)};
constexpr CommandResponse_t cuMaxCommandResponse{
    static_cast<CommandResponse_t>(255)};

// This is the version of the protocol used by latest-build clients.
constexpr ProtocolVersion cuCurrentProtocolVersion{1};

// This is the minimum protocol version number that the client must
// be able to speak in order to communicate with the server.
// The client sends its protocol version this before every command, and if we
// don't support that version anymore then we tell it nicely.  The client
// should respond by doing an auto-update.
constexpr ProtocolVersion cuRequiredProtocolVersion{1};

namespace Commands {
constexpr Command cuGracefulClose{0};
constexpr Command cuSendBugReport{1};
constexpr Command cuNumCommands{2};
constexpr Command cuNoCommandReceivedYet{cuMaxCommand};
}  // namespace Commands

namespace HarvestFileCommand {
typedef u32 SenderTypeId_t;
typedef u32 SenderTypeUniqueId_t;
typedef u32 SenderSourceCodeControlId_t;
typedef u32 FileSize_t;

// Legal values defined by EFileType
typedef u32 FileType_t;

// Legal values defined by ESendMethod
typedef u32 SendMethod_t;

constexpr CommandResponse_t cuOkToSendFile = 0;
constexpr CommandResponse_t cuFileTooBig = 1;
constexpr CommandResponse_t cuInvalidSendMethod = 2;
constexpr CommandResponse_t cuInvalidMaxCompressedChunkSize = 3;
constexpr CommandResponse_t cuInvalidBugReportContext = 4;
constexpr uint32_t cuNumCommandResponses = 5;
}  // namespace HarvestFileCommand

enum class BugReportUploadStatus {
  // General statuses.
  kSucceeded = 0,
  kFailed,

  // Specific statuses.
  kBadParameter,
  kUnknownStatus,
  kSendingBugReportHeaderSucceeded,
  kSendingBugReportHeaderFailed,
  kReceivingResponseSucceeded,
  kReceivingResponseFailed,
  kConnectToCSERServerSucceeded,
  kConnectToCSERServerFailed,
  kUploadingBugReportSucceeded,
  kUploadingBugReportFailed
};

struct BugReportProgress {
  // A text string describing the current progress.
  char status[512];
};

using BugReportReportProgressFunc =
    void (*)(u32 context, const BugReportProgress& report_progress);

static void BugUploadProgress(u32 context,
                              const BugReportProgress& report_progress) {
  DevMsg("%s\n", report_progress.status);
}

struct BugReportParameters {
  // IP Address of the CSERServer to send the report to.
  netadr_t cserServerIp;
  TSteamGlobalUserID steamUserId;

  // Source Control Id (or build_number) of the product.
  u32 buildNumber;
  // Name of the .exe.
  char exeName[64];
  // Game directory.
  char gameDirectory[64];
  // Map name the server wants to upload statistics about.
  char mapName[64];

  u32 ram, cpu;

  char cpuDescription[128];

  u32 dxVersionHigh, dxVersionLow;
  u32 dxVendorId, dxDeviceId;

  char osVersion[64];

  char reportType[40];
  char email[80];
  char accountName[64];

  char title[128];
  char body[1024];

  u32 attachmentFileSize;
  char attachmentFile[128];

  u32 progressContext;
  BugReportReportProgressFunc reportProgressCallback;
};

// Note that this API is blocking, though the callback, if passed, can occur
// during execution.
BugReportUploadStatus Win32UploadBugReportBlocking(
    const BugReportParameters& rBugReportParameters);

void EncryptBuffer(IceKey& cipher, unsigned char* buffer, int size) {
  unsigned char *plain = buffer, *crypted = buffer;
  int encrypted_bytes = 0;

  while (encrypted_bytes < size) {
    // encrypt 8 byte section
    cipher.encrypt(plain, crypted);
    encrypted_bytes += 8;
    plain += 8;
    crypted += 8;
  }
}

bool UploadBugReport(const netadr_t& cserIP, const TSteamGlobalUserID& userid,
                     int build, char const* title, char const* body,
                     char const* exename, char const* game_dir,
                     char const* mapname, char const* reporttype,
                     char const* email, char const* accountname, int ram,
                     int cpu, char const* processor, unsigned int high,
                     unsigned int low, unsigned int vendor, unsigned int device,
                     char const* osversion, char const* attachedfile,
                     unsigned int attachedfilesize) {
  BugReportParameters params{0};
  params.cserServerIp = cserIP;
  params.steamUserId = userid;

  params.buildNumber = build;
  Q_strncpy(params.exeName, exename, sizeof(params.exeName));
  Q_strncpy(params.gameDirectory, game_dir, sizeof(params.gameDirectory));
  Q_strncpy(params.mapName, mapname, sizeof(params.mapName));

  params.ram = ram;
  params.cpu = cpu;
  Q_strncpy(params.cpuDescription, processor, sizeof(params.cpuDescription));

  params.dxVersionHigh = high;
  params.dxVersionLow = low;
  params.dxVendorId = vendor;
  params.dxDeviceId = device;

  Q_strncpy(params.osVersion, osversion, sizeof(params.osVersion));

  Q_strncpy(params.reportType, reporttype, sizeof(params.reportType));
  Q_strncpy(params.email, email, sizeof(params.email));
  Q_strncpy(params.accountName, accountname, sizeof(params.accountName));

  Q_strncpy(params.title, title, sizeof(params.title));
  Q_strncpy(params.body, body, sizeof(params.body));

  Q_strncpy(params.attachmentFile, attachedfile, sizeof(params.attachmentFile));
  params.attachmentFileSize = attachedfilesize;

  params.progressContext = 1u;
  params.reportProgressCallback = BugUploadProgress;

  BugReportUploadStatus result = Win32UploadBugReportBlocking(params);

  return result == BugReportUploadStatus::kSucceeded;
}

void UpdateProgress(const BugReportParameters& params, char const* fmt, ...) {
  if (!params.reportProgressCallback) return;

  char buffer[2048];
  va_list argptr;
  va_start(argptr, fmt);
  _vsnprintf_s(buffer, SOURCE_ARRAYSIZE(buffer) - 1, fmt, argptr);
  va_end(argptr);

  char status[2060];
  sprintf_s(status, "(%u): %s", params.progressContext, buffer);

  BugReportProgress progress;
  strcpy_s(progress.status, status);

  // Invoke the callback
  (*params.reportProgressCallback)(params.progressContext, progress);
}

class Win32UploadBugReport {
 public:
  Win32UploadBugReport(const netadr_t& harvester,
                       const BugReportParameters& rBugReportParameters,
                       u32 contextid)
      : states_(),
        current_state_(State::kCreateTcpSocket),
        harvester_socket_address_(),
        socket_tcp_(0),
        bug_report_params_(rBugReportParameters),
        context_id_(contextid) {
    harvester.ToSockadr((struct sockaddr*)&harvester_socket_address_);

    AddState(State::kCreateTcpSocket, &Win32UploadBugReport::CreateTcpSocket);
    AddState(State::kConnectToHarvesterServer,
             &Win32UploadBugReport::ConnectToHarvesterServer);
    AddState(State::kSendProtocolVersion,
             &Win32UploadBugReport::SendProtocolVersion);
    AddState(State::kReceiveProtocolOkay,
             &Win32UploadBugReport::ReceiveProtocolOkay);
    AddState(State::kSendUploadCommand,
             &Win32UploadBugReport::SendUploadCommand);
    AddState(State::kReceiveOKToSendFile,
             &Win32UploadBugReport::ReceiveOkToSendFile);
    AddState(State::kSendWholeFile, &Win32UploadBugReport::SendWholeFile);
    AddState(State::kReceiveFileUploadSuccess,
             &Win32UploadBugReport::ReceiveFileUploadSuccess);
    AddState(State::kSendGracefulClose,
             &Win32UploadBugReport::SendGracefulClose);
    AddState(State::kCloseTcpSocket, &Win32UploadBugReport::CloseTCPSocket);
  }

  ~Win32UploadBugReport() {
    if (socket_tcp_ != 0) {
      closesocket(socket_tcp_);  // lint !e534
      socket_tcp_ = 0;
    }
  }

  BugReportUploadStatus Upload(CUtlBuffer& buffer) {
    UpdateProgress(bug_report_params_,
                   "Commencing bug report upload connection.");

    auto result = BugReportUploadStatus::kSucceeded;

    // Run the state machine
    while (true) {
      Assert(
          states_[static_cast<std::underlying_type_t<decltype(current_state_)>>(
                      current_state_)]
              .state == current_state_);

      ProtocolStateHandlerFunc handler =
          states_[static_cast<std::underlying_type_t<decltype(current_state_)>>(
                      current_state_)]
              .handler;

      if (!(this->*handler)(result, buffer)) {
        return result;
      }
    }
  }

 private:
  enum class State : uint32_t {
    kCreateTcpSocket = 0,
    kConnectToHarvesterServer,
    kSendProtocolVersion,
    kReceiveProtocolOkay,
    kSendUploadCommand,
    kReceiveOKToSendFile,
    // This could push chunks onto the wire, but we'll just use a whole buffer
    // for now.
    kSendWholeFile,
    kReceiveFileUploadSuccess,
    kSendGracefulClose,
    kCloseTcpSocket
  };

  using ProtocolStateHandlerFunc = bool (Win32UploadBugReport::*)(
      BugReportUploadStatus& status, CUtlBuffer& buffer);

  struct StateHandlerMap {
    StateHandlerMap(State state, ProtocolStateHandlerFunc handler)
        : state{state}, handler{handler} {}

    State state;
    ProtocolStateHandlerFunc handler;
  };

  bool CreateTcpSocket(BugReportUploadStatus& status, CUtlBuffer& buffer) {
    UpdateProgress(bug_report_params_, "Creating bug report upload socket.");

    socket_tcp_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket_tcp_ != INVALID_SOCKET) {
      SetNextState(State::kConnectToHarvesterServer);
      return true;
    }

    UpdateProgress(bug_report_params_, "CreateTcpSocket failed (%d).",
                   WSAGetLastError());

    status = BugReportUploadStatus::kFailed;
    return false;
  }

  bool ConnectToHarvesterServer(BugReportUploadStatus& status,
                                CUtlBuffer& buffer) {
    UpdateProgress(bug_report_params_,
                   "Connecting to bug report harvesting server.");

    if (connect(socket_tcp_, (const sockaddr*)&harvester_socket_address_,
                sizeof(harvester_socket_address_)) != SOCKET_ERROR) {
      SetNextState(State::kSendProtocolVersion);
      return true;
    }

    UpdateProgress(bug_report_params_, "ConnectToHarvesterServer failed (%d).",
                   WSAGetLastError());

    status = BugReportUploadStatus::kConnectToCSERServerFailed;
    return false;
  }

  bool SendProtocolVersion(BugReportUploadStatus& status, CUtlBuffer& buffer) {
    UpdateProgress(bug_report_params_,
                   "Sending bug report harvester protocol info.");
    buffer.SetBigEndian(true);
    buffer.Purge();
    buffer.PutInt(cuCurrentProtocolVersion);

    // Send protocol version.
    if (send(socket_tcp_, (const char*)buffer.Base(), buffer.TellPut(), 0) !=
        SOCKET_ERROR) {
      SetNextState(State::kReceiveProtocolOkay);
      return true;
    }

    UpdateProgress(bug_report_params_, "SendProtocolVersion failed (%d).",
                   WSAGetLastError());

    status = BugReportUploadStatus::kFailed;
    return false;
  }

  bool ReceiveProtocolOkay(BugReportUploadStatus& status, CUtlBuffer& buffer) {
    UpdateProgress(bug_report_params_,
                   "Receiving harvesting protocol acknowledgement.");
    buffer.Purge();

    // Now receive the protocol is acceptable token from the server.
    if (!IsReceive(1, buffer)) {
      UpdateProgress(bug_report_params_, "Receive protocol failed.");

      status = BugReportUploadStatus::kFailed;
      return false;
    }

    u8 response = buffer.GetChar();
    if (!!response) {
      UpdateProgress(bug_report_params_, "Protocol OK.");

      SetNextState(State::kSendUploadCommand);
      return true;
    }

    UpdateProgress(bug_report_params_,
                   "Server was rejected protocol (response " PRIu8 ").",
                   response);

    status = BugReportUploadStatus::kFailed;
    return false;
  }

  bool SendUploadCommand(BugReportUploadStatus& status, CUtlBuffer& buffer) {
    UpdateProgress(bug_report_params_,
                   "Sending harvesting protocol upload request.");
    // Send upload command.
    buffer.Purge();

    NetworkMessageLengthPrefix messageSize(
        sizeof(Command) + sizeof(ContextID) +
        sizeof(HarvestFileCommand::FileSize_t) +
        sizeof(HarvestFileCommand::SendMethod_t) +
        sizeof(HarvestFileCommand::FileSize_t));

    // Prefix the length to the command.
    buffer.PutInt(messageSize);
    buffer.PutChar(Commands::cuSendBugReport);
    buffer.PutInt(context_id_);

    buffer.PutInt(bug_report_params_.attachmentFileSize);
    buffer.PutInt(static_cast<HarvestFileCommand::SendMethod_t>(
        SendMethod::kWholeRawFileNoBlocks));
    buffer.PutInt(static_cast<HarvestFileCommand::FileSize_t>(0));

    if (send(socket_tcp_, (const char*)buffer.Base(), buffer.TellPut(), 0) !=
        SOCKET_ERROR) {
      SetNextState(State::kReceiveOKToSendFile);
      return true;
    }

    UpdateProgress(bug_report_params_, "Send file upload command failed (%d).",
                   WSAGetLastError());

    status = BugReportUploadStatus::kFailed;
    return false;
  }

  bool ReceiveOkToSendFile(BugReportUploadStatus& status, CUtlBuffer& buffer) {
    UpdateProgress(
        bug_report_params_,
        "Receive bug report harvesting protocol upload permissible.");

    // Now receive the protocol is acceptable token from the server.
    if (!IsReceive(1, buffer)) {
      UpdateProgress(bug_report_params_, "Receive ok to send file failed.");

      status = BugReportUploadStatus::kFailed;
      return false;
    }

    auto command_response = (CommandResponse_t)buffer.GetChar();
    if (command_response == HarvestFileCommand::cuOkToSendFile) {
      SetNextState(State::kSendWholeFile);
      return true;
    }

    UpdateProgress(bug_report_params_,
                   "Server rejected upload command (response %" PRIu8 ").",
                   command_response);

    status = BugReportUploadStatus::kFailed;
    return false;
  }

  bool SendWholeFile(BugReportUploadStatus& status, CUtlBuffer& buffer) {
    UpdateProgress(bug_report_params_, "Uploading bug report data.");

    IBaseFileSystem* file_system = GetSharedFileSystem();
    size_t file_size =
        file_system ? file_system->Size(bug_report_params_.attachmentFile) : 0;
    std::unique_ptr<char[]> file_buffer =
        file_size > 0 ? std::make_unique<char[]>(file_size + 1) : nullptr;

    if (file_size && file_buffer && file_system) {
      FileHandle_t handle{
          file_system->Open(bug_report_params_.attachmentFile, "rb")};

      if (FILESYSTEM_INVALID_HANDLE != handle) {
        file_system->Read(file_buffer.get(), file_size, handle);
        file_system->Close(handle);
      }

      file_buffer[file_size] = '\0';
    } else {
      UpdateProgress(
          bug_report_params_,
          "Bug .zip file size zero or unable to allocate memory for file.");

      status = BugReportUploadStatus::kFailed;
      return false;
    }

    if (send(socket_tcp_, file_buffer.get(), file_size, 0) != SOCKET_ERROR) {
      SetNextState(State::kReceiveFileUploadSuccess);
      return true;
    }

    UpdateProgress(bug_report_params_, "Send whole file failed (%d).",
                   WSAGetLastError());
    status = BugReportUploadStatus::kFailed;
    return false;
  }

  bool ReceiveFileUploadSuccess(BugReportUploadStatus& status,
                                CUtlBuffer& buffer) {
    UpdateProgress(bug_report_params_,
                   "Receiving bug report upload success/fail message.");

    // Now receive the protocol is acceptable token from the server
    if (!IsReceive(1, buffer)) {
      UpdateProgress(bug_report_params_, "Receive file upload success failed.");

      status = BugReportUploadStatus::kFailed;
      return false;
    }

    const u8 response = buffer.GetChar();
    if (response == 1) {
      UpdateProgress(bug_report_params_, "Upload OK.");

      SetNextState(State::kSendGracefulClose);
      return true;
    }

    UpdateProgress(bug_report_params_,
                   "File upload failed (response %" PRIu8 ").", response);

    status = BugReportUploadStatus::kFailed;
    return false;
  }

  bool SendGracefulClose(BugReportUploadStatus& status, CUtlBuffer& buffer) {
    UpdateProgress(bug_report_params_, "Closing connection to server.");

    buffer.Purge();
    buffer.PutInt(sizeof(Command));
    buffer.PutChar(Commands::cuGracefulClose);

    // Now send disconnect command
    if (send(socket_tcp_, (const char*)buffer.Base(), buffer.TellPut(), 0) !=
        SOCKET_ERROR) {
      SetNextState(State::kCloseTcpSocket);
      return true;
    }

    UpdateProgress(bug_report_params_,
                   "Send graceful close connection failed (%d).",
                   WSAGetLastError());

    status = BugReportUploadStatus::kFailed;
    return false;
  }

  bool CloseTCPSocket(BugReportUploadStatus& status, CUtlBuffer& buffer) {
    UpdateProgress(bug_report_params_, "Closing socket, upload succeeded.");

    if (closesocket(socket_tcp_) == SOCKET_ERROR) {
      UpdateProgress(bug_report_params_, "Closing socket failed (%d).",
                     WSAGetLastError());
    }

    socket_tcp_ = 0;

    status = BugReportUploadStatus::kSucceeded;
    // NOTE: Returning false here ends the state machine!!!
    return false;
  }

  void AddState(State StateIndex, ProtocolStateHandlerFunc handler) {
    states_.AddToTail(StateHandlerMap{StateIndex, handler});
  }

  void SetNextState(State StateIndex) {
    Assert(StateIndex > current_state_);
    current_state_ = StateIndex;
  }

  bool IsReceive(int expected_bytes, CUtlBuffer& buffer) {
    buffer.Purge();

    while (true) {
      char temp[8192];
      const int bytes_received = recv(socket_tcp_, temp, sizeof(temp), 0);
      if (bytes_received == SOCKET_ERROR) {
        UpdateProgress(bug_report_params_, "Receive failed (%d).",
                       WSAGetLastError());
        return false;
      }

      if (bytes_received <= 0) return false;

      buffer.Put(temp, bytes_received);
      if (buffer.TellPut() >= expected_bytes) break;
    }

    return true;
  }

 private:
  CUtlVector<StateHandlerMap> states_;
  State current_state_;
  sockaddr_in harvester_socket_address_;
  SOCKET socket_tcp_;
  const BugReportParameters& bug_report_params_;  // lint !e1725
  u32 context_id_;
};

// Note that this API is blocking, though the callback, if passed, can occur
// during execution.
BugReportUploadStatus Win32UploadBugReportBlocking(
    const BugReportParameters& bug_report_params) {
  auto status = BugReportUploadStatus::kFailed;
  UpdateProgress(bug_report_params, "Creating initial report.");

  CUtlBuffer buffer{2048};
  buffer.SetBigEndian(false);
  buffer.Purge();
  buffer.PutChar(C2M_BUGREPORT);
  buffer.PutChar('\n');
  buffer.PutChar(C2M_BUGREPORT_PROTOCOL_VERSION);

  // See CSERServerProtocol.h for format
  constexpr u8 kCorruptionIdentifier{0x01};

  CUtlBuffer encrypted{2000};
  encrypted.PutChar(kCorruptionIdentifier);
  encrypted.PutInt(bug_report_params.buildNumber);  // build_identifier
  encrypted.PutString(bug_report_params.exeName);
  encrypted.PutString(bug_report_params.gameDirectory);
  encrypted.PutString(bug_report_params.mapName);
  encrypted.PutInt(bug_report_params.ram);  // ram mb
  encrypted.PutInt(bug_report_params.cpu);  // cpu mhz
  encrypted.PutString(bug_report_params.cpuDescription);
  // dx high part
  encrypted.PutInt(bug_report_params.dxVersionHigh);
  // dx low part
  encrypted.PutInt(bug_report_params.dxVersionLow);
  // dxvendor id
  encrypted.PutInt(bug_report_params.dxVendorId);
  // dxdevice id
  encrypted.PutInt(bug_report_params.dxDeviceId);
  encrypted.PutString(bug_report_params.osVersion);
  encrypted.PutInt(bug_report_params.attachmentFileSize);

  // protocol version 2 stuff
  encrypted.PutString(bug_report_params.reportType);
  encrypted.PutString(bug_report_params.email);
  encrypted.PutString(bug_report_params.accountName);

  // protocol version 3 stuff
  encrypted.Put(&bug_report_params.steamUserId,
                sizeof(bug_report_params.steamUserId));

  encrypted.PutString(bug_report_params.title);

  int body_length = Q_strlen(bug_report_params.body) + 1;
  encrypted.PutInt(body_length);
  encrypted.Put(bug_report_params.body, body_length);

  while (encrypted.TellPut() % 8) {
    encrypted.PutChar(0);
  }

  constexpr unsigned char kEncryptionKey[8] = {200, 145, 10,  149,
                                               195, 190, 108, 243};
  IceKey cipher{1};  // Medium encryption level.
  cipher.set(kEncryptionKey);

  EncryptBuffer(cipher, (unsigned char*)encrypted.Base(), encrypted.TellPut());

  buffer.PutShort(encrypted.TellPut());
  buffer.Put((unsigned char*)encrypted.Base(), encrypted.TellPut());

  CBlockingUDPSocket bcs;
  if (!bcs.IsValid()) return BugReportUploadStatus::kFailed;

  sockaddr_in sa;
  bug_report_params.cserServerIp.ToSockadr((sockaddr*)&sa);

  UpdateProgress(bug_report_params, "Sending bug report to server.");

  bcs.SendSocketMessage(sa, (const u8*)buffer.Base(), buffer.TellPut());

  UpdateProgress(bug_report_params, "Waiting for response.");

  if (bcs.WaitForMessage(2.0f)) {
    UpdateProgress(bug_report_params, "Received response.");

    buffer.EnsureCapacity(2048);

    sockaddr_in reply_address;
    u32 bytes_received =
        bcs.ReceiveSocketMessage(&reply_address, (u8*)buffer.Base(), 2048);
    if (bytes_received > 0) {
      // Fixup actual size
      buffer.SeekPut(CUtlBuffer::SEEK_HEAD, bytes_received);

      UpdateProgress(bug_report_params, "Checking response.");

      // Parse out data
      u8 msg_type = (u8)buffer.GetChar();
      if (M2C_ACKBUGREPORT != msg_type) {
        UpdateProgress(bug_report_params,
                       "Request denied, invalid message type.");
        return BugReportUploadStatus::kSendingBugReportHeaderFailed;
      }

      bool is_valid_protocol = (u8)buffer.GetChar() == 1;
      if (!is_valid_protocol) {
        UpdateProgress(bug_report_params,
                       "Request denied, invalid message protocol.");
        return BugReportUploadStatus::kSendingBugReportHeaderFailed;
      }

      u8 disposition = (u8)buffer.GetChar();
      if (BR_REQEST_FILES != disposition) {
        // Server doesn't want a bug report, oh well
        if (bug_report_params.attachmentFileSize > 0) {
          UpdateProgress(
              bug_report_params,
              "Bug report accepted, attachment rejected (server too busy)");
        } else {
          UpdateProgress(bug_report_params, "Bug report accepted.");
        }

        return BugReportUploadStatus::kSucceeded;
      }

      // Read in the bug report info parameters
      u32 harvester_ip = buffer.GetInt();
      u16 harvester_port = buffer.GetShort();
      u32 context = buffer.GetInt();

      sockaddr_in harvester_sock_address;
      harvester_sock_address.sin_family = AF_INET;
      harvester_sock_address.sin_port = htons(harvester_port);
      harvester_sock_address.sin_addr.S_un.S_addr = harvester_ip;

      netadr_t harvester_address;
      harvester_address.SetFromSockadr((sockaddr*)&harvester_sock_address);

      UpdateProgress(bug_report_params, "Server requested bug report upload.");

      // Keep using the same scratch buffer for messaging
      Win32UploadBugReport uploader{harvester_address, bug_report_params,
                                    context};

      status = uploader.Upload(buffer);
    }
  } else {
    UpdateProgress(bug_report_params,
                   "No response from server in 2 seconds...");
  }

  return status;
}
