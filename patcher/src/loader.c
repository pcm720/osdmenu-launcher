#include "defaults.h"
#include "init.h"
#include "patches_common.h"
#include "patches_osdmenu.h"
#include "settings.h"
#include "splash.h"
#include <kernel.h>
#include <loadfile.h>
#include <malloc.h>
#include <sifrpc.h>
#include <string.h>
#define NEWLIB_PORT_AWARE
#include <fileXio_rpc.h>

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

  // Wipe user memory and reinitialize DMAC, VU0/1, VIF0/1, GIF, IPU
  wipeUserMem();
  ResetEE(0x7F);

  FlushCache(0);
  FlushCache(2);

  // Reinitialize IOP to a known state
  initModules();
  SifLoadModule("rom0:CLEARSPU", 0, 0);

  FlushCache(0);
  FlushCache(2);

  if (fileXioMount("pfs0:", HOSD_SYS_PARTITION, 0))
    Exit(-1);

  // Build argv for the launcher
  char **argv;
  int argc;
  if (strcmp(item, "cdrom")) {
    argv = malloc(2 * sizeof(char *));
    argv[0] = "pfs0:" HOSD_LAUNCHER_PATH;
    argv[1] = strdup(item);
    argc = 2;
  } else {
    // Handle CDROM
    argv = malloc(5 * sizeof(char *));
    argv[0] = "pfs0:" HOSD_LAUNCHER_PATH;
    argv[1] = strdup(item);
    argv[2] = (settings.patcherFlags & FLAG_SKIP_PS2_LOGO) ? "-nologo" : "";
    argv[3] = (!(settings.patcherFlags & FLAG_DISABLE_GAMEID)) ? "" : "-nogameid";
    argc = 4;
    if (settings.patcherFlags & FLAG_USE_DKWDRV) {
      argv[4] = "-dkwdrv=" HOSD_DKWDRV_PATH;
      argc++;
    }
  }

  static t_ExecData elfdata;
  elfdata.epc = 0;

  SifLoadFileInit();
  int ret = SifLoadElf(argv[0], &elfdata);
  SifLoadFileExit();

  fileXioUmount("pfs0:");

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
