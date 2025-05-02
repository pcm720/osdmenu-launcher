#include "common.h"
#include "handlers.h"
#include "loader.h"
#include <fcntl.h>
#include <kernel.h>
#include <ps2sdkapi.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

// Reduce binary size by disabling unneeded functionality
void _libcglue_timezone_update() {}
void _libcglue_rtc_update() {}
PS2_DISABLE_AUTOSTART_PTHREAD();

int main(int argc, char *argv[]) {
  if (argc < 2)
    // Try to quickboot with paths from .CNF located at the current working directory
    fail("Quickboot failed: %d", handleQuickboot(argv[0]));

#ifdef FMCB
  if (!strncmp("fmcb", argv[1], 4))
    fail("Failed to launch %s: %d", argv[1], handleFMCB(argc, argv));
#endif

  // Remove launcher path from arguments
  argc--;
  argv = &argv[1];
  if (!argv[0])
    fail("Invalid argv[0]");

  char *p = strrchr(argv[0], '.');
  if (p && (!strcmp(p, ".cfg") || !strcmp(p, ".CFG") || !strcmp(p, ".CNF") || !strcmp(p, ".cnf")))
    // If argv[1] is a CNF/CFG file, try to load it
    fail("Quickboot failed: %d", handleQuickboot(argv[0]));

  fail("Failed to launch %s: %d", argv[0], launchPath(argc, argv));
}
