#include "fmcb_patches.h"
#include "init.h"
#include "osdmenu_patches.h"
#include "settings.h"
#include "splash.h"
#include <kernel.h>
#include <loadfile.h>
#include <malloc.h>
#include <sifrpc.h>
#include <string.h>

// Executes selected item by passing it to the launcher
void launchItem(char *item) {
  if (!strcmp(item, "OSDSYS")) {
    // Do nothing
    return;
  }

  restoreGSVideoMode();

  DisableIntc(3);
  DisableIntc(2);

  // Reinitialize IOP to a known state
  initModules();
  SifLoadModule("rom0:CLEARSPU", 0, 0);

  FlushCache(0);
  FlushCache(2);

  if (pCNF != NULL)
    free(pCNF);

  // Build argv for the launcher
  char **argv;
  int argc;
  if (strcmp(item, "cdrom")) {
    argv = malloc(2 * sizeof(char *));
    argv[0] = strdup(settings.launcherPath);
    argv[1] = strdup(item);
    argc = 2;
  } else {
    // Handle CDROM
    argv = malloc(5 * sizeof(char *));
    argv[0] = settings.launcherPath;
    argv[1] = strdup(item);
    argv[2] = (settings.skipPS2LOGO) ? "-nologo" : "";
    argv[3] = (!settings.disableGameID) ? "" : "-nogameid";
    argc = 4;
    if (settings.useDKWDRV) {
      if (!settings.dkwdrvPath)
        argv[4] = (settings.useDKWDRV) ? "-dkwdrv" : "";
      else {
        argv[4] = malloc(sizeof("-dkwdrv") + strlen(settings.dkwdrvPath) + 1);
        strcat(argv[4], "-dkwdrv=");
        strcat(argv[4], settings.dkwdrvPath);
      }
      argc++;
    }
  }

  // Wipe user memory and writeback data cache before loading the ELF
  wipeUserMem();
  FlushCache(0);

  static t_ExecData elfdata;
  elfdata.epc = 0;

  SifLoadFileInit();
  int ret = SifLoadElf(argv[0], &elfdata);
  SifLoadFileExit();
  if (ret == 0 && elfdata.epc != 0) {
    FlushCache(0);
    FlushCache(2);
    ExecPS2((void *)elfdata.epc, (void *)elfdata.gp, argc, argv);
  }
  sceSifExitRpc();
  Exit(-1);
}

// Uses the launcher to run the disc
void launchDisc() { launchItem("cdrom"); }
