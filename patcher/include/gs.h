// Defines GS register operations and macros for assembling and sending GS packets
// Based on code by Tony Saveski, t_saveski@yahoo.com
#ifndef _GS_H_
#define _GS_H_

#include <kernel.h>
#include <stdint.h>

typedef enum {
  GS_MODE_NTSC = 0x02,
  GS_MODE_PAL,

  GS_MODE_DTV_480P = 0x50,
  GS_MODE_DTV_1080I
} GSVideoMode;

// Resets and initializes GS
int gsInit(GSVideoMode vmode);

// Draws black rectangle
void gsClearScreen();

// Transfers raw bitmap image to GS' memory and draws it on screen at specified coordinates
void gsPrintBitmap(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint32_t *data);

//---------------------------------------------------------------------------
// GS_PACKET macros
//---------------------------------------------------------------------------
#define DECLARE_GS_PACKET(NAME, ITEMS)                                                                                                               \
  uint64_t __attribute__((aligned(64))) NAME[ITEMS * 2 + 2];                                                                                         \
  int NAME##_cur;                                                                                                                                    \
  int NAME##_dma_size

#define BEGIN_GS_PACKET(NAME) NAME##_cur = 0

#define GIF_TAG(NAME, NLOOP, EOP, PRE, PRIM, FLG, NREG, REGS)                                                                                        \
  NAME##_dma_size = NLOOP + 1;                                                                                                                       \
  NAME[NAME##_cur++] = ((uint64_t)(NLOOP) << 0) | ((uint64_t)(EOP) << 15) | ((uint64_t)(PRE) << 46) | ((uint64_t)(PRIM) << 47) |                     \
                       ((uint64_t)(FLG) << 58) | ((uint64_t)(NREG) << 60);                                                                           \
  NAME[NAME##_cur++] = (uint64_t)REGS

#define GIF_TAG_AD(NAME, NLOOP, EOP, PRE, PRIM, FLG) GIF_TAG(NAME, NLOOP, EOP, PRE, PRIM, FLG, 1, 0x0e)

#define GIF_TAG_IMG(NAME, QSIZE)                                                                                                                     \
  GIF_TAG(NAME, (QSIZE), 1, 0, 0, 2, 0, 0);                                                                                                          \
  NAME##_dma_size = 1

#define GIF_DATA_AD(NAME, REG, DAT)                                                                                                                  \
  NAME[NAME##_cur++] = (uint64_t)DAT;                                                                                                                \
  NAME[NAME##_cur++] = (uint64_t)REG

#define SEND_GS_PACKET(NAME)                                                                                                                         \
  FlushCache(0);                                                                                                                                     \
  SET_QWC(GIF_QWC, NAME##_dma_size);                                                                                                                 \
  SET_MADR(GIF_MADR, NAME, 0);                                                                                                                       \
  SET_CHCR(GIF_CHCR, 1, 0, 0, 0, 0, 1, 0);                                                                                                           \
  DMA_WAIT(GIF_CHCR)

//---------------------------------------------------------------------------
// GS Privileged Registers
//---------------------------------------------------------------------------
#define GS_REG_PMODE ((volatile uint64_t *)(0x12000000))    // Setup CRT Controller
#define GS_REG_SMODE2 ((volatile uint64_t *)(0x12000020))   // CRTC display settings
#define GS_REG_DISPFB2 ((volatile uint64_t *)(0x12000090))  // Setup the CRTC's Read Circuit 2 data source settings
#define GS_REG_DISPLAY2 ((volatile uint64_t *)(0x120000a0)) // RC2 display output settings
#define GS_REG_BGCOLOR ((volatile uint64_t *)(0x120000e0))  // Set CRTC background color
#define GS_REG_CSR ((volatile uint64_t *)(0x12001000))      // System status and reset

//---------------------------------------------------------------------------
// PMODE Register
//---------------------------------------------------------------------------
#define GS_SET_PMODE(EN1, EN2, MMOD, AMOD, SLBG, ALP)                                                                                                \
  *GS_REG_PMODE = ((uint64_t)(EN1) << 0) | ((uint64_t)(EN2) << 1) | ((uint64_t)(001) << 2) | ((uint64_t)(MMOD) << 5) | ((uint64_t)(AMOD) << 6) |     \
                  ((uint64_t)(SLBG) << 7) | ((uint64_t)(ALP) << 8)
//---------------------------------------------------------------------------
// SMODE2 Register
//---------------------------------------------------------------------------
#define GS_SET_SMODE2(INT, FFMD, DPMS)                                                                                                               \
  *GS_REG_SMODE2 = (u64)((INT) & 0x00000001) << 0 | (u64)((FFMD) & 0x00000001) << 1 | (u64)((DPMS) & 0x00000003) << 2
//---------------------------------------------------------------------------
// DISPFP2 Register
//---------------------------------------------------------------------------
#define GS_SET_DISPFB2(FBP, FBW, PSM, DBX, DBY)                                                                                                      \
  *GS_REG_DISPFB2 = ((uint64_t)(FBP) << 0) | ((uint64_t)(FBW) << 9) | ((uint64_t)(PSM) << 15) | ((uint64_t)(DBX) << 32) | ((uint64_t)(DBY) << 43)
//---------------------------------------------------------------------------
// DISPLAY2 Register
//---------------------------------------------------------------------------
#define GS_SET_DISPLAY2(DX, DY, MAGH, MAGV, DW, DH)                                                                                                  \
  *GS_REG_DISPLAY2 = ((uint64_t)(DX) << 0) | ((uint64_t)(DY) << 12) | ((uint64_t)(MAGH) << 23) | ((uint64_t)(MAGV) << 27) | ((uint64_t)(DW) << 32) | \
                     ((uint64_t)(DH) << 44)
//---------------------------------------------------------------------------
// BGCOLOR Register
//---------------------------------------------------------------------------
#define GS_SET_BGCOLOR(R, G, B) *GS_REG_BGCOLOR = ((uint64_t)(R) << 0) | ((uint64_t)(G) << 8) | ((uint64_t)(B) << 16)

//
//
//

//---------------------------------------------------------------------------
// GS General Purpose Registers
//---------------------------------------------------------------------------
#define GS_REG_PRIM 0x00       // Select and configure current drawing primitive
#define GS_REG_RGBAQ 0x01      // Setup current vertex color
#define GS_REG_XYZ2 0x05       // Set vertex coordinate and 'kick' drawing
#define GS_REG_XYOFFSET_1 0x18 // Mapping from Primitive to Window coordinate system (Context 1)
#define GS_REG_SCISSOR_1 0x40  // Setup clipping rectangle (Context 1)
#define GS_REG_FRAME_1 0x4c    // Frame buffer settings (Context 1)
#define GS_REG_BITBLTBUF 0x50  // Setup Image Transfer Between EE and GS
#define GS_REG_TRXPOS 0x51     // Setup Image Transfer Coordinates
#define GS_REG_TRXREG 0x52     // Setup Image Transfer Size
#define GS_REG_TRXDIR 0x53     // Set Image Transfer Directon + Start Transfer

//---------------------------------------------------------------------------
// PRIM Register
//---------------------------------------------------------------------------
#define PRIM_POINT 0
#define PRIM_LINE 1
#define PRIM_LINE_STRIP 2
#define PRIM_TRI 3
#define PRIM_TRI_STRIP 4
#define PRIM_TRI_FAN 5
#define PRIM_SPRITE 6
#define GS_PRIM(PRI, IIP, TME, FGE, ABE, AA1, FST, CTXT, FIX)                                                                                        \
  (((uint64_t)(PRI) << 0) | ((uint64_t)(IIP) << 3) | ((uint64_t)(TME) << 4) | ((uint64_t)(FGE) << 5) | ((uint64_t)(ABE) << 6) |                      \
   ((uint64_t)(AA1) << 7) | ((uint64_t)(FST) << 8) | ((uint64_t)(CTXT) << 9) | ((uint64_t)(FIX) << 10))
//---------------------------------------------------------------------------
// RGBAQ Register
//---------------------------------------------------------------------------
#define GS_RGBAQ(R, G, B, A, Q) (((uint64_t)(R) << 0) | ((uint64_t)(G) << 8) | ((uint64_t)(B) << 16) | ((uint64_t)(A) << 24) | ((uint64_t)(Q) << 32))
//---------------------------------------------------------------------------
// XYZ2 Register
//---------------------------------------------------------------------------
#define GS_XYZ2(X, Y, Z) (((uint64_t)(X) << 0) | ((uint64_t)(Y) << 16) | ((uint64_t)(Z) << 32))
//---------------------------------------------------------------------------
// XYOFFSET_x Register
//---------------------------------------------------------------------------
#define GS_XYOFFSET(OFX, OFY) (((uint64_t)(OFX) << 0) | ((uint64_t)(OFY) << 32))
//---------------------------------------------------------------------------
// SCISSOR_x Register
//---------------------------------------------------------------------------
#define GS_SCISSOR(X0, X1, Y0, Y1) (((uint64_t)(X0) << 0) | ((uint64_t)(X1) << 16) | ((uint64_t)(Y0) << 32) | ((uint64_t)(Y1) << 48))
//---------------------------------------------------------------------------
// FRAME_x Register
//---------------------------------------------------------------------------
#define GS_FRAME(FBP, FBW, PSM, FBMSK) (((uint64_t)(FBP) << 0) | ((uint64_t)(FBW) << 16) | ((uint64_t)(PSM) << 24) | ((uint64_t)(FBMSK) << 32))
//---------------------------------------------------------------------------
// BITBLTBUF Register - Setup Image Transfer Between EE and GS
//   SBP  - Source buffer address (Address/64)
//   SBW  - Source buffer width (Pixels/64)
//   SPSM - Source pixel format (0 = 32bit RGBA)
//   DBP  - Destination buffer address (Address/64)
//   DBW  - Destination buffer width (Pixels/64)
//   DPSM - Destination pixel format (0 = 32bit RGBA)
//
// - When transferring from EE to GS, only the Detination fields
//   need to be set. (Only Source fields for GS->EE, and all for GS->GS).
//---------------------------------------------------------------------------
#define GS_BITBLTBUF(SBP, SBW, SPSM, DBP, DBW, DPSM)                                                                                                 \
  (((uint64_t)(SBP) << 0) | ((uint64_t)(SBW) << 16) | ((uint64_t)(SPSM) << 24) | ((uint64_t)(DBP) << 32) | ((uint64_t)(DBW) << 48) |                 \
   ((uint64_t)(DPSM) << 56))
//---------------------------------------------------------------------------
// TRXPOS Register - Setup Image Transfer Coordinates
//   SSAX - Source Upper Left X
//   SSAY - Source Upper Left Y
//   DSAX - Destionation Upper Left X
//   DSAY - Destionation Upper Left Y
//   DIR  - Pixel Transmission Order (00 = top left -> bottom right)
//
// - When transferring from EE to GS, only the Detination fields
//   need to be set. (Only Source fields for GS->EE, and all for GS->GS).
//---------------------------------------------------------------------------
#define GS_TRXPOS(SSAX, SSAY, DSAX, DSAY, DIR)                                                                                                       \
  (((uint64_t)(SSAX) << 0) | ((uint64_t)(SSAY) << 16) | ((uint64_t)(DSAX) << 32) | ((uint64_t)(DSAY) << 48) | ((uint64_t)(DIR) << 59))
//---------------------------------------------------------------------------
// TRXREG Register - Setup Image Transfer Size
//   RRW - Image Width
//   RRH - Image Height
//---------------------------------------------------------------------------
#define GS_TRXREG(RRW, RRH) (((uint64_t)(RRW) << 0) | ((uint64_t)(RRH) << 32))
//---------------------------------------------------------------------------
// TRXDIR Register - Set Image Transfer Directon, and Start Transfer
//   XDIR - (0=EE->GS, 1=GS->EE, 2=GS-GS, 3=Transmission is deactivated)
//---------------------------------------------------------------------------
#define XDIR_EE_GS 0
#define XDIR_GS_EE 1
#define XDIR_GS_GS 2
#define XDIR_DEACTIVATE 3

#define GS_TRXDIR(XDIR) ((uint64_t)(XDIR))

//
//
//

//---------------------------------------------------------------------------
// DMA Channel Registers
//---------------------------------------------------------------------------
#define GS_REG_GIF_CHCR 0x1000a000 // GIF Channel Control Register
#define GS_REG_GIF_MADR 0x1000a010 // Transfer Address Register
#define GS_REG_GIF_QWC 0x1000a020  // Transfer Size Register (in qwords)
//---------------------------------------------------------------------------
// CHCR Register - Channel Control Register
//---------------------------------------------------------------------------
#define GIF_CHCR ((volatile uint32_t *)(GS_REG_GIF_CHCR))
#define SET_CHCR(WHICH, DIR, MOD, ASP, TTE, TIE, STR, TAG)                                                                                           \
  *WHICH = ((uint32_t)(DIR) << 0) | ((uint32_t)(MOD) << 2) | ((uint32_t)(ASP) << 4) | ((uint32_t)(TTE) << 6) | ((uint32_t)(TIE) << 7) |              \
           ((uint32_t)(STR) << 8) | ((uint32_t)(TAG) << 16)
#define DMA_WAIT(WHICH) while ((*WHICH) & (1 << 8))
//---------------------------------------------------------------------------
// MADR Register - Transfer Address Register
//---------------------------------------------------------------------------
#define GIF_MADR ((volatile uint32_t *)(GS_REG_GIF_MADR))
#define SET_MADR(WHICH, ADDR, SPR) *WHICH = ((uint32_t)(ADDR) << 0) | ((uint32_t)(SPR) << 31)
//---------------------------------------------------------------------------
// QWC Register - Transfer Data Size Register
//---------------------------------------------------------------------------
#define GIF_QWC ((volatile uint32_t *)(GS_REG_GIF_QWC))
#define SET_QWC(WHICH, SIZE) *WHICH = (uint32_t)(SIZE)

#endif
