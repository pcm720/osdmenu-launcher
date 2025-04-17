#ifndef _PATTERNS_OSDMENU_H_
#define _PATTERNS_OSDMENU_H_
// Additional patch patterns for osdmenu-launcher
#include <stdint.h>

// Pattern for injecting custom entries into the Version submenu
static uint32_t patternVersionInit[] = {
    // Search pattern in the OSD pad handler table (at address 0x208800 for ROMVER 0200E20040614):
    0x00000000, // nop
                // case 0x16:
    0x0c000000, // jal versionInfoInit — target function that inits version submenu strings
    0x3c10001f, // lui s0,0x001F
    0x1000ffad, // beq zero, zero, <switch>
    0x00000000, // nop
};
static uint32_t patternVersionInit_mask[] = {0xffffffff, 0xfc000000, 0xffffffff, 0xffffffff, 0xffffffff};

// Pattern for getting the Version submenu string table address from the versionInfoInit function.
// Address will point to the first string location ("Console")
// The table seems to have a fixed location at address 0x1f1238 in ROM versions 1.20-2.50
static uint32_t patternVersionStringTable[] = {
    // Search pattern in the versionInfoInit function that points to the string table address
    0x3c030000, // lui  v1, <top address bytes>
    0x00000018, // mult ??,??,??
    0x34630000, // ori  v1,v1, <bottom address bytes>
};
static uint32_t patternVersionStringTable_mask[] = {0xffff0000, 0x00000018, 0xffff0000};

// Pattern for getting the address of the sceGsGetGParam function
static uint32_t patternGsGetGParam[] = {
    // Searching for particular pattern in sceGsResetGraph
    0x0c000000, // jal sceGsGetGParam
    0x00000000, // nop
    0x3c031200, // lui v1,0x1200 // REG_GS_CSR = 0x200
    0x24040200, // li a0,0x200
    0x34631000, // ori v1,v1,0x1000
};
static uint32_t patternGsGetGParam_mask[] = {0xfc000000, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff};

// Pattern for overriding the sceGsPutDispEnv function call in sceGsSwapDBuff
static uint32_t patternGsPutDispEnv[] = {
    // Searching for particular pattern in sceGsSwapDBuff
    0x0c000000, // jal sceGsPutDispEnv
    0x00512021, // addu a0,v0,s1
    0x12000005, // beq s0,zero,0x05
    0x00000000, // nop
    0x0c000000, // jal sceGsPutDrawEnv
    0x26240140, // addiu a0,s1,0x0140
    0x10000004, // beq zero,zero,0x04
    0xdfbf0020, // ld ra,0x0020,sp
};
static uint32_t patternGsPutDispEnv_mask[] = {
    0xfc000000, 0xffffffff, 0xffffffff, 0xffffffff, //
    0xfc000000, 0xffffffff, 0xffffffff, 0xffffffff, //
};

// Pattern for getting the address of sceCdApplySCmd function
// After finding this pattern, go back until reaching
// _lw(addr) & 0xffff0000 == 0x27bd0000 (addiu $sp,$sp, ?) to get the function address.
static uint32_t patternCdApplySCmd[] = {
    //  0x27bd0000, // addiu $sp,$sp,??
    //  ~15-20 instructions
    0x0c000000, // jal WaitSema
    0x00000000, // ...
    0x3c000000, // lui v?, ?
    0x24000019, // li v?, 0x19
    0x00000000, // ...
    0x0c000000, // jal sceCdSyncS
};
static uint32_t patternCdApplySCmd_mask[] = {0xfc000000, 0x00000000, 0xfc000000, 0xfc00ffff, 0x00000000, 0xfc000000};

// Pattern for getting the address of the Browser file properties/Copy/Delete view init function
// Seems to be consistent across all ROM versions, including protokernels
static uint32_t patternBrowserFileMenuInit[] = {
    0x27bdffe0, // addiu sp,sp,-0x20
    0x30a500ff, // andi  a1,a1,0x00FF
                // 0xffb00000, // sd s0,0x0000,sp // s0 contains current memory card index
                // 0xffbf0010, // sd ra,0x0010,sp
                // 0x0c000000, // jal browserDirSubmenuInitView <- target function
};
static uint32_t patternBrowserFileMenuInit_mask[] = {0xffffffff, 0xffffffff};

// Pattern for getting the address of the currently selected memory card
// Located in browserDirSubmenuInitView function
static uint32_t patternBrowserSelectedMC[] = {
  0x8f820000, // lw v0,0x0000,gp <-- address relative to $gp
  0x2442fffe, // addiu v0,v0,-0x2
  0x2c420000, // sltiu v0,v0,0x?
};
static uint32_t patternBrowserSelectedMC_mask[] = {0xffff0000,0xffffffff, 0xfffffff0};

// Pattern for getting the address of the sceOpen function
// Seems to be consistent across all ROM versions, including protokernels
static uint32_t patternSCEopen[] = {
    // 0x27bd???? // addiu sp,sp,0x???? --> function start
    // 0x3c02???? // lui v0,0x????
    // 0x3c03???? // lui v1,0x????
    0xffb40050, // sd s4,0x0050,sp
    0xffb30040, // sd s3,0x0040,sp
    0x0060a02d, // daddu s4,v1,zero
};
static uint32_t patternSCEopen_mask[] = {0xffffffff, 0xffffffff, 0xffffffff};

// Pattern for getting the address of the sceClose function
// Seems to be consistent across all ROM versions, including protokernels
static uint32_t patternSCEclose[] = {
    0x27bdffa0, // addiu sp,sp,-0x70
    0xffb30040, // sd    s3,0x0040,sp
    0xffb10020, // sd    s1,0x0020,sp
    0xffb00010, // sd    s0,0x0010,sp
};
static uint32_t patternSCEclose_mask[] = {0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff};

//
// Protokernel patterns
//

// Pattern for injecting custom entries into the Version submenu
static uint32_t patternVersionInit_Proto[] = {
    // <init Unit, Browser, CD Player, PS1DRV version>
    0x24a615c8, // addiu a2,a1,0x15C8
    0x24a41598, // addiu a0,a1,0x1598
    0x0c000000, // jal   getDVDPlayerVersion
};
static uint32_t patternVersionInit_Proto_mask[] = {0xffffffff, 0xffffffff, 0xfc000000};

// Pattern for getting the address of sceCdApplySCmd function
// After finding this pattern, go back until reaching
// _lw(addr) & 0xffff0000 == 0x27bd0000 (addiu $sp,$sp, ?) to get the function address.
static uint32_t patternCdApplySCmd_Proto[] = {
    //  0x27bd0000, // addiu $sp,$sp,??
    //  ~50 instructions
    0x26500000, // addiu s0,s2,??
    0x3c058000, // lui   a1,0x8000
    0x0200202d, // daddu a0,s0,zero
    0x34a50593, // ori   a1,a1,0x0593
    0x0c000000, // jal   sceSifBindRpc
    0x0000302d, // daddu a2,zero,zero
};
static uint32_t patternCdApplySCmd_Proto_mask[] = {0xffff0000, 0xffffffff, 0xffffffff, 0xffffffff, 0xfc000000, 0xffffffff};

#endif
