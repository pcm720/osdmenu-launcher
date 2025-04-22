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
#include <fileio.h>

// Reduce binary size by disabling the unneeded functionality
void _libcglue_init() {}
void _libcglue_deinit() {}
void _libcglue_args_parse(int argc, char **argv) {}
DISABLE_PATCHED_FUNCTIONS();
DISABLE_EXTRA_TIMERS_FUNCTIONS();
PS2_DISABLE_AUTOSTART_PTHREAD();

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

  // Set FMCB & OSDSYS default settings for configureable items
  initConfig();

  // Determine from which mc slot FMCB was booted
  if (!strncmp(argv[0], "mc0", 3))
    settings.mcSlot = 0;
  else if (!strncmp(argv[0], "mc1", 3))
    settings.mcSlot = 1;

  // Read config before to check args for an elf to load
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

  launchOSDSYS();

  Exit(-1);
}
