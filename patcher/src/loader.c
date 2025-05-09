#include "defaults.h"
#include "gs.h"
#include "init.h"
#include "patches_common.h"
#include "patches_osdmenu.h"
#include "settings.h"
#include <kernel.h>
#include <loadfile.h>
#include <malloc.h>
#include <sifrpc.h>
#include <string.h>

#ifdef HOSD
#define NEWLIB_PORT_AWARE
#include <fileXio_rpc.h>
#endif

// Executes selected item by passing it to the launcher
void launchItem(char *item) {
  DisableIntc(3);
  DisableIntc(2);

  // SIF operations do not work on Protokernels
  // without terminating all other threads
  uint32_t tID = GetThreadId();
  ChangeThreadPriority(tID, 0);
  CancelWakeupThread(tID);
  for (int i = 0; i < 0x100; i++) {
    if (i != tID) {
      TerminateThread(i);
      DeleteThread(i);
    }
  }

  // Revert video patch and deinit OSDSYS
  restoreGSVideoMode();
  deinitOSDSYS();
  // Clear the screen
  gsInit(settings.videoMode);

#ifdef HOSD
  // Wipe user memory, important for HDD-OSD
  wipeUserMem();
#endif
  // Reinitialize DMAC, VU0/1, VIF0/1, GIF, IPU
  ResetEE(0x7F);

  FlushCache(0);
  FlushCache(2);

  // Reinitialize IOP to a known state
  initModules();
  SifLoadModule("rom0:CLEARSPU", 0, 0);

#ifdef HOSD
  // Mount the partition containing launcher.elf
  if (fileXioMount("pfs0:", HOSD_SYS_PARTITION, 0))
    Exit(-1);
#endif

  // Build argv for the launcher
  char **argv;
  int argc;
  if (strcmp(item, "cdrom")) {
    argv = malloc(2 * sizeof(char *));
    argv[1] = strdup(item);
    argc = 2;
  } else {
    // Handle CDROM
    argv = malloc(5 * sizeof(char *));
    argv[1] = strdup(item);
    argv[2] = (settings.patcherFlags & FLAG_SKIP_PS2_LOGO) ? "-nologo" : "";
    argv[3] = (!(settings.patcherFlags & FLAG_DISABLE_GAMEID)) ? "" : "-nogameid";
    argc = 4;
    if (settings.patcherFlags & FLAG_USE_DKWDRV) {
#ifndef HOSD
      if (settings.dkwdrvPath[0] == '\0')
        argv[4] = "-dkwdrv";
      else {
        argv[4] = malloc(sizeof("-dkwdrv") + strlen(settings.dkwdrvPath) + 1);
        strcat(argv[4], "-dkwdrv=");
        strcat(argv[4], settings.dkwdrvPath);
      }
#else // HOSD DKWDRV path is fixed
      argv[4] = "-dkwdrv=" HOSD_DKWDRV_PATH;
#endif
      argc++;
    }
  }

// Set launcher path
#ifndef HOSD
  argv[0] = settings.launcherPath;
#else
  // launcher.elf handles pfs0 cwd properly, so we don't need to build the full path
  argv[0] = "pfs0:" HOSD_LAUNCHER_PATH;
#endif

  static t_ExecData elfdata;
  elfdata.epc = 0;

  SifLoadFileInit();
  int ret = SifLoadElf(argv[0], &elfdata);
  SifLoadFileExit();

#ifdef HOSD
  // Unmount the partition
  fileXioUmount("pfs0:");
#endif

  sceSifExitRpc();
  if (ret == 0 && elfdata.epc != 0) {
    FlushCache(0);
    FlushCache(2);
    ExecPS2((void *)elfdata.epc, (void *)elfdata.gp, argc, argv);
  }
  Exit(-1);
}

// Uses the launcher to run the disc
void launchDisc() { launchItem("cdrom"); }
