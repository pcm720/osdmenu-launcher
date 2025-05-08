#ifndef _PATTERNS_OSDMENU_H_
#define _PATTERNS_OSDMENU_H_
// Additional patch patterns for osdmenu-launcher
#include <stdint.h>

// Pattern for injecting custom entries into the Version submenu
static uint32_t patternVersionInit[] = {
    0x0c000000, // jal   initVersionInfoStrings
    0x26300000, // addiu s0,s1,0x????
    0x10000003, // beq   zero,zero,0x0003
};
static uint32_t patternVersionInit_mask[] = {0xfc000000, 0xffff0000, 0xffffffff};

// Using fixed offset to avoid unneeded tracing
uint32_t hddosdVerinfoStringTableAddr = 0x1f1298;

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

// Pattern for getting the address of the function related to filling the browser icon properties
static uint32_t patternBuildIconData[] = {
    0xacc00690, // sw zero,0x0690,a2
    0x0c000000, // jal buildIconData
    0xacc00694, // sw zero,0x0694,a2
};
static uint32_t patternBuildIconData_mask[] = {0xffffffff, 0xfc000000, 0xffffffff};

// Pattern for geting the address of the exit from browser function
static uint32_t patternExitToPreviousModule[] = {
    0x8f820000, // sw zero,0x0690,a2
    0x14000000, // bne ??, ??, ??
    0x00000000, // nop
    0x0c000000, // jal exitToPreviousModule
    0x00000000, // nop
};
static uint32_t patternExitToPreviousModule_mask[] = {0xffff0000, 0xff000000, 0xffffffff, 0xfc000000, 0xffffffff};

// Pattern for geting the address of the function that sets up data for main menu, used to launch CD/DVD or HDD applications
// When this function is called, $s2 register contains the address of the currently selected entry icon data
static uint32_t patternSetupExitToPreviousModule[] = {
    0x0c000000, // jal setupExitToPreviousModule
    0x8e000694, // lw zero,0x0694,??
};
static uint32_t patternSetupExitToPreviousModule_mask[] = {0xfc000000, 0xff00ffff};

#endif
