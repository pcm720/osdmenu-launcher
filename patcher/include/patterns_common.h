#ifndef _PATTERNS_COMMON_H_
#define _PATTERNS_COMMON_H_
#include <stdint.h>

//
// FMCB 1.8 patterns
//

// ExecPS2 pattern for patching OSDSYS unpacker on newer PS2 with compressed OSDSYS
static uint32_t patternExecPS2[] = {
    0x24030007, // li v1, 7
    0x0000000c, // syscall
    0x03e00008, // jr ra
    0x00000000  // nop
};
static uint32_t patternExecPS2_mask[] = {0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff};

//
// The following two patterns are introduced in FMCB 1.9 and found by reverse-engineering FMCB 1.9 code
//

// Used to deinit OSDSYS
static uint32_t patternOSDSYSDeinit[] = {
    0x27bdffe0, // addiu sp,sp,0xFFE0
    0xffb00000, // sd    s0,0x0000,sp
    0x0080802d, // addiu s0,a0,zero
    0xffbf0010, // sd    ra,0x0010,sp
    0x0c000000, // jal   DisableIntc
    0x24040003, // addiu a0,zero,0x0003
    0x0c000000, // jal   DisableIntc
    0x24040002, // addiu a0,zero,0x0002
};
static uint32_t patternOSDSYSDeinit_mask[] = {
    0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xfc000000, 0xffffffff, 0xfc000000, 0xffffffff,
};

#ifndef HOSD
// Used to inject the patching function into the OSDSYS init on protokernels
// Inject j <applyPatches> at offset +0x3c
static uint32_t patternOSDSYSProtokernelInit[] = {
    0x26940414, // addiu s4,s4,0x0414
    0x26d60414, // addiu s6,s6,0x0414
    0x2aa2000c, // slti  v0,s5,0x000C
    0x14400000, // bne   v0,zero,0x????
    0x26520414, // addiu s2,s2,0x0414
};
static uint32_t patternOSDSYSProtokernelInit_mask[] = {
    0xffffffff, 0xffffffff, 0xffffffff, 0xffff0000, 0xffffffff,
};
#else
//
// HDD-OSD patterns
//

static uint32_t patternSCERemove[] = {
    0x27bdfff0, // addiu sp,sp,0xFFF0
    0xffbf0000, // sd    ra,0x0000,sp
    0x0c000000, // jal   _sceCallCode
    0x24050006, // addiu a1,zero,0x6
    0xdfbf0000, // ld    ra,0x0000,sp
};
static uint32_t patternSCERemove_mask[] = {0xffffffff, 0xffffffff, 0xfc000000, 0xffffffff, 0xffffffff};

static uint32_t patternSCEUmount[] = {
    0x27bdfff0, // addiu sp,sp,0xFFF0
    0xffbf0000, // sd    ra,0x0000,sp
    0x0c000000, // jal   _sceCallCode
    0x24050015, // addiu a1,zero,0x15
    0xdfbf0000, // ld    ra,0x0000,sp
};
static uint32_t patternSCEUmount_mask[] = {0xffffffff, 0xffffffff, 0xfc000000, 0xffffffff, 0xffffffff};
#endif

#endif
