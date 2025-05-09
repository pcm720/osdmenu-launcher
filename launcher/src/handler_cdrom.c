#include "common.h"
#include "defaults.h"
#include "game_id.h"
#include "game_id_table.h"
#include "history.h"
#include "init.h"
#include "loader.h"
#include <ctype.h>
#include <fcntl.h>
#include <kernel.h>
#include <libcdvd.h>
#include <ps2sdkapi.h>
#include <sifrpc.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
  DiscType_PS1,
  DiscType_PS2,
} DiscType;

const char *getPS1GenericTitleID();
int parseDiscCNF(char *bootPath, char *titleID, char *titleVersion);
int startCDROM(int displayGameID, int skipPS2LOGO, char *dkwdrvPath);

#define MAX_STR 256

// Launches the disc while displaying the visual game ID and writing to the history file
int handleCDROM(int argc, char *argv[]) {
  // Parse arguments
  int displayGameID = 1;
  int useDKWDRV = 0;
  int skipPS2LOGO = 0;

  char *arg;
  char *dkwdrvPath = NULL;
  for (int i = 0; i < argc; i++) {
    arg = argv[i];
    if ((arg == NULL) || (arg[0] != '-'))
      continue;
    arg++;

    if (!strcmp("nologo", arg)) {
      skipPS2LOGO = 1;
    } else if (!strcmp("nogameid", arg)) {
      displayGameID = 0;
    } else if (!strncmp("dkwdrv", arg, 6)) {
      useDKWDRV = 1;
      dkwdrvPath = strchr(arg, '=');
      if (dkwdrvPath) {
        dkwdrvPath++;
      }
    }
  }

  if (useDKWDRV && !dkwdrvPath)
    dkwdrvPath = strdup(DKWDRV_PATH);

  return startCDROM(displayGameID, skipPS2LOGO, dkwdrvPath);
}

int startCDROM(int displayGameID, int skipPS2LOGO, char *dkwdrvPath) {
  // Always reset IOP to a known state
  int res = initModules(Device_MemoryCard);
  if (res)
    return res;

  if (!sceCdInit(SCECdINIT)) {
    msg("CDROM ERROR: Failed to initialize libcdvd\n");
    return -ENODEV;
  }

  if (dkwdrvPath)
    DPRINTF("CDROM: Using DKWDRV for PS1 discs\n");
  if (!displayGameID)
    DPRINTF("CDROM: Disabling visual game ID\n");
  if (skipPS2LOGO)
    DPRINTF("CDROM: Skipping PS2LOGO\n");

  // Wait until the drive is ready
  sceCdDiskReady(0);
  int discType = sceCdGetDiskType();

  if ((discType >= SCECdNODISC) && (discType < SCECdUNKNOWN)) {
    msg("\n\nWaiting for disc\n");
    while ((discType >= SCECdNODISC) && (discType < SCECdUNKNOWN)) {
      sleep(1);
      discType = sceCdGetDiskType();
    }
  }

  // Make sure the disc is a valid PS1/PS2 disc
  discType = sceCdGetDiskType();
  if (!(discType >= SCECdPSCD || discType <= SCECdPS2DVD)) {
    msg("CDROM ERROR: Unsupported disc type\n");
    return -EINVAL;
  }

  // Parse SYSTEM.CNF
  char *bootPath = calloc(sizeof(char), MAX_STR);
  char *titleID = calloc(sizeof(char), 12);
  char *titleVersion = calloc(sizeof(char), MAX_STR);
  discType = parseDiscCNF(bootPath, titleID, titleVersion);
  if (discType < 0) {
    msg("CDROM ERROR: Failed to parse SYSTEM.CNF\n");
    free(bootPath);
    free(titleID);
    free(titleVersion);
    return -ENOENT;
  }

  if (titleID[0] != '\0') {
    // Update history file and display game ID
    updateHistoryFile(titleID);
    if (displayGameID)
      gsDisplayGameID(titleID);
  } else
    strcpy(titleID, "???"); // Set placeholder value

  if (titleVersion[0] == '\0')
    // Set placeholder value
    strcpy(titleVersion, "???");

  sceCdInit(SCECdEXIT);

  switch (discType) {
  case DiscType_PS1:
    if (dkwdrvPath) {
      DPRINTF("Starting DKWDRV\n");
      free(bootPath);
      free(titleID);
      free(titleVersion);
      char *argv[] = {dkwdrvPath};
      launchPath(1, argv);
    } else {
      char *argv[] = {titleID, titleVersion};
      DPRINTF("Starting PS1DRV with title ID %s and version %s\n", argv[0], argv[1]);
      sceSifExitCmd();
      LoadExecPS2("rom0:PS1DRV", 2, argv);
    }
    break;
  case DiscType_PS2:
    if (skipPS2LOGO) {
      // Apply IOP emulation flags for Deckard consoles
      // (simply returns on PGIF consoles)
      if (titleID[0] != '\0')
        applyXPARAM(titleID);

      sceSifExitCmd();
      // Launch PS2 game directly
      LoadExecPS2(bootPath, 0, NULL);
    } else {
      sceSifExitCmd();
      // Launch PS2 game with rom0:PS2LOGO
      char *argv[] = {bootPath};
      LoadExecPS2("rom0:PS2LOGO", 1, argv);
    }
    break;
  default:
    msg("CDROM ERROR: unknown disc type\n");
  }

  return -1;
}

// Parses SYSTEM.CNF on disc into bootPath, titleID and titleVersion
// Returns disc type or a negative number if an error occurs
int parseDiscCNF(char *bootPath, char *titleID, char *titleVersion) {
  // Open SYSTEM.CNF
  int fd = open("cdrom0:\\SYSTEM.CNF;1", O_RDONLY);
  if (fd < 0) {
    // Apparently not all PS1 titles have SYSTEM.CNF
    // Try to guess the title ID from the disc PVD
    const char *tID = getPS1GenericTitleID();
    if (tID) {
      DPRINTF("Guessed the title ID from disc PVD: %s\n", tID);
      strncpy(titleID, tID, 11);
      return DiscType_PS1;
    }
    return -ENOENT;
  }

  // Get the file size
  int size = lseek(fd, 0, SEEK_END);
  if (size <= 0) {
    msg("CDROM ERROR: Bad SYSTEM.CNF size\n");
    close(fd);
    return -EIO;
  }
  lseek(fd, 0, SEEK_SET);

  // Read file into memory
  char *cnf = malloc(size * sizeof(char));
  if (read(fd, cnf, size) != size) {
    msg("CDROM ERROR: Failed to read SYSTEM.CNF\n");
    close(fd);
    free(cnf);
    return -EIO;
  }
  close(fd);

  // Open memory buffer as stream
  FILE *file = fmemopen(cnf, size, "r");
  if (file == NULL) {
    msg("CDROM ERROR: Failed to open SYSTEM.CNF for reading\n");
    free(cnf);
    return -ENOENT;
  }

  char lineBuffer[255] = {0};
  char *valuePtr = NULL;
  DiscType type = -1;
  while (fgets(lineBuffer, sizeof(lineBuffer), file)) { // fgets returns NULL if EOF or an error occurs
    // Find the start of the value
    valuePtr = strchr(lineBuffer, '=');
    if (!valuePtr)
      continue;

    // Trim whitespace and terminate the value
    do {
      valuePtr++;
    } while (isspace((int)*valuePtr));
    valuePtr[strcspn(valuePtr, "\r\n")] = '\0';

    if (!strncmp(lineBuffer, "BOOT2", 5)) { // PS2 title
      type = DiscType_PS2;
      strncpy(bootPath, valuePtr, MAX_STR);
      continue;
    }
    if (!strncmp(lineBuffer, "BOOT", 4)) { // PS1 title
      type = DiscType_PS1;
      strncpy(bootPath, valuePtr, MAX_STR);
      continue;
    }
    if (!strncmp(lineBuffer, "VER", 3)) { // Title version
      strncpy(titleVersion, valuePtr, MAX_STR);
      continue;
    }
  }
  fclose(file);
  free(cnf);

  // Get the start of the executable path
  valuePtr = strchr(bootPath, '\\');
  if (!valuePtr) {
    valuePtr = strchr(bootPath, ':'); // PS1 CDs don't have \ in the path
    if (!valuePtr) {
      DPRINTF("CDROM: Failed to parse the executable for the title ID\n");
      return type;
    }
  }
  valuePtr++;

  // Do a basic sanity check.
  // Some PS1 titles might have an off-by-one error in executable name (SLPS_11.111 instead of SLPS_111.11)
  if ((strlen(valuePtr) > 11) && (valuePtr[4] == '_') && ((valuePtr[7] == '.') || (valuePtr[8] == '.')))
    strncpy(titleID, valuePtr, 11);
  else {
    // Try to guess the title ID from the disc PVD
    const char *tID = getPS1GenericTitleID();
    if (tID) {
      DPRINTF("Guessed the title ID from disc PVD: %s\n", tID);
      strncpy(titleID, tID, 11);
    }
  }

  return type;
}

// Attempts to guess PS1 title ID from volume creation date stored in PVD
const char *getPS1GenericTitleID() {
  char sectorData[2048] = {0};

  sceCdRMode mode = {
      .trycount = 3,
      .spindlctrl = SCECdSpinNom,
      .datapattern = SCECdSecS2048,
  };

  // Read sector 16 (Primary Volume Descriptor)
  if (!sceCdRead(16, 1, &sectorData, &mode) || sceCdSync(0)) {
    DPRINTF("Failed to read PVD\n");
    return NULL;
  }

  // Make sure the PVD is valid
  if (strncmp(&sectorData[1], "CD001", 5)) {
    DPRINTF("Invalid PVD\n");
    return NULL;
  }

  // Try to match the volume creation date at offset 0x32D against the table
  for (size_t i = 0; i < sizeof(gameIDTable) / sizeof(gameIDTable[0]); ++i) {
    if (strncmp(&sectorData[0x32D], gameIDTable[i].volumeTimestamp, 16) == 0) {
      return gameIDTable[i].gameID;
    }
  }
  return NULL;
}
