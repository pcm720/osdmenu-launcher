#include "common.h"
#include "init.h"
#include "loader.h"
#include <fcntl.h>
#include <ps2sdkapi.h>
#include <stdio.h>
#include <string.h>

char bdmMountpoint[] = BDM_MOUNTPOINT;
#define BDM_MAX_DEVICES 10

// Launches ELF from BDM device
int handleBDM(DeviceType device, int argc, char *argv[]) {
  if ((argv[0] == 0) || (strlen(argv[0]) < 5)) {
    msg("BDM: invalid argument\n");
    return -EINVAL;
  }

  // Build ELF path
  char *elfPath = normalizePath(argv[0], device);
  if (!elfPath)
    return -ENODEV;

  // Initialize device modules
  int res = initModules(device);
  if (res)
    return res;

  // Try all BDM devices while decreasing the number of wait
  // attempts for each consecutive device to reduce init times
  int delayAttempts = DELAY_ATTEMPTS; // Max number of attempts
  for (int i = 0; i < BDM_MAX_DEVICES; i++) {
    // Build mountpoint path
    bdmMountpoint[4] = i + '0';
    elfPath[4] = i + '0';
    while (delayAttempts != 0) {
      // Try to open the mountpoint to make sure the device exists
      res = open(bdmMountpoint, O_DIRECTORY | O_RDONLY);
      if (res >= 0) {
        // If mountpoint is available
        close(res);

        // Jump to launch if file exists
        if (!(res = tryFile(elfPath)))
          goto found;
        // Else, try next device
        break;
      }
      // If the mountpoitnt is unavailable, delay and decrement the number of wait attempts
      sleep(1);
      delayAttempts--;
    }
    // No more mountpoints available
    if (res < 0)
      break;
  }
  return -ENODEV;

found:
  argv[0] = elfPath;
  return LoadELFFromFile(argc, argv);
}
