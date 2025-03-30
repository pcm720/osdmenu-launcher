#include "settings.h"
#include "defaults.h"
#include <stdlib.h>
#include <string.h>
#define NEWLIB_PORT_AWARE
#include <fileio.h>

// If defined, ensures that CNF content loads at 0x01f00000 leaving sufficient space for big CNF files
// If we don't do this, memory will be allocated just after the loader, something like 0x000ecb00
// leading to leave unsufficient space for big cnf files since OSDSYS needs to load at 0x00100000
#define DUMMY_MALLOC

char *pCNF = NULL;
PatcherSettings settings;

// Defined in common/defaults.h
char cnfPath[] = CONF_PATH;
char launcherPath[] = LAUNCHER_PATH;

// getCNFString is the main CNF parser called for each CNF variable in a CNF file.
// Input and output data is handled via its pointer parameters.
// The return value flags 'false' when no variable is found. (normal at EOF)
int getCNFString(char **cnfPos, char **name, char **value) {
  char *pName, *pValue, *pToken = *cnfPos;

nextLine:
  while ((*pToken <= ' ') && (*pToken > '\0'))
    pToken += 1; // Skip leading whitespace, if any
  if (*pToken == '\0')
    return 0; // Exit at EOF

  pName = pToken;      // Current pos is potential name
  if (*pToken < 'A') { // If line is a comment line
    while ((*pToken != '\r') && (*pToken != '\n') && (*pToken > '\0'))
      pToken += 1; // Seek line end
    goto nextLine; // Go back to try next line
  }

  while ((*pToken >= 'A') || ((*pToken >= '0') && (*pToken <= '9')))
    pToken += 1; // Seek name end
  if (*pToken == '\0')
    return 0; // Exit at EOF

  while ((*pToken <= ' ') && (*pToken > '\0'))
    *pToken++ = '\0'; // Zero and skip post-name whitespace
  if (*pToken != '=')
    return 0;       // Exit (syntax error) if '=' missing
  *pToken++ = '\0'; // Zero '=' (possibly terminating name)

  while ((*pToken <= ' ') && (*pToken > '\0')      // Skip pre-value whitespace, if any
         && (*pToken != '\r') && (*pToken != '\n') // but do not pass the end of the line
         && (*pToken != '\7')                      // allow ctrl-G (BEL) in value
  )
    pToken += 1;
  if (*pToken == '\0')
    return 0;      // Exit at EOF
  pValue = pToken; // Current pos is potential value

  while ((*pToken != '\r') && (*pToken != '\n') && (*pToken != '\0'))
    pToken += 1; // Seek line end
  if (*pToken != '\0')
    *pToken++ = '\0'; // Terminate value (passing if not EOF)
  while ((*pToken <= ' ') && (*pToken > '\0'))
    pToken += 1; // Skip following whitespace, if any

  *cnfPos = pToken; // Set new CNF file position
  *name = pName;    // Set found variable name
  *value = pValue;  // Set found variable value
  return 1;
}

// Loads config file from the memory card
int loadConfig(void) {
  if (settings.mcSlot == 1)
    cnfPath[2] = '1';
  else
    cnfPath[2] = '0';

  int fd = fioOpen(cnfPath, FIO_O_RDONLY);
  if (fd < 0) {
    // If CNF doesn't exist on boot MC, try the other slot
    if (settings.mcSlot == 1)
      cnfPath[2] = '0';
    else
      cnfPath[2] = '1';
    if ((fd = fioOpen(cnfPath, FIO_O_RDONLY)) < 0)
      return -1;
  }

  // Change mcSlot to point to the memory card contaning the config file
  settings.mcSlot = cnfPath[2] - '0';

  size_t cnfSize = fioLseek(fd, 0, FIO_SEEK_END);
  fioLseek(fd, 0, FIO_SEEK_SET);

#ifndef DUMMY_MALLOC
  pCNF = (char *)malloc(cnfSize);
#else
  char *dummyPtr = NULL;
  dummyPtr = (char *)malloc(32);
  uint32_t dummySZ = 0x01f00000 - (uint32_t)(dummyPtr)-16;
  free(dummyPtr);
  dummyPtr = (char *)malloc(dummySZ);
  pCNF = (char *)malloc(cnfSize);
  // printf("dummySZ: %08x\n", (uint32_t)dummySZ);
  // printf("dummyPtr: %08x\n", (uint32_t)dummyPtr);
  // printf("pCNF: %08lx\n", (uint32_t)pCNF);

  if (dummyPtr != NULL)
    free(dummyPtr);
#endif

  char *cnfPos = pCNF;
  if (cnfPos == NULL) {
    fioClose(fd);
    return -1;
  }

  fioRead(fd, cnfPos, cnfSize); // Read CNF as one long string
  fioClose(fd);
  cnfPos[cnfSize] = '\0'; // Terminate the CNF string

  char *name, *value;
  char valueBuf[4];
  int i, j;
  while (getCNFString(&cnfPos, &name, &value)) {
    if (!strcmp(name, "OSDSYS_video_mode")) {
      settings.videoMode = value;
      continue;
    }
    if (!strcmp(name, "hacked_OSDSYS")) {
      settings.hackedOSDSYS = atoi(value);
      continue;
    }
    if (!strcmp(name, "OSDSYS_scroll_menu")) {
      settings.scrollMenu = atoi(value);
      continue;
    }
    if (!strcmp(name, "OSDSYS_menu_x")) {
      settings.menuX = atoi(value);
      continue;
    }
    if (!strcmp(name, "OSDSYS_menu_y")) {
      settings.menuY = atoi(value);
      continue;
    }
    if (!strcmp(name, "OSDSYS_enter_x")) {
      settings.enterX = atoi(value);
      continue;
    }
    if (!strcmp(name, "OSDSYS_enter_y")) {
      settings.enterY = atoi(value);
      continue;
    }
    if (!strcmp(name, "OSDSYS_version_x")) {
      settings.versionX = atoi(value);
      continue;
    }
    if (!strcmp(name, "OSDSYS_version_y")) {
      settings.versionY = atoi(value);
      continue;
    }
    if (!strcmp(name, "OSDSYS_cursor_max_velocity")) {
      settings.cursorMaxVelocity = atoi(value);
      continue;
    }
    if (!strcmp(name, "OSDSYS_cursor_acceleration")) {
      settings.cursorAcceleration = atoi(value);
      continue;
    }
    if (!strcmp(name, "OSDSYS_left_cursor")) {
      settings.leftCursor = value;
      continue;
    }
    if (!strcmp(name, "OSDSYS_right_cursor")) {
      settings.rightCursor = value;
      continue;
    }
    if (!strcmp(name, "OSDSYS_menu_top_delimiter")) {
      settings.menuDelimiterTop = value;
      continue;
    }
    if (!strcmp(name, "OSDSYS_menu_bottom_delimiter")) {
      settings.menuDelimiterBottom = value;
      continue;
    }
    if (!strcmp(name, "OSDSYS_num_displayed_items")) {
      settings.displayedItems = atoi(value);
      continue;
    }
    if (!strcmp(name, "OSDSYS_Skip_Disc")) {
      settings.skipDisc = atoi(value);
      continue;
    }
    if (!strcmp(name, "OSDSYS_Skip_Logo")) {
      settings.skipLogo = atoi(value);
      continue;
    }
    if (!strcmp(name, "OSDSYS_Inner_Browser")) {
      settings.goToInnerBrowser = atoi(value);
      continue;
    }
    if (!strcmp(name, "OSDSYS_selected_color")) {
      for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
          valueBuf[j] = value[j];
        }
        settings.colorSelected[i] = strtol(valueBuf, NULL, 16);
        value += 5;
      }
      continue;
    }
    if (!strcmp(name, "OSDSYS_unselected_color")) {
      for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
          valueBuf[j] = value[j];
        }
        settings.colorUnselected[i] = strtol(valueBuf, NULL, 16);
        value += 5;
      }
      continue;
    }
    if (!strncmp(name, "name_OSDSYS_ITEM_", 17)) {
      j = atoi(&name[17]);
      if ((strlen(value) > 0)) {
        // Ignore empty values
        settings.menuItemName[settings.menuItemCount] = value;
        settings.menuItemIdx[settings.menuItemCount] = j;
        settings.menuItemCount++;
      }
      continue;
    }
    if (!strcmp(name, "path_LAUNCHER_ELF")) {
      if (strlen(value) < 4 || strncmp(value, "mc", 2))
        continue; // Accept only memory card paths

      settings.launcherPath = value;
      continue;
    }
    if (!strcmp(name, "path_DKWDRV_ELF")) {
      if (strlen(value) < 4 || strncmp(value, "mc", 2))
        continue; // Accept only memory card paths

      settings.dkwdrvPath = value;
      continue;
    }
    if (!strcmp(name, "cdrom_skip_ps2logo")) {
      settings.skipPS2LOGO = atoi(value);
      continue;
    }
    if (!strcmp(name, "cdrom_disable_gameid")) {
      settings.disableGameID = atoi(value);
      continue;
    }
    if (!strcmp(name, "cdrom_use_dkwdrv")) {
      settings.useDKWDRV = atoi(value);
      continue;
    }
  }
  return 0;
}

// Initializes static variables
void initVariables() {
  // Init ROMVER
  int fdn = 0;
  if ((fdn = fioOpen("rom0:ROMVER", FIO_O_RDONLY)) > 0) {
    fioRead(fdn, settings.romver, 14);
    settings.romver[14] = '\0';
    fioClose(fdn);
  }
}

// Loads defaults
void initConfig(void) {
  settings.mcSlot = 0;
  settings.hackedOSDSYS = 0;
  settings.skipDisc = 0;
  settings.skipLogo = 1;
  settings.goToInnerBrowser = 0;
  settings.scrollMenu = 1;
  settings.videoMode = "AUTO";
  settings.menuX = 320;
  settings.menuY = 110;
  settings.enterX = 30;
  settings.enterY = -1;
  settings.versionX = -1;
  settings.versionY = -1;
  settings.cursorMaxVelocity = 1000;
  settings.cursorAcceleration = 100;
  settings.leftCursor = ">>";
  settings.rightCursor = "<<";
  settings.menuDelimiterTop = "------=/\\=------";
  settings.menuDelimiterBottom = "------=\\/=------";
  settings.colorSelected[0] = 0x10;
  settings.colorSelected[1] = 0x80;
  settings.colorSelected[2] = 0xe0;
  settings.colorSelected[3] = 0x80;
  settings.colorUnselected[0] = 0x33;
  settings.colorUnselected[1] = 0x33;
  settings.colorUnselected[2] = 0x33;
  settings.colorUnselected[3] = 0x80;
  settings.displayedItems = 7;
  for (int i = 0; i < NEWITEMS; i++) {
    settings.menuItemName[i] = NULL;
    settings.menuItemIdx[i] = 0;
  }
  settings.menuItemCount = 0;
  settings.launcherPath = launcherPath;
  settings.dkwdrvPath = NULL; // Can be null
  settings.skipPS2LOGO = 1;
  settings.disableGameID = 0;
  settings.useDKWDRV = 0;

  settings.romver[0] = '\0';
  initVariables();
}
