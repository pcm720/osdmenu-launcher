#include "gs.h"
#include "init.h"
#include "patches_common.h"
#include "settings.h"
#include "splash.h"
#include <fcntl.h>
#include <kernel.h>
#include <ps2sdkapi.h>
#include <stdlib.h>
#include <string.h>
#define NEWLIB_PORT_AWARE
#include <fileXio_rpc.h>

// Reduce binary size by disabling the unneeded functionality
void _libcglue_init() {}
void _libcglue_deinit() {}
void _libcglue_args_parse(int argc, char **argv) {}
DISABLE_PATCHED_FUNCTIONS();
DISABLE_EXTRA_TIMERS_FUNCTIONS();
PS2_DISABLE_AUTOSTART_PTHREAD();

// Tries to open launcher ELF on both memory cards
int probeLauncher() {
  int fd = open(settings.launcherPath, O_RDONLY);
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
    __builtin_trap();

  if (fileXioMount("pfs1:", "hdd0:__sysconf", 0))
    __builtin_trap();

  // Set FMCB & OSDSYS default settings for configureable items
  initConfig();

  // Read config before to check args for an elf to load
  loadConfig();

  // Make sure launcher is accessible
  if (probeLauncher())
    Exit(-1);

  if (fileXioUmount("pfs1:"))
    __builtin_trap();

  if (fileXioMount("pfs0:", "hdd0:__system", 0))
    __builtin_trap();

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
