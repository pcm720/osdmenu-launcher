
#include "loader.h"
#include "patches_common.h"
#include "patterns_osdmenu.h"
#include "settings.h"
#include <debug.h>
#include <gs.h>
#include <kernel.h>
#include <stdint.h>
#include <string.h>

typedef struct {
  uint16_t sceGsInterMode; // Interlace/non-interlace value
  uint16_t sceGsOutMode;   // NTSC/PAL value
  uint16_t sceGsFFMode;    // FIELD/FRAME value
  uint16_t sceGsVersion;   // GS version
} sceGsGParam;

// Returns a pointer to sceGsGParam
// Can't use PS2SDK libgs function because these parameters
// are set by OSDSYS at an unknown address when setting the video mode
static sceGsGParam *(*sceGsGetGParam)(void) = NULL;

//
// Version info menu patch
//

// Static variables
static char romverValue[] = "\ar0.80VVVVRTYYYYMMDD\ar0.00";
static char mechaconRev[] = "0.00 (Debug)";
static char eeRevision[5] = {0};
static char gsRevision[5] = {0};

static uint16_t *(*sceCdApplySCmd)(uint16_t cmdNum, const void *inBuff, uint16_t inBuffSize, void *outBuff) = NULL;

// Initializes version info menu strings
static void (*versionInfoInit)(void);
uint32_t verinfoStringTableAddr = 0;

typedef struct {
  char *name;
  char *value;               // Used for static values
  char *(*valueFunc)();      // Used for dynamic values
  char *(*valueFuncProto)(); // Used for dynamic values on Protokernels
} customVersionEntry;

char *getVideoMode();
char *getGSRevision();
char *getMechaConRevision();
char *getPatchVersionProto() { return GIT_VERSION; }
char *getPatchVersion() { return "\ar0.80" GIT_VERSION "\ar0.00"; }

// Table for custom menu entries
// Supports dynamic variables that will be updated every time the version menu opens
customVersionEntry entries[] = {
    {"Video Mode", NULL, getVideoMode, getVideoMode},               //
    {"OSDMenu Patch", NULL, getPatchVersion, getPatchVersionProto}, //
    {"ROM", romverValue, NULL},                                     //
    {"Emotion Engine", eeRevision, NULL},                           //
    {"Graphics Synthesizer", NULL, getGSRevision, getGSRevision},   //
    {"MechaCon", NULL, getMechaConRevision, getMechaConRevision},   //
};

// This function will be called every time the version menu opens
void versionInfoInitHandler() {
  // Extend the string table used by the version menu drawing function.
  // It picks up the entries automatically and stops once it gets a NULL pointer (0)
  //
  // Each table entry is represented by three words:
  // Word 0 — pointer to entry name string
  // Word 1 — pointer to entry value string
  // Word 2 — points to some sort of submenu index, 00 is Diagnosis submenu
  //
  // Can be 0 if entry doesn't have a submenu.

  // Find the first empty entry or an entry with the name located in the patcher address space (<0x100000)
  uint32_t ptr = verinfoStringTableAddr;
  while (1) {
    if ((_lw(ptr) < 0x100000) || (!_lw(ptr) && !_lw(ptr + 4) && !_lw(ptr + 8)))
      break;

    ptr += 12;
  }
  if (ptr == verinfoStringTableAddr) {
    return;
  }

  // Add custom entries
  char *value = NULL;
  for (int i = 0; i < sizeof(entries) / sizeof(customVersionEntry); i++) {
    value = NULL;
    if (entries[i].valueFunc)
      value = entries[i].valueFunc();
    else if (entries[i].value)
      value = entries[i].value;

    if (!value)
      continue;

    _sw((uint32_t)entries[i].name, ptr);
    _sw((uint32_t)value, ptr + 4);
    _sw(0xFF, ptr + 8); // Set to fixed index to avoid entries showing up as "Diagnosis"
    ptr += 12;
  }

  // Execute the original init function
  versionInfoInit();
};

// Formats single-byte number into M.mm string.
// dst is expected to be at least 5 bytes long
void formatRevision(char *dst, uint8_t rev) {
  dst[0] = '0' + (rev >> 4);
  dst[1] = '.';
  dst[2] = '0' + ((rev & 0x0f) / 10);
  dst[3] = '0' + ((rev & 0x0f) % 10);
  dst[4] = '\0';
}

// Extends version menu with custom entries by overriding the function called every time the version menu opens
void patchVersionInfo(uint8_t *osd) {
  verinfoStringTableAddr = hddosdVerinfoStringTableAddr;
  // Find the function that inits version menu entries
  uint8_t *ptr = findPatternWithMask(osd, 0x100000, (uint8_t *)patternVersionInit, (uint8_t *)patternVersionInit_mask, sizeof(patternVersionInit));
  if (!ptr)
    return;

  // Get the original function call and save the address
  uint32_t tmp = _lw((uint32_t)ptr);
  tmp &= 0x03ffffff;
  tmp <<= 2;
  versionInfoInit = (void *)tmp;

  // Replace versionInfoInit with the custom function
  tmp = 0x0c000000;
  tmp |= ((uint32_t)versionInfoInitHandler >> 2);
  _sw(tmp, (uint32_t)ptr); // jal versionInfoInitHandler

  // Find sceGsGetGParam address
  ptr = findPatternWithMask(osd, 0x100000, (uint8_t *)patternGsGetGParam, (uint8_t *)patternGsGetGParam_mask, sizeof(patternGsGetGParam));
  if (ptr) {
    tmp = _lw((uint32_t)ptr);
    tmp &= 0x03ffffff;
    tmp <<= 2;
    sceGsGetGParam = (void *)tmp;
  }

  // Find sceCdApplySCmd address
  ptr = findPatternWithMask(osd, 0x100000, (uint8_t *)patternCdApplySCmd, (uint8_t *)patternCdApplySCmd_mask, sizeof(patternCdApplySCmd));
  if (ptr) {
    uint32_t fnptr = (uint32_t)ptr;
    while ((_lw(fnptr) & 0xffff0000) != 0x27bd0000)
      fnptr -= 4;

    sceCdApplySCmd = (void *)fnptr;
  }

  // Initialize static values
  // ROM version
  if (settings.romver[0] != '\0') {
    memcpy(&romverValue[6], settings.romver, 14);
  } else {
    romverValue[0] = '-';  // Put placeholer value
    romverValue[1] = '\0'; // Put placeholer value
  }

  // EE Revision
  formatRevision(eeRevision, GetCop0(15));
}

char *getVideoMode() {
  uint16_t vmode;
  if (sceGsGetGParam) {
    sceGsGParam *gParam = sceGsGetGParam();
    vmode = gParam->sceGsOutMode;
  } else {
    // This function doesn't exist on protokernel,
    // so try using video mode from settings
    vmode = settings.videoMode;
    if (!vmode)
      vmode = GS_MODE_NTSC; // Default to NTSC
  }

  switch (vmode) {
  case GS_MODE_PAL:
    return "PAL";
  case GS_MODE_NTSC:
    return "NTSC";
  case GS_MODE_DTV_480P:
    return "480p";
  case GS_MODE_DTV_1080I:
    return "1080i";
  }

  return "-";
}

char *getGSRevision() {
  uint8_t rev = (*GS_REG_CSR >> 16) & 0xFF;
  if (rev) {
    formatRevision(gsRevision, rev);
    return gsRevision;
  }

  gsRevision[0] = '-';
  gsRevision[1] = '\0';
  return gsRevision;
}

char *getMechaConRevision() {
  if (!sceCdApplySCmd || mechaconRev[0] == '\0')
    return NULL;

  if (mechaconRev[0] != '0')
    // Retrieve the revision only once
    return mechaconRev;

  // sceCdApplySCmd response is always 16 bytes and will corrupt memory with a smaller buffer
  // byte 0 - status byte, byte 1 - major, byte 2 - minor
  uint8_t outBuffer[16] = {0};

  if (sceCdApplySCmd(0x03, outBuffer, 1, outBuffer)) {
    if (outBuffer[1] > 4) {
      // If major version is >=5, clear the last bit (DTL flag on Dragon consoles)
      if (!(outBuffer[2] & 0x1))
        mechaconRev[4] = '\0';

      outBuffer[2] &= 0xFE;
    } else
      mechaconRev[4] = '\0';

    mechaconRev[0] = outBuffer[1] + '0';
    mechaconRev[2] = '0' + (outBuffer[2] / 10);
    mechaconRev[3] = '0' + (outBuffer[2] % 10);
    return mechaconRev;
  }

  // Failed to get the revision
  mechaconRev[0] = '\0';
  return NULL;
}

//
// 480p/1080i patch.
// Partially based on Neutrino GSM
//

// Struct passed to sceGsPutDispEnv
typedef struct {
  uint64_t pmode;
  uint64_t smode2;
  uint64_t dispfb;
  uint64_t display;
  uint64_t bgcolor;
} sceGsDispEnv;

static void (*origSetGsCrt)(short int interlace, short int mode, short int ffmd) = NULL;
static GSVideoMode selectedMode = 0;

// Using dedicated functions instead of switching on selectedMode because SetGsCrt is executed in kernel mode
static void setGsCrt480p(short int interlace, short int mode, short int ffmd) {
  // Override out mode
  origSetGsCrt(0, GS_MODE_DTV_480P, ffmd);
  return;
}
static void setGsCrt1080i(short int interlace, short int mode, short int ffmd) {
  // Override out mode
  origSetGsCrt(1, GS_MODE_DTV_1080I, ffmd);
  return;
}

// sceGsPutDispEnv function replacement
void gsPutDispEnv(sceGsDispEnv *disp) {
  if (sceGsGetGParam) {
    // Modify gsGsGParam (needed to show the proper mode in the Version submenu, completely cosmetical)
    sceGsGParam *gParam = sceGsGetGParam();
    switch (selectedMode) {
    case GS_MODE_DTV_480P:
      gParam->sceGsInterMode = 0;
    case GS_MODE_DTV_1080I:
      gParam->sceGsOutMode = selectedMode;
    default:
    }
  }
  // Override writes to SMODE2/DISPLAY2 registers
  switch (selectedMode) {
  case GS_MODE_DTV_480P:
    GS_SET_SMODE2(0, 1, 0);
    GS_SET_DISPLAY2(318, 50, 1, 1, 1279, 447);
    break;
  case GS_MODE_DTV_1080I:
    *GS_REG_SMODE2 = disp->smode2;
    GS_SET_DISPLAY2(558, 130, 1, 1, 1279, 895);
    break;
  default:
    *GS_REG_SMODE2 = disp->smode2;
    *GS_REG_DISPLAY2 = disp->display;
  }

  *GS_REG_PMODE = disp->pmode;
  *GS_REG_DISPFB2 = disp->dispfb;
  *GS_REG_BGCOLOR = disp->bgcolor;
}

// Overrides SetGsCrt and sceGsPutDispEnv functions to support 480p and 1080i output modes
// ALWAYS call restoreGSVideoMode before launching apps
void patchGSVideoMode(uint8_t *osd, GSVideoMode outputMode) {
  if (outputMode < GS_MODE_DTV_480P)
    return; // Do not apply patch for PAL/NTSC modes

  // Find sceGsPutDispEnv address
  uint8_t *ptr = findPatternWithMask(osd, 0x100000, (uint8_t *)patternGsPutDispEnv, (uint8_t *)patternGsPutDispEnv_mask, sizeof(patternGsPutDispEnv));
  if (!ptr)
    return;

  // Get the address of the original SetGsCrt handler and translate it to kernel mode address range used by syscalls (kseg0)
  origSetGsCrt = (void *)(((uint32_t)GetSyscallHandler(0x2) & 0x0fffffff) | 0x80000000);
  if (!origSetGsCrt)
    return;

  // Replace call to sceGsPutDispEnv with the custom function
  uint32_t tmp = 0x0c000000;
  tmp |= ((uint32_t)gsPutDispEnv >> 2);
  _sw(tmp, (uint32_t)ptr); // jal gsPutDispEnv

  // Replace SetGsCrt with custom handler
  switch (outputMode) {
  case GS_MODE_DTV_480P:
    selectedMode = outputMode;
    SetSyscall(0x2, (void *)(((uint32_t)(setGsCrt480p) & ~0xE0000000) | 0x80000000));
    break;
  case GS_MODE_DTV_1080I:
    selectedMode = outputMode;
    SetSyscall(0x2, (void *)(((uint32_t)(setGsCrt1080i) & ~0xE0000000) | 0x80000000));
    break;
  default:
  }
}

// Restores SetGsCrt.
// Can be safely called even if GS video mode patch wasn't applied
void restoreGSVideoMode() {
  if (!origSetGsCrt)
    return;

  // Restore the original syscall handler
  SetSyscall(0x2, origSetGsCrt);
}

//
// Browser application launch patch
// Adds the "Enter" option for ELF files or SAS-compliant applications
//
// 1. Inject custom code into the function that generates icon data
//    This code checks the directory name and sets "application" flag (0x40) if it contains _ at index 3, equals BOOT or ends with .ELF/.elf
// 2. Inject custom code into the function that sets up data for exiting to main menu
//    This code checks the icon data and writes the ELF path into pathBuf if all conditions are met
// 3. Inject custom code into the function that exits to main menu
//    This code calls launchItem instead of the original function if pathBuf is not empty
//
// Browser entry icon offsets:
//  0x90  - icon flags
//  0x134 - pointer to parent device data
//  0x1C0 - dir/file name
// Browser device data offsets:
//  0x124 - device number (2 - mc0, 6 - mc1, >10 = directories on HDD)
//  0x12C - device name or directory name in __common

// Path buffer
char pathBuf[100];
// Original functions
void (*buildIconData)(uint8_t *iconDataLocation, uint8_t *deviceDataLocation) = NULL;
void (*exitToPreviousModule)() = NULL;
void (*setupExitToPreviousModule)(int appType) = NULL;

void buildIconDataCustom(uint8_t *iconDataLocation, uint8_t *deviceDataLocation) {
  // Call the original function
  buildIconData(iconDataLocation, deviceDataLocation);

  // Get directory path
  char *dirPath = (char *)(iconDataLocation + 0x1c0);
  if (!dirPath)
    return;

  // Set executable flag if directory name has _ at index 3, equals BOOT or if file name ends with .elf/.ELF
  if ((dirPath[4] == '_') || !strcmp(dirPath, "/BOOT"))
    *(iconDataLocation + 0x90) |= 0x40;
  else {
    dirPath = strrchr(dirPath, '.');
    if (dirPath && (!strcmp(dirPath, ".elf") || !strcmp(dirPath, ".ELF")))
      *(iconDataLocation + 0x90) |= 0x40;
  }
}

void exitToPreviousModuleCustom() {
  if (pathBuf[0] != '\0')
    launchItem(pathBuf);

  exitToPreviousModule();
}

void setupExitToPreviousModuleCustom(int appType) {
  uint32_t iconPropertiesPtr = 0;
  uint32_t deviceDataPtr = 0;
  // Get the pointer to currently selected item from $s2
  asm volatile("move %0, $s2\n\t" : "=r"(iconPropertiesPtr)::);

  char *targetName = NULL;
  if (iconPropertiesPtr) {
    // Get the pointer to device properties starting at offset 0x134
    deviceDataPtr = _lw(iconPropertiesPtr + 0x134);
    // Find the path in icon properties starting from offset 0x160
    targetName = (char *)iconPropertiesPtr + 0x1c0;
  }

  if (appType || !deviceDataPtr || !targetName)
    goto out;

  pathBuf[0] = '\0';

  // Get the device number and build the path
  uint32_t devNumber = _lw(deviceDataPtr + 0x124);
  switch (devNumber) {
  case 6:
    // mc1
    devNumber -= 3;
  case 2:
    // mc0
    devNumber -= 2;
    pathBuf[0] = '\0';
    strcat(pathBuf, "mc?:");
    pathBuf[2] = devNumber + '0';
    strcat(pathBuf, targetName);
    break;
  default:
    if (devNumber < 11)
      goto out;

    // Assume the target is one of directories in hdd0:__common
    char *commonDirName = (char *)deviceDataPtr + 0x12c;
    if (!commonDirName)
      goto out;

    strcat(pathBuf, "hdd0:__common:pfs:");
    strcat(pathBuf, commonDirName);
    if (targetName[0] != '/')
      strcat(pathBuf, "/");

    strcat(pathBuf, targetName);
    break;
  }

  // Append title.cfg for SAS applications
  targetName = strrchr(targetName, '.');
  if (!targetName || (strcmp(targetName, ".ELF") && strcmp(targetName, ".elf")))
    strcat(pathBuf, "/title.cfg");

out:
  setupExitToPreviousModule(appType);
  return;
}

// Browser application launch patch
void patchBrowserApplicationLaunch(uint8_t *osd) {
  // Find buildIconData address
  uint8_t *ptr1 =
      findPatternWithMask(osd, 0x100000, (uint8_t *)patternBuildIconData, (uint8_t *)patternBuildIconData_mask, sizeof(patternBuildIconData));
  if (!ptr1)
    return;

  // Find exitToPreviousModule address
  uint8_t *ptr2 = findPatternWithMask(osd, 0x100000, (uint8_t *)patternExitToPreviousModule, (uint8_t *)patternExitToPreviousModule_mask,
                                      sizeof(patternExitToPreviousModule));
  if (!ptr2)
    return;

  // Find setupExitToPreviousModule address
  uint8_t *ptr3 = findPatternWithMask(osd, 0x100000, (uint8_t *)patternSetupExitToPreviousModule, (uint8_t *)patternSetupExitToPreviousModule_mask,
                                      sizeof(patternSetupExitToPreviousModule));
  if (!ptr3)
    return;

  ptr1 += 0x4;
  buildIconData = (void *)((_lw((uint32_t)ptr1) & 0x03ffffff) << 2);
  _sw((0x0c000000 | ((uint32_t)buildIconDataCustom >> 2)), (uint32_t)ptr1); // jal browserDirSubmenuInitViewCustom

  ptr2 += 0xc;
  exitToPreviousModule = (void *)((_lw((uint32_t)ptr2) & 0x03ffffff) << 2);
  _sw((0x0c000000 | ((uint32_t)exitToPreviousModuleCustom >> 2)), (uint32_t)ptr2); // jal exitToPreviousModuleCustom

  setupExitToPreviousModule = (void *)((_lw((uint32_t)ptr3) & 0x03ffffff) << 2);
  _sw((0x0c000000 | ((uint32_t)setupExitToPreviousModuleCustom >> 2)), (uint32_t)ptr3); // jal setupExitToPreviousModuleCustom
}
