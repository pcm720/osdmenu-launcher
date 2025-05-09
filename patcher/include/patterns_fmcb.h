#ifndef _PATTERNS_FMCB_H_
#define _PATTERNS_FMCB_H_
// FMCB 1.8 OSDSYS patch patterns by Neme
// Patterns adapter for HDD-OSD by pcm720
#include <stdint.h>

// Pattern for finding OSD menu info struct
static uint32_t patternMenuInfo[] = {
    0x00000001, // unknown
    0x00000000, // pointer to osdmenu
    0x00000002, // number of entries
    0x00000003, // unknown
    0x00000000  // current selection
};
static uint32_t patternMenuInfo_mask[] = {0xffffffff, 0xff800000, 0xffffffff, 0xffffffff, 0xffffffff};

// Pattern for injecting OSD strings
static uint32_t patternOSDString[] = {
    // Search pattern in the osd string function:
    0x10000005, //     beq	zero, zero, L2
    0x8c620000, //     lw	v0, $xxxx(v1)
    0x00101880, // L1: sll	v1, s0, 2	# string index * 4
    0x8c440000, //     lw	a0, $xxxx(v0)	# osd string pointer array
    0x00641821, //     addu	v1, v1, a0	# byte offset into array
    0x8c620000, //     lw	v0, $0000(v1)	# pointer to string
    0xdfbf0010  // L2: ld	ra, $0010(sp)
};
static uint32_t patternOSDString_mask[] = {0xffffffff, 0xffff0000, 0xffffffff, 0xffff0000, 0xffffffff, //
                                           0xffffffff, 0xffffffff};

// Pattern for patching the user input handling function, compatible with both CEX and DEX
// Slightly different offsets on DEX >=2.20 are likely caused by the addition of region select menu
static uint32_t patternUserInputHandler[] = {
    0x10000000, //     beq	zero, zero, exit
    0xdfbf0000, //     ld	ra, $00xx(sp)
    0x24040001, // L1: li	a0, 1		# the 2nd menu item (sys conf)
    0x8c430000, //     lw	v1, $xxxx(v0)	# current selection
    0x14640000, //     bne	v1, a0, exit	# sys config not selected?
    0xdfbf0000, //     ld	ra, $00xx(sp)
    0x0c000000, //     jal	StartSysConfig
    0x00000000, //     nop
    0x10000000, //     beq	zero, zero, exit
    0xdfbf0000  //     ld	ra, $00xx(sp)
};
static uint32_t patternUserInputHandler_mask[] = {0xffffff00, 0xffffff00, 0xffffffff, 0xffff0000, 0xffffff00,
                                                  0xffffff00, 0xfc000000, 0xffffffff, 0xffffff00, 0xffffff00};

// OSDMenu pattern for patching the draw functions for selected/unselected items
static uint32_t patternDrawMenuItem[] = {
// Search pattern in the drawmenu function:
#ifndef HOSD
    0x00101000, //     sll	v0, s0, 3	# selection multiplied by 8 (offset into menu)
    0x00431021, //     addu	v0, v0, v1	# pointer to string index
#else
    0x8e620000, //     lw   v0, 0x00??, s3
    0x02421021, //     addu v0, s2, v0	# pointer to string index
#endif
    0x0c000000, //     jal	GetOSDString
    0x8c440000, //     lw	a0, $0000(v0)	# get string index
    0x0040402d, //     daddu	t0, v0, zero	# arg4: pointer to string
    0x240401ae, //     li	a0, $01ae	# arg0: X coord = 430
    0x0200282d, //     daddu	a1, ??, zero	# arg1: Y coord
    0x26000000, //     addiu	a2, ??, $xxxx	# arg2: ptr to color components
    0x0c000000, //     jal	DrawMenuItem
#ifndef HOSD
    0x0260382d //     daddu	a3, s3, zero	# arg3: alpha
#else
    0x0280382d //     daddu	a3, s4, zero	# arg3: alpha
#endif
};
static uint32_t patternDrawMenuItem_mask[] = {0xffffff00, 0xffffffff, 0xfc000000, 0xffffffff, 0xffffffff, //
                                              0xffffffff, 0xff00ffff, 0xff000000, 0xfc000000, 0xffffffff};

// Patterns for patching the draw functions for bottom button prompts
static uint32_t patternDrawButtonPanel_1[] = {
    // Search for draw_button_panel function start:
    0x00c0002d, //     daddu XX, a2, zero
    0xff000000, //     sd	 XX, 0x00XX(sp)
    0x0080802d, //     daddu s0, a0, zero
    0xff000000, //     sd	 XX, 0x00XX(sp)
    0xff000000, //     sd	 XX, 0x00XX(sp)
    0xffb70000, //     sd	 s7, 0x00XX(sp)
    0xffb60000, //     sd	 s6, 0x00XX(sp)
    0xff000000, //     sd	 XX, 0x00XX(sp)
    0x0c000000, //     jal getOSDLanguage
    0xff000000  //     sd	 XX, 0x00XX(sp)
};
static uint32_t patternDrawButtonPanel_1_mask[] = {0xffff00ff, 0xff00ff00, 0xffffffff, 0xff00ff00, 0xff00ff00, //
                                                   0xffffff00, 0xffffff00, 0xff00ff00, 0xfc000000, 0xff00ff00};
static uint32_t patternDrawButtonPanel_2[] = {
    // Search pattern in the draw_button_panel function:
    0x3c020000, //     lui 	 v0, 0x00XX
    0x0200302d, //     daddu a2, XX, zero    # arg2 : Y
    0x24420000, //     addiu v0, v0, 0xXXXX
    0x00b12823, //     subu  a1, a1, s1
    0x8c44000c, //     lw 	 a0, 0x000c(v0)  # arg0 : Icon type
    0x02a0382d, //     daddu a3, s5, zero    # arg3 : alpha
    0x0c000000, //     jal 	 DrawIcon
    0x24a5ffe4  //     addiu a1, a1, 0xffe4  # arg1 : X
};
static uint32_t patternDrawButtonPanel_2_mask[] = {0xffffff00, 0xff00ffff, 0xffff0000, 0xffffffff, 0xffffffff, //
                                                   0xffffffff, 0xfc000000, 0xffffffff};
static uint32_t patternDrawButtonPanel_3[] = {
    // Search pattern in the draw_button_panel function:
    0x0040402d, //     daddu t0, v0, zero 	 # arg4 : pointer to string
    0x0200202d, //     daddu a0, s0, zero 	 # arg0 : X
    0x3c020000, //     lui	 v0, 0x00XX
#ifndef HOSD
    0x0200282d, //     daddu a1, XX, zero 	 # arg1 : Y
#else
    0x26000001, //     addiu a1,s4,0x0001
#endif
    0x24460000, //     addiu a2, v0, 0xXXXX  # arg2 : pointer to color struct
    0x0c000000, //     jal 	 DrawNonSelectableItem
    0x02a0382d  //     daddu a3, s5, zero 	 # arg3 : alpha
};
static uint32_t patternDrawButtonPanel_3_mask[] = {0xffffffff, 0xffffffff, 0xffffff00, 0xff00ffff, 0xffff0000, //
                                                   0xfc000000, 0xffffffff};

// Patterns for patching the ExecuteDisc function to override the disc launch handlers
static uint32_t patternExecuteDisc[] = {
  // ExecuteDisc function
  0x27bdfff0, //    addiu	sp, sp, $fff0
  0x3c03001f, //    lui	v1, $001f
  0xffbf0000, //    sd	ra, $0000(sp)
  0x0080302d, //    daddu	a2, a0, zero
  0x8c620010, //    lw	v0, $001?(v1)
  0x2c420000, //    sltiu	v0, v0, 7/8
  0x10400000, //    beq	v0, zero, quit
  0x00000000, //    li	v0, 1		# or ld ra,(sp)
  0x3c02001f, //    lui	v0, $001f
  0x8c420010, //    lw	v0, $001?(v0)
  0x3c030000, //    lui	v1, xxxx
  0x24630000  //    addiu	v1, v1, xxxx
};
static uint32_t patternExecuteDisc_mask[] = {0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xfffffff0, 0xfffffff0, //
                                           0xffffff00, 0x00000000, 0xffffffff, 0xfffffff0, 0xffffff00, 0xffff0000};

#ifndef HOSD
// OSDMenu patterns for patching the disc detection to bypass automatic disc launch
static uint32_t patternDetectDisc[] = {
    // Code around main menu disc detection part1
    0xac220cec, //    sw	v0, $0cec(at)
    0x0c000000, //    jal	xxxx
    0x00000000, //    nop
    0x0c000000, //    jal	xxxx
    0x00000000, //    nop
    0x3c02001f, //    lui	v0, $001f
    0x26440000, //    addiu	a0, s2, $xxxx
    0x8c430c44, //    lw	v1, $0c44(v0)
    0x0c000000, //    jal	xxxx
    0xac830008, //    sw	v1, $0008(a0)
    0x10000000, //    beq	zero, zero, xxxx
    0x26420000  //    addiu	v0, s2, $xxxx
};
static uint32_t patternDetectDisc_mask[] = {0xffffffff, 0xfc000000, 0xffffffff, 0xfc000000, 0xffffffff, 0xffffffff, //
                                            0xffff0000, 0xffffffff, 0xfc000000, 0xffffffff, 0xffff0000, 0xffff0000};
static uint32_t patternDetectDisc_Clock[] = {
    // Code around main menu disc detection part2
    0x3c02001f, //    lui	v0, $001f
    0x24030003, //    li	v1, 3
    0xac4305e8, //    sw	v1, $05e8(v0)
    0x24040002, //    li	a0, 2
    0xac4405ec, //    sw	a0, $05ec(v0)
    0x3c10001f, //    lui	s0, $001f
    0x24020001, //    li	v0, 1
    0xae0205e4, //    sw	v0, $05e4(s0)
    0x0c000000  //    jal	WaitVblankStart?
};
static uint32_t patternDetectDisc_Clock_mask[] = {0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
                                                  0xffffffff, 0xffffffff, 0xffffffff, 0xfc000000};
#else
// HOSDMenu patterns for patching the disc detection to bypass automatic disc launch
static uint32_t patternDetectDisc[] = {
    // Find the jump table in main() responsible for setting the disc type for the disc launch handler
    0x306300ff, // andi v1,v1,0xff
    0x10640022, // beq  v1,a0,0x0022
    0x28620015, // slti v0,v1,0x0015
    0x10400007, // beq  v1,a0,0x0007
    0x28620012, // slti v0,v1,0x0012
    0x10400019, // beq  v1,a0,0x0019
    0x28620010, // slti v0,v1,0x0010
    0x10400012, // beq  v1,a0,0x0012
};
static uint32_t patternDetectDisc_mask[] = {0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff};

static uint32_t patternDetectDisc_Clock[] = {
    // Find the jump table in clock handler responsible for setting the disc type for the disc launch handler
    0x10400011, // beq   v0,zero,0x0011
    0x2862006e, // slti  v0,v1,0x006e
    0x1040000a, // beq   v0,zero,0x000a
    0x2862006c, // slti  v0,v1,0x006c
    0x1040001d, // beq   v0,zero,0x001d
    0x2862006a, // slti  v0,v1,0x006a
    0x14400033, // bne   v0,zero,0x0033
    0x3c02001f, // lui   v0,0x001f
    0x3c03001f, // lui   v1,0x001f
    0x24020002, // addiu v0,zero,0x0002
    0xac620010, // sw    v0,0x0010,v1
    0x10000033, // beq   zero,zero,0x0033
    0x3c10001f, // lui   s0,0x001f
    0x2402006e, // addiu v0,zero,0x006E
    0x10620018, // beq   v1,v0,0x0018
};
static uint32_t patternDetectDisc_Clock_mask[] = {0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
                                                  0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff};
#endif

// Pattern for patching the menu scrolling behavior
static uint32_t patternMenuLoop[] = {0x30621000, 0x10400007, 0x2604ffe8, 0x8c830010, 0x2462ffff, 0x0441000e, 0xac820010};
static uint32_t patternMenuLoop_mask[] = {0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff};

// Pattern for patching the OSDSYS video mode
static uint32_t patternVideoMode[] = {0xffbf0000, 0x0c000000, 0x00000000, 0x38420002, 0xdfbf0000, 0x2c420001};
static uint32_t patternVideoMode_mask[] = {0xffffffff, 0xfc000000, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff};

#ifndef HOSD
// Pattern for patching update loading to bypass HDD init
static uint32_t patternHDDLoad[] = {
    // Code near MC Update & HDD load for early ROMs not supporting SkipHdd argument
    0x0c000000, // jal 	 CheckMcUpdate
    0x0220282d, // daddu a1, s1, zero
    0x3c04002a, // lui	 a0, 0x002a         #SkipHdd jump must be here
    0x0000282d, // daddu a1, zero, zero 	#arg1: 0
    0x24840000, // addiu a0, a0, 0xXXXX  	#arg0: "rom0:ATAD"
    0x0c000000, // jal 	 LoadModule
    0x0000302d, // daduu a2, zero, zero		#arg2: 0
    0x04400000  // bltz  v0, Exit_HddLoad
};
static uint32_t patternHDDLoad_mask[] = {0xfc000000, 0xffffffff, 0xffffffff, 0xffffffff, 0xffff0000, 0xfc000000, 0xffffffff, 0xffff0000};

static uint32_t patternExecuteDiscProto[] = {
    // ExecuteDisc function on protokernel
    0x27bdfff0, //    addiu	sp, sp, $fff0
    0x8c430000, //    lw	v1, $XXXX(v0)
    0xffbf0000, //    sd	ra, $0000(sp)
    0x8c640000, //    lw	a0, $XXXX(v1)
    0x24830002, //    addiu	v1, a0, 2
    0x2c620006, //    sltiu	v0, v1, 6
    0x10400000, //    beq	v0, zero, quit
    0x3c020000, //    lui	v0, xxxx
    0x00031880, //	  sll	v1, v1, 2
    0x24420000  //    addiu	v0, v0, xxxx
};
static uint32_t patternExecuteDiscProto_mask[] = {0xffffffff, 0xffff0000, 0xffffffff, 0xffff0000, 0xffffffff, //
                                                  0xffffffff, 0xffff0000, 0xffff0000, 0xffffffff, 0xffff0000};

//
// The following patterns are introduced in FMCB 1.9 and found by reverse-engineering the FMCB 1.9 code
//

// Pattern for patching the draw functions for selected/unselected items
static uint32_t patternDrawMenuItem_Proto[] = {
    0x240401ae, // addiu a0,zero,0x01AE
    0x0220282d, // daddu a1,s1,zero
    0x24060000, // addiu a2,zero,0x0000
    0x01124021, // addu  t0,t0,s2
    0x0c180f26, // jal   DrawNonSelectableItem
    0x0280382d, // daddu a3,s4,zero
};
static uint32_t patternDrawMenuItem_Proto_mask[] = {0xffffffff, 0xffffffff, 0xfc1f0000, 0xffffffff, 0xffffffff, 0xffffffff};

// Pattern for finding OSD menu info struct
static uint32_t patternMenuInfo_Proto[] = {
    0x00000000, // pointer to osdmenu
    0x00000002, // number of entries
    0x00000003, // unknown
    0x00000000, // current selection
};
static uint32_t patternMenuInfo_Proto_mask[] = {0xff800000, 0xffffffff, 0xffffffff, 0xffffffff};

// Patterns for patching the draw functions for bottom button prompts
static uint32_t patternDrawButtonPanel_2_Proto[] = {
    0x0280302d, // daddu a2,s4,zero
    0x8e640000, // lw    a0,0x0000,s3
    0x8e450000, // lw    a1,0x0000,s2
    0x0c000000, // jal 	 DrawIcon
    0x02a0382d, // daddu a3,s5,zero
};
static uint32_t patternDrawButtonPanel_2_Proto_mask[] = {0xffffffff, 0xffffffff, 0xffffffff, 0xfc000000, 0xffffffff};

static uint32_t patternDrawButtonPanel_3_Proto[] = {
    0x8e440000, // lw    a0,0x0000,s2
    0x0280282d, // daddu a1,s4,zero
    0x26e6f480, // addiu a2,s7,0xF480
    0x02a0382d, // daddu a3,s5,zero
    0x009e2021, // addu  a0,a0,fp
    0x0c000000, // jal   DrawNonSelectableItem
    0x0220402d, // daddu t0,s1,zero
};
static uint32_t patternDrawButtonPanel_3_Proto_mask[] = {0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xfc000000, 0xffffffff};

// Pattern for patching the menu scrolling behavior. Uses patternMenuLoop_mask.
static uint32_t patternMenuLoop_Proto[] = {0x30621000, 0x10400007, 0x2604ffe4, 0x8c830014, 0x2462ffff, 0x0441000e, 0xac820014};
#endif

#endif
