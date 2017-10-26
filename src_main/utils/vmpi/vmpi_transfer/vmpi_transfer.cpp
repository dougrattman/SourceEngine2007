//====== Copyright � 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose:
//
//=============================================================================

#include "vmpi_transfer.h"
#include <conio.h>
#include <windows.h>
#include "cmdlib.h"
#include "tier0/icommandline.h"
#include "tools_minidump.h"
#include "vmpi.h"
#include "vmpi_tools_shared.h"

void MyDisconnectHandler(int procID, const char *pReason) {
  Error("Premature disconnect.\n");
}

void DownloadFile(const char *pCachePath, const char *pRemoteFileBase,
                  const char *pFilename) {
  // Setup local and remote filenames.
  char remoteFilename[MAX_PATH];
  char localFilename[MAX_PATH];
  V_ComposeFileName(pRemoteFileBase, pFilename, remoteFilename,
                    sizeof(remoteFilename));
  V_ComposeFileName(pCachePath, pFilename, localFilename,
                    sizeof(localFilename));

  // Read the file in.
  FileHandle_t fpSrc = g_pFileSystem->Open(remoteFilename, "rb");
  if (fpSrc == FILESYSTEM_INVALID_HANDLE) {
    Error("Unable to open %s on master.\n", remoteFilename);
  }

  unsigned int fileSize = g_pFileSystem->Size(fpSrc);
  CUtlVector<char> data;
  data.SetSize(fileSize);
  g_pFileSystem->Read(data.Base(), fileSize, fpSrc);
  g_pFileSystem->Close(fpSrc);

  // Now write the file to disk.
  FILE *fpDest = fopen(localFilename, "wb");
  if (!fpDest) {
    Error("Can't open %s for writing.\n");
  }
  fwrite(data.Base(), 1, data.Count(), fpDest);
  fclose(fpDest);

  Warning("Got file: %s\n", pFilename);
}

#if 0
SpewRetval_t MySpewFunc( SpewType_t spewType, const tchar *pMsg )
{
	printf( "%s", pMsg );
	if ( spewType == SPEW_ERROR )
	{
		printf( "\nWaiting for keypress to quit...\n" );
		getch();
		TerminateProcess( GetCurrentProcess(), 1 );
	}

	return SPEW_CONTINUE;
}
#endif

int RunVMPITransferWorker(int argc, char **argv) {
  if (!VMPI_Init(argc, argv, NULL, MyDisconnectHandler, VMPI_RUN_NETWORKED,
                 true)) {
    return 1;
  }

  SetupToolsMinidumpHandler(VMPI_ExceptionFilter);

  if (!FileSystem_Init(".", 0, FS_INIT_COMPATIBILITY_MODE)) return 1;

  ICommandLine *pCommandLine = CommandLine();

  // Look for the cache path and file base args.
  const char *pCachePath = pCommandLine->ParmValue("-CachePath", (char *)NULL);
  if (!pCachePath) Error("No -CachePath specified.");

  const char *pRemoteFileBase =
      pCommandLine->ParmValue("-mpi_filebase", (char *)NULL);
  if (!pRemoteFileBase) Error("No -mpi_filebase specified.");

  // Now just ask the master for each file.
  for (size_t i = 1; i < pCommandLine->ParmCount() - 1; i++) {
    const char *pParm = pCommandLine->GetParm(i);
    if (V_stricmp(pParm, "-mpi_file") == 0) {
      const char *pNextParm = pCommandLine->GetParm(i + 1);
      DownloadFile(pCachePath, pRemoteFileBase, pNextParm);
      ++i;
    }
  }

  // Ok, we're done. Write the status file so the service knows all the files
  // are ready to go.
  char statusFilename[MAX_PATH];
  V_ComposeFileName(pCachePath, "ReadyToGo.txt", statusFilename,
                    sizeof(statusFilename));
  FILE *fp = fopen(statusFilename, "wb");
  fclose(fp);

  return 0;
}

// In this mode, we just initialize VMPI appropriately, and it'll host out the
// specified files. The command line to vmpi_transfer is -PatchHost
// -PatchDirectory <directory> Sample: vmpi_transfer -PatchHost
// -mpi_PatchDirectory \\fileserver\vmpi\patch1 -mpi_PatchWorkers <count> <ip1>
// <ip2>... Then it'll tell those workers to connect and it'll send them the
// files in the specified directory.
int RunVMPITransferMaster(int argc, char **argv) {
  // Since we didn't use -mpi_worker on the command line, VMPI will init as the
  // master. We put a special character in front of the dependency filename,
  // which tells it the dependencies consist of every file in the specified
  // directory.
  VMPI_Init_PatchMaster(argc, argv);

  if (!FileSystem_Init(".", 0, FS_INIT_COMPATIBILITY_MODE)) return 1;

  Msg("Hosting patch files. Press ESC to exit. ");
  while (1) {
    VMPI_DispatchNextMessage(100);
    if (kbhit()) {
      if (getch() == 27) break;
    }
  }

  return 0;
}

// ---------------------------------------------------------------------------------
// // Purpose: This app is used by vmpi_service to acquire the executables for
// a VMPI job. When the service is asked to join a job, it runs this program
// to connect to the VMPI master and download all the exes for the job.
//
// This app is ALSO used to do patches. vmpi_browser_services runs it with a
// list of machines it wants to patch. Then it runs as the VMPI master and
// instead of broadcasting its presence, it sends messages to the specific list
// of machines.
// ---------------------------------------------------------------------------------
// //
int main(int argc, char **argv) {
  InstallSpewFunction();
  CommandLine()->CreateCmdLine(argc, argv);

  int ret;
  if (CommandLine()->FindParm("-PatchHost") == 0) {
    ret = RunVMPITransferWorker(argc, argv);
  } else {
    ret = RunVMPITransferMaster(argc, argv);
  }

  CmdLib_Cleanup();
  return ret;
}
