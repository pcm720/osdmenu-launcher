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

// Defined in common/defaults.h
char cnfPath[] = CONF_PATH;

typedef struct linkedStr {
  char *str;
  struct linkedStr *next;
} linkedStr;

// Adds a new string to linkedStr and returns
linkedStr *addStr(linkedStr *lstr, char *str) {
  linkedStr *newLstr = malloc(sizeof(linkedStr));
  newLstr->str = strdup(str);
  newLstr->next = NULL;

  if (lstr) {
    linkedStr *tLstr = lstr;
    // If lstr is not null, go to the last element and
    // link the new element
    while (tLstr->next)
      tLstr = tLstr->next;

    tLstr->next = newLstr;
    return lstr;
  }

  // Else, return the new element as the first one
  return newLstr;
}

// Frees all elements of linkedStr
void freeLinkedStr(linkedStr *lstr) {
  if (!lstr)
    return;

  linkedStr *tPtr = lstr;
  while (lstr->next) {
    free(lstr->str);
    tPtr = lstr->next;
    free(lstr);
    lstr = tPtr;
  }
  free(lstr->str);
  free(lstr);
}

// Loads ELF specified in OSDMENU.CNF on the memory card
int handleFMCB(int argc, char *argv[]) {
  int res = initModules(Device_MemoryCard);
  if (res)
    return res;

  if (cnfPath[2] == '?')
    cnfPath[2] = '0';

  // Get memory card slot from argv[0] (fmcb0/1)
  if (!strncmp("mc0", cnfPath, 3) && (argv[0][4] == '1')) {
    // If path is fmcb1:, try to get config from mc1 first
    cnfPath[2] = '1';
    if (tryFile(cnfPath)) // If file is not found, revert to mc0
      cnfPath[2] = '0';
  }

  char *idx = strchr(argv[0], ':');
  if (!idx) {
    msg("FMCB: Argument '%s' doesn't contain entry index\n", argv[0]);
    return -EINVAL;
  }
  int targetIdx = atoi(++idx);
  printf("Searching for FMCB entry %d\n", targetIdx);

  // Open the config file
  FILE *file = fopen(cnfPath, "r");
  if (!file) {
    msg("FMCB: Failed to open %s\n", cnfPath);
    return -ENOENT;
  }

  // CDROM arguments
  int displayGameID = 1;
  int skipPS2LOGO = 0;
  int useDKWDRV = 0;
  char *dkwdrvPath = NULL;

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
    printf("Trying to launch %s\n", targetArgv[0]);
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
