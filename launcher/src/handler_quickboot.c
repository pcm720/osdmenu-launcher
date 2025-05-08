#include "common.h"
#include <ctype.h>
#include <init.h>
#include <ps2sdkapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int handleQuickboot(char *cnfPath) {
  int isHDD = 0;
  if (!strncmp(cnfPath, "hdd", 3))
    isHDD = 1;

  int res;
  char *ext = strrchr(cnfPath, '.');
  if (!ext)
    return -ENOENT;

  if (!strcmp(ext, ".ELF") || !strcmp(ext, ".elf")) {
    // Replace .ELF extension with .CNF
    ext[1] = 'C';
    ext[2] = 'N';
    ext[3] = 'F';
    ext[4] = '\0';
  }

  DeviceType dtype;
  if (isHDD) {
    dtype = Device_PFS;
    if ((res = initPFS(cnfPath)))
      return res;
  } else {
    dtype = guessDeviceType(cnfPath);
    if (dtype == Device_None)
      return -ENODEV;

    // Always reset IOP to a known state
    if ((res = initModules(dtype)))
      return res;
  }

  // Open the config file
  char *launchTarget = cnfPath;
  cnfPath = normalizePath(cnfPath, dtype);
  if (!cnfPath) {
    if (isHDD)
      deinitPFS();
    return -ENOENT;
  }

  DPRINTF("Opening %s\n", cnfPath);
  FILE *file = fopen(cnfPath, "r");
  if (!file) {
    msg("Quickboot: Failed to open %s\n", cnfPath);
    if (isHDD)
      deinitPFS();
    return -ENODEV;
  }

  // Temporary path and argument lists
  linkedStr *targetPaths = NULL;
  linkedStr *targetArgs = NULL;
  int targetArgc = 1; // argv[0] is the ELF path

  char lineBuffer[PATH_MAX] = {0};
  char relpathBuffer[PATH_MAX] = {0};
  char *valuePtr = NULL;

  ext = strrchr(launchTarget, '/');
  if (ext)
    *ext = '\0';

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

    if (!strncmp(lineBuffer, "boot", 4) && ext) {
      if (strlen(valuePtr) > 0) {
        // Assemble full path
        snprintf(relpathBuffer, PATH_MAX - 1, "%s/%s", launchTarget, valuePtr);
        targetPaths = addStr(targetPaths, relpathBuffer);
      }
      continue;
    }
    if (!strncmp(lineBuffer, "path", 4)) {
      if ((strlen(valuePtr) > 0))
        targetPaths = addStr(targetPaths, valuePtr);
      continue;
    }
    if (!strncmp(lineBuffer, "arg", 3)) {
      if ((strlen(valuePtr) > 0)) {
        targetArgs = addStr(targetArgs, valuePtr);
        targetArgc++;
      }
      continue;
    }
  }
  fclose(file);
  if (isHDD)
    deinitPFS();

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
    DPRINTF("Attempting to launch %s\n", tlstr->str);
    // If target path is valid, it'll never return from launchPath
    launchPath(targetArgc, targetArgv);
    free(tlstr->str);
    tlstr = tlstr->next;
    free(targetPaths);
    targetPaths = tlstr;
  }
  free(targetPaths);

  msg("Quickboot: all paths have been tried\n");
  return -ENODEV;
}
