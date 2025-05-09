#include "defaults.h"
#include "gs.h"
#include "init.h"
#include "patches_common.h"
#include "settings.h"
#include "splash.h"
#include <kernel.h>
#include <ps2sdkapi.h>
#include <stdlib.h>
#include <string.h>
#define NEWLIB_PORT_AWARE

// Reduce binary size by disabling the unneeded functionality
void _libcglue_init() {}
void _libcglue_deinit() {}
void _libcglue_args_parse(int argc, char **argv) {}
DISABLE_PATCHED_FUNCTIONS();
DISABLE_EXTRA_TIMERS_FUNCTIONS();
PS2_DISABLE_AUTOSTART_PTHREAD();

#ifndef HOSD
// OSDMenu

#include <fileio.h>

// Tries to open launcher ELF on both memory cards
int probeLauncher() {
  if (settings.launcherPath[2] == '?') {
    if (settings.mcSlot == 1)
      settings.launcherPath[2] = '1';
    else
      settings.launcherPath[2] = '0';
  }

  int fd = fioOpen(settings.launcherPath, FIO_O_RDONLY);
  if (fd < 0) {
    // If ELF doesn't exist on boot MC, try the other slot
    if (settings.launcherPath[2] == '1')
      settings.launcherPath[2] = '0';
    else
      settings.launcherPath[2] = '1';
    if ((fd = fioOpen(settings.launcherPath, FIO_O_RDONLY)) < 0)
      return -1;
  }
  fioClose(fd);

  return 0;
}

int main(int argc, char *argv[]) {
  // Clear memory
  wipeUserMem();

  // Load needed modules
  initModules();

  // Set FMCB & OSDSYS default settings for configurable items
  initConfig();

  // Determine from which mc slot FMCB was booted
  if (!strncmp(argv[0], "mc0", 3))
    settings.mcSlot = 0;
  else if (!strncmp(argv[0], "mc1", 3))
    settings.mcSlot = 1;

  // Read config file
  loadConfig();

  // Make sure launcher is accessible
  if (probeLauncher())
    Exit(-1);

#ifdef ENABLE_SPLASH
  GSVideoMode vmode = GS_MODE_NTSC; // Use NTSC by default

  // Respect preferred mode
  if (!settings.videoMode) {
    // If mode is not set, read console region from ROM
    if (settings.romver[4] == 'E')
      vmode = GS_MODE_PAL;
  } else if (settings.videoMode == GS_MODE_PAL)
    vmode = GS_MODE_PAL;

  gsDisplaySplash(vmode);
#endif

  int fd = fioOpen("rom0:MBROWS", FIO_O_RDONLY);
  if (fd >= 0) {
    // MBROWS exists only on protokernel systems
    fioClose(fd);
    launchProtokernelOSDSYS();
  } else
    launchOSDSYS();

  Exit(-1);
}
#else
// HOSDMenu

#include <fcntl.h>
#include <fileXio_rpc.h>

// Tries to open launcher ELF on both memory cards
int probeLauncher() {
  int fd = open("pfs0:" HOSD_LAUNCHER_PATH, O_RDONLY);
  if (fd < 0)
    return -1;

  close(fd);

  return 0;
}

int main(int argc, char *argv[]) {
  // Clear memory
  wipeUserMem();

  // Load needed modules
  if (initModules())
    Exit(-1);

  // Set FMCB & OSDSYS default settings for configurable items
  initConfig();

  if (fileXioMount("pfs0:", HOSD_CONF_PARTITION, 0))
    Exit(-1);

  // Read config file
  loadConfig();

  fileXioUmount("pfs0:");

  if (fileXioMount("pfs0:", HOSD_SYS_PARTITION, 0))
    Exit(-1);

  // Make sure launcher is accessible
  if (probeLauncher())
    goto fail;

#ifdef ENABLE_SPLASH
  GSVideoMode vmode = GS_MODE_NTSC; // Use NTSC by default

  // Respect preferred mode
  if (!settings.videoMode) {
    // If mode is not set, read console region from ROM
    if (settings.romver[4] == 'E')
      vmode = GS_MODE_PAL;
  } else if (settings.videoMode == GS_MODE_PAL)
    vmode = GS_MODE_PAL;

  gsDisplaySplash(vmode);
#endif

  launchOSDSYS();

fail:
  fileXioUmount("pfs0:");
  Exit(-1);
}
#endif
