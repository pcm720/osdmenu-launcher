
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
  // Execute the original init function
  versionInfoInit();

  // Extend the string table used by the version menu drawing function.
  // It picks up the entries automatically and stops once it gets a NULL pointer (0)
  //
  // Each table entry is represented by three words:
  // Word 0 — pointer to entry name string
  // Word 1 — pointer to entry value string
  // Word 2 — indicates whether the entry has a submenu and points to:
  // 1. ROMs <2.00 — a list of newline-separated submenu entries where each menu entry is represented
  //  as a comma-separated list of strings (e.g. 'Disc Speed,Standard,Fast\nTexture Mapping,Standard,Smooth\n').
  //  Used to build the menu, but not used to draw it.
  // 2. ROMs >=2.00 — some unknown value that doesn't seem to be used by the menu functions as modifying it doesn't seem to change anything
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
    _sw(0, ptr + 8);
    ptr += 12;
  }
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
  // Find the function that inits version menu entries
  uint8_t *ptr = findPatternWithMask(osd, 0x100000, (uint8_t *)patternVersionInit, (uint8_t *)patternVersionInit_mask, sizeof(patternVersionInit));
  if (!ptr)
    return;

  // First pattern word is nop, advance ptr to point to the function call
  ptr += 4;

  // Get the original function call and save the address
  uint32_t tmp = _lw((uint32_t)ptr);
  tmp &= 0x03ffffff;
  tmp <<= 2;
  versionInfoInit = (void *)tmp;

  // Find the string table address in versionInfoInit
  // Even if it's the same in all ROM versions >=1.20, this acts as a basic sanity check
  // to make sure the patch is replacing the actual versionInfoInit
  uint8_t *tableptr = findPatternWithMask((uint8_t *)versionInfoInit, 0x200, (uint8_t *)patternVersionStringTable,
                                          (uint8_t *)patternVersionStringTable_mask, sizeof(patternVersionStringTable));
  if (!tableptr)
    return;

  // Assemble the table address
  tmp = (_lw((uint32_t)tableptr) & 0xFFFF) << 16;
  tmp |= (_lw((uint32_t)tableptr + 8) & 0xFFFF);

  if (tmp > 0x100000 && tmp < 0x2000000)
    // Make sure the address is in the valid address space
    verinfoStringTableAddr = tmp;
  else
    return;

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
// Swaps file properties and Copy/Delete menus around and launches an app when pressing Enter
// if a title.cfg file is present in the save file directory
//

// Initializes submenu data
// entryProps — a pointer to save file properties. Contains icon properties, save date, file size and path relative to mc0/mc1
// fileSubmenuType — indicates submenu type, where 0 is the Copy/Delete menu and 1 is save file properties­
static void (*browserDirSubmenuInitView)(uint8_t *entryProps, uint8_t fileSubmenuType) = NULL;
// Returns directory size. Called only after submenu is entered and only once: the result is cached in entry properties and the buffer
// is reused for subsequent sceMcGetDir calls. To persist the app indicator, browserGetMcDirSizeCustom stores 0xAA at *(entryProps - 0x2)
static int (*browserGetMcDirSize)(void) = NULL;
// The address of sceMcGetDir result buffer
static uint32_t sceMcGetDirResultAddr = 0x0;

// Selected MC index offset from $gp, signed
int16_t selectedMCOffset = 0;
// Entry properties address for the currently selected entry
// The most significant bit is used to signal the browserGetMcDirSizeCustom function that it needs to launch the app immediately
uint32_t entryPropsAddr = 0;
// Path buffer
char pathBuf[100];

// This function is always executed first and called every time a submenu is opened
void browserDirSubmenuInitViewCustom(uint8_t *entryProps, uint8_t fileSubmenuType) {
  // Store the address for browserGetMcDirSizeCustom
  entryPropsAddr = (uint32_t)entryProps;

  if (fileSubmenuType == 1) { // Swap functions around so "Option" button triggers the Copy/Delete menu
    browserDirSubmenuInitView(entryProps, 0);
    return;
  }

  // Get memory card number by reading address relative to $gp
  int mcNumber;
  asm volatile("addu $t0, $gp, %1\n\t" // Add the offset to the gp register
               "lw %0, 0($t0)"         // Load the word at the offset
               : "=r"(mcNumber)
               : "r"((int32_t)selectedMCOffset) // Cast the type to avoid GCC using lhu instead of lh
               : "$t0");

  // Translate the memory card number
  // The memory card number OSDSYS uses equals 2 for mc0 and 6 for mc1 (3 for mc1 on protokernels).
  switch (mcNumber) {
  case 6: // mc1
    mcNumber -= 3;
  case 3:
  case 2: // mc0
    mcNumber = mcNumber - 2;

    // Set the most significant bit to indicate that immediate launch is needed
    entryPropsAddr |= (1 << 31);

    // Find the path in icon properties starting from offset 0x160
    char *stroffset = (char *)entryProps + 0x160;
    while (*stroffset != '/')
      stroffset++;

    // Assemble the path
    pathBuf[0] = '\0';
    strcat(pathBuf, "mc?:");
    strcat(pathBuf, stroffset);
    strcat(pathBuf, "/title.cfg");
    pathBuf[2] = mcNumber + '0';

    if (*(entryProps - 0x2) == 0xAA)
      // If icon is an app, launch the app
      launchItem(pathBuf);
    // Else, the app will be launched by the browserGetMcDirSizeCustom function
  }

  browserDirSubmenuInitView(entryProps, 1);
}

// This function is always executed after browserDirSubmenuInitViewCustom and never called again
// once it returns the directory size until the user goes back to the memory card select screen.
// If immediate launch is requested, launches the app if it has title.cfg entry in sceMcGetDir result
// Otherwise, puts 0xAA at (entryProps - 0x2) for browserDirSubmenuInitViewCustom
int browserGetMcDirSizeCustom() {
  int res = browserGetMcDirSize();
  // This function returns -8 until sceMcSync returns the result
  if ((res != -8)) {
    // Each entry occupies 0x40 bytes, with entry name starting at offset 0x20
    char *off = (char *)(sceMcGetDirResultAddr + 0x20);
    while (off[0] != '\0') {
      if (!strcmp("title.cfg", off)) {
        // If immediate launch is requested, launch the app
        if (entryPropsAddr & (1 << 31))
          launchItem(pathBuf);

        // Else, put app marker (0xAA) to *(entryProps - 0x2)
        *(((uint8_t *)((entryPropsAddr) & 0x7FFFFFFF)) - 0x2) = 0xAA;
        off += 0x40;
        break;
      }
      off += 0x40;
    }
    // Zero-out the result buffer to avoid false positives since we don't know the number of entries
    memset((void *)sceMcGetDirResultAddr, 0, ((uint32_t)off - sceMcGetDirResultAddr));
  }
  return res;
}

// Browser application launch patch
void patchBrowserApplicationLaunch(uint8_t *osd) {
  // Find the target function
  uint8_t *ptr = findPatternWithMask(osd, 0x100000, (uint8_t *)patternBrowserFileMenuInit, (uint8_t *)patternBrowserFileMenuInit_mask,
                                     sizeof(patternBrowserFileMenuInit));

  if (!ptr || ((_lw((uint32_t)ptr + 4 * 4) & 0xfc000000) != 0x0c000000))
    return;
  ptr += 4 * 4;

  // Get the original function call and save the address
  browserDirSubmenuInitView = (void *)((_lw((uint32_t)ptr) & 0x03ffffff) << 2);

  // Find the selected memory card offset
  uint8_t *ptr2 = findPatternWithMask((uint8_t *)browserDirSubmenuInitView, 0x500, (uint8_t *)patternBrowserSelectedMC,
                                      (uint8_t *)patternBrowserSelectedMC_mask, sizeof(patternBrowserSelectedMC));
  if (!ptr2)
    return;
  // Store memory card offset
  selectedMCOffset = _lw((uint32_t)ptr2) & 0xffff;

  // Find the target function
  ptr2 = findPatternWithMask(osd, 0x100000, (uint8_t *)patternBrowserGetMcDirSize, (uint8_t *)patternBrowserGetMcDirSize_mask,
                             sizeof(patternBrowserGetMcDirSize));
  if (!ptr2)
    return;
  // Get the original function call and save the address
  browserGetMcDirSize = (void *)((_lw((uint32_t)ptr2) & 0x03ffffff) << 2);

  // Trace the sceMcGetDir result buffer address
  // From the browserGetMcDirSize function, get the address of the next function call
  //  that retrieves directory size for mc0/mc1. This function sends sceMcGetDir request
  //  to the libmc worker thread. The buffer address is stored in $t0.
  // This function loads the upper part of the address to $v0
  // adds the base offset to $s1, adds constant offset and puts the result into $t0.
  // So, to get the address:
  // 1. Find the first load into $v0 (lui   v0,????)
  // 2. Find the first add into $s1  (addiu s1,v0,????)
  // 3. Add the constant offset 0xc38 (same for all ROM versions >=1.10)
  uint32_t offset = (uint32_t)browserGetMcDirSize;

  // Find the first function call in browserGetMcDirSize and get the function address
  while ((_lw(offset) & 0xfc000000) != 0x0c000000)
    offset += 4;
  offset = (_lw((uint32_t)offset) & 0x03ffffff) << 2;

  // Initialize with fixed offset
  sceMcGetDirResultAddr = 0xc38;

  // Search for lui v0,???? instruction to get the upper part
  while ((_lw(offset) & 0x3c020000) != 0x3c020000)
    offset += 4;
  sceMcGetDirResultAddr |= (_lw(offset) & 0xffff) << 16;

  // Search for addiu s1,v0,???? to get the lower part (might be negative)
  while ((_lw(offset) & 0x24510000) != 0x24510000)
    offset += 4;
  sceMcGetDirResultAddr += (int32_t)((int16_t)(_lw(offset) & 0xffff));

  // Replace original functions
  _sw((0x0c000000 | ((uint32_t)browserDirSubmenuInitViewCustom >> 2)), (uint32_t)ptr); // jal browserDirSubmenuInitViewCustom
  _sw((0x0c000000 | ((uint32_t)browserGetMcDirSizeCustom >> 2)), (uint32_t)ptr2);      // jal browserGetMcDirSizeCustom
}
