#include "common.h"
#include "defaults.h"
#include "handlers.h"
#include <ctype.h>
#include <init.h>
#include <kernel.h>
#include <ps2sdkapi.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NEWLIB_PORT_AWARE
#include <fileXio_rpc.h>
#include <hdd-ioctl.h>
#include <io_common.h>

#define DELAY_ATTEMPTS 20

// Defined in common/defaults.h
char cnfPath[sizeof(CONF_PATH) + 6] = {0};

// Loads ELF specified in OSDMENU.CNF on the memory card
int handleFMCB(int argc, char *argv[]) {
  int isHDD = 1;
  if (!strncmp(argv[0], "pfs", 3))
    isHDD = 1;

  if (!isHDD) {
    // Handle OSDMenu launch
    int res = initModules(Device_MemoryCard);
    if (res)
      return res;

    // Build path to OSDMENU.CNF
    strcat(cnfPath, "mc0:");
    strcat(cnfPath, CONF_PATH);

    // Get memory card slot from argv[1] (fmcb0/1)
    if (argv[1][4] == '1') {
      // If path is fmcb1:, try to get config from mc1 first
      cnfPath[2] = '1';
      if (tryFile(cnfPath)) // If file is not found, revert to mc0
        cnfPath[2] = '0';
    }
  } else {
    // Handle HOSDMenu launch
    int res = initModules(Device_PFS);
    if (res)
      return res;

    // Wait for IOP to initialize device driver
    DPRINTF("Waiting for HDD to become available\n");
    for (int attempts = 0; attempts < DELAY_ATTEMPTS; attempts++) {
      res = open("hdd0:", O_DIRECTORY | O_RDONLY);
      if (res >= 0) {
        close(res);
        break;
      }
      sleep(1);
    }
    if (res < 0)
      return -ENODEV;

    // Mount the partition
    DPRINTF("Mounting %s to %s\n", HOSD_CONF_PARTITION, PFS_MOUNTPOINT);
    if (fileXioMount(PFS_MOUNTPOINT, HOSD_CONF_PARTITION, FIO_MT_RDONLY))
      return -ENODEV;

    // Build path to OSDMENU.CNF
    strcat(cnfPath, PFS_MOUNTPOINT);
    strcat(cnfPath, CONF_PATH);
  }

  char *idx = strchr(argv[1], ':');
  if (!idx) {
    msg("FMCB: Argument '%s' doesn't contain entry index\n", argv[0]);
    if (isHDD) {
      // Unmount the partition
      fileXioDevctl(PFS_MOUNTPOINT, PDIOC_CLOSEALL, NULL, 0, NULL, 0);
      fileXioSync(PFS_MOUNTPOINT, FXIO_WAIT);
      fileXioUmount(PFS_MOUNTPOINT);
    }
    return -EINVAL;
  }
  int targetIdx = atoi(++idx);

  // Open the config file
  FILE *file = fopen(cnfPath, "r");
  if (!file) {
    msg("FMCB: Failed to open %s\n", cnfPath);
    if (isHDD) {
      // Unmount the partition
      fileXioDevctl(PFS_MOUNTPOINT, PDIOC_CLOSEALL, NULL, 0, NULL, 0);
      fileXioSync(PFS_MOUNTPOINT, FXIO_WAIT);
      fileXioUmount(PFS_MOUNTPOINT);
    }
    return -ENOENT;
  }

  // CDROM arguments
  int displayGameID = 1;
  int skipPS2LOGO = 0;
  int useDKWDRV = 0;
  char *dkwdrvPath = NULL;

  if (isHDD) {
    // Set DKWDRV path
    dkwdrvPath = HOSD_DKWDRV_PATH;
  }

  // Temporary path and argument lists
  linkedStr *targetPaths = NULL;
  linkedStr *targetArgs = NULL;
  int targetArgc = 1; // argv[0] is the ELF path

  char lineBuffer[PATH_MAX] = {0};
  char *valuePtr = NULL;
  char *idxPtr = NULL;
  while (fgets(lineBuffer, sizeof(lineBuffer), file)) { // fgets returns NULL if EOF or an error occurs
    // Find the start of the value
    valuePtr = strchr(lineBuffer, '=');
    if (!valuePtr)
      continue;
    *valuePtr = '\0';

    // Trim whitespace and terminate the value
    do {
      valuePtr++;
    } while (isspace((int)*valuePtr));
    valuePtr[strcspn(valuePtr, "\r\n")] = '\0';

    if (!strncmp(lineBuffer, "path", 4)) {
      // Get the pointer to path?_OSDSYS_ITEM_
      idxPtr = strrchr(lineBuffer, '_');
      if (!idxPtr)
        continue;

      if (atoi(++idxPtr) != targetIdx)
        continue;

      if ((strlen(valuePtr) > 0)) {
        targetPaths = addStr(targetPaths, valuePtr);
      }
      continue;
    }
    if (!strncmp(lineBuffer, "arg", 3)) {
      // Get the pointer to arg?_OSDSYS_ITEM_
      idxPtr = strrchr(lineBuffer, '_');
      if (!idxPtr)
        continue;

      if (atoi(++idxPtr) != targetIdx)
        continue;

      if ((strlen(valuePtr) > 0)) {
        targetArgs = addStr(targetArgs, valuePtr);
        targetArgc++;
      }
      continue;
    }
    if (!strncmp(lineBuffer, "cdrom_skip_ps2logo", 18)) {
      skipPS2LOGO = atoi(valuePtr);
      continue;
    }
    if (!strncmp(lineBuffer, "cdrom_disable_gameid", 20)) {
      if (atoi(valuePtr))
        displayGameID = 0;
      continue;
    }
    if (!strncmp(lineBuffer, "cdrom_use_dkwdrv", 16)) {
      useDKWDRV = 1;
      continue;
    }
    if (!strncmp(lineBuffer, "path_DKWDRV_ELF", 15)) {
      dkwdrvPath = strdup(valuePtr);
      continue;
    }
  }
  fclose(file);

  if (isHDD) {
    // Unmount the partition
    fileXioDevctl(PFS_MOUNTPOINT, PDIOC_CLOSEALL, NULL, 0, NULL, 0);
    fileXioSync(PFS_MOUNTPOINT, FXIO_WAIT);
    fileXioUmount(PFS_MOUNTPOINT);
  }

  if (!targetPaths) {
    msg("FMCB: No paths found for entry %d\n", targetIdx);
    freeLinkedStr(targetPaths);
    freeLinkedStr(targetArgs);
    if (dkwdrvPath)
      free(dkwdrvPath);
    return -EINVAL;
  }

  // Handle 'OSDSYS' entry
  if (!strcmp(targetPaths->str, "OSDSYS")) {
    freeLinkedStr(targetPaths);
    freeLinkedStr(targetArgs);
    if (dkwdrvPath)
      free(dkwdrvPath);
    rebootPS2();
  }

  // Handle 'POWEROFF' entry
  if (!strcmp(targetPaths->str, "POWEROFF")) {
    freeLinkedStr(targetPaths);
    freeLinkedStr(targetArgs);
    if (dkwdrvPath)
      free(dkwdrvPath);
    shutdownPS2();
  }

  // Handle 'cdrom' entry
  if (!strcmp(targetPaths->str, "cdrom")) {
    freeLinkedStr(targetPaths);
    freeLinkedStr(targetArgs);
    if (!useDKWDRV && dkwdrvPath) {
      free(dkwdrvPath);
      dkwdrvPath = NULL;
    }
    return startCDROM(displayGameID, skipPS2LOGO, dkwdrvPath);
  }

  if (dkwdrvPath)
    free(dkwdrvPath);

  // Build argv, freeing targetArgs
  char **targetArgv = malloc(targetArgc * sizeof(char *));
  linkedStr *tlstr;
  if (targetArgs) {
    tlstr = targetArgs;
    for (int i = 1; i < targetArgc; i++) {
      targetArgv[i] = tlstr->str;
      tlstr = tlstr->next;
      free(targetArgs);
      targetArgs = tlstr;
    }
    free(targetArgs);
  }

  // Try every path
  tlstr = targetPaths;
  while (tlstr) {
    targetArgv[0] = tlstr->str;
    // If target path is valid, it'll never return from launchPath
    DPRINTF("Trying to launch %s\n", targetArgv[0]);
    launchPath(targetArgc, targetArgv);
    free(tlstr->str);
    tlstr = tlstr->next;
    free(targetPaths);
    targetPaths = tlstr;
  }
  free(targetPaths);

  msg("FMCB: All paths have been tried\n");
  return -ENODEV;
}
