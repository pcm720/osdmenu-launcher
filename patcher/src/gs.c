// GS functions
// Based on code by Tony Saveski, t_saveski@yahoo.com
#include "gs.h"
#include <dma.h>
#include <kernel.h>
#include <malloc.h>
#include <stdint.h>
#include <string.h>

typedef struct {
  GSVideoMode mode;
  uint16_t width;
  uint16_t height;
  uint16_t psm;
  uint16_t bpp;
  uint16_t magh;
} vmode_t;

vmode_t vmodes[] = {
    {GS_MODE_PAL, 640, 512, 0, 32, 4},  // PAL
    {GS_MODE_NTSC, 640, 448, 0, 32, 4}, // NTSC
};

// Max coordinates for currently initialized video mode
static uint16_t gsMaxX = 0;
static uint16_t gsMaxY = 0;

DECLARE_GS_PACKET(gsDMABuf, 50);

// Resets and initializes GS
int gsInit(GSVideoMode vmode) {
  vmode_t *mode;

  if (vmode == GS_MODE_PAL)
    mode = &vmodes[0];
  else
    mode = &vmodes[1];

  gsMaxX = mode->width - 1;
  gsMaxY = mode->height - 1;

  // Reset DMA
  dma_reset();
  // Reset GS
  *(volatile uint64_t *)GS_REG_CSR = 0x200;
  // Mask interrupts
  GsPutIMR(0xff00);
  // Configure GS CRT
  SetGsCrt(1, mode->mode, 0);

  GS_SET_PMODE(0,   // ReadCircuit1 OFF
               1,   // ReadCircuit2 ON
               1,   // Use ALP register for Alpha Blending
               1,   // Alpha Value of ReadCircuit2 for output selection
               0,   // Blend Alpha with the output of ReadCircuit2
               0xFF // Alpha Value = 1.0
  );

  GS_SET_DISPFB2(0,                // Frame Buffer base pointer = 0 (Address/2048)
                 mode->width / 64, // Buffer Width (Address/64)
                 mode->psm,        // Pixel Storage Format
                 0,                // Upper Left X in Buffer = 0
                 0                 // Upper Left Y in Buffer = 0
  );

  GS_SET_DISPLAY2(656,                          // X position in the display area (in VCK units)
                  36,                           // Y position in the display area (in Raster units)
                  mode->magh - 1,               // Horizontal Magnification - 1
                  0,                            // Vertical Magnification = 1x
                  mode->width * mode->magh - 1, // Display area width  - 1 (in VCK units) (Width*HMag-1)
                  mode->height - 1              // Display area height - 1 (in pixels)	  (Height-1)
  );

  GS_SET_BGCOLOR(0, 0, 0);

  BEGIN_GS_PACKET(gsDMABuf);

  GIF_TAG_AD(gsDMABuf, 3, 1, 0, 0, 0);
  GIF_DATA_AD(gsDMABuf, GS_REG_FRAME_1,
              GS_FRAME(0,                // FrameBuffer base pointer = 0 (Address/2048)
                       mode->width / 64, // Frame buffer width (Pixels/64)
                       mode->psm,        // Pixel Storage Format
                       0));

  // No displacement between Primitive and Window coordinate systems.
  GIF_DATA_AD(gsDMABuf, GS_REG_XYOFFSET_1, GS_XYOFFSET(0x0, 0x0));
  // Clip to frame buffer.
  GIF_DATA_AD(gsDMABuf, GS_REG_SCISSOR_1, GS_SCISSOR(0, gsMaxX, 0, gsMaxY));

  SEND_GS_PACKET(gsDMABuf);

  gsClearScreen();

  return 1;
}

// Draws black rectangle
void gsClearScreen() {
  BEGIN_GS_PACKET(gsDMABuf);

  GIF_TAG_AD(gsDMABuf, 4, 1, 0, 0, 0);
  GIF_DATA_AD(gsDMABuf, GS_REG_PRIM, GS_PRIM(PRIM_SPRITE, 0, 0, 0, 0, 0, 0, 0, 0));
  GIF_DATA_AD(gsDMABuf, GS_REG_RGBAQ, GS_RGBAQ(0, 0, 0, 0, 0));
  GIF_DATA_AD(gsDMABuf, GS_REG_XYZ2, GS_XYZ2(0, 0, 0));
  GIF_DATA_AD(gsDMABuf, GS_REG_XYZ2, GS_XYZ2((gsMaxX + 1) << 4, (gsMaxY + 1) << 4, 0));

  SEND_GS_PACKET(gsDMABuf);
}

#define MAX_TRANSFER 16384

// Transfers raw bitmap image to GS' memory and draws it on screen at specified coordinates
void gsPrintBitmap(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint32_t *data) {
  uint32_t i;       // DMA buffer loop counter
  uint32_t frac;    // flag for whether to run a fractional buffer or not
  uint32_t current; // number of pixels to transfer in current DMA
  uint32_t qtotal;  // total number of qwords of data to transfer

  BEGIN_GS_PACKET(gsDMABuf);
  GIF_TAG_AD(gsDMABuf, 4, 1, 0, 0, 0);
  GIF_DATA_AD(gsDMABuf, GS_REG_BITBLTBUF,
              GS_BITBLTBUF(0, 0, 0,
                           0,                 // frame buffer address
                           (gsMaxX + 1) / 64, // frame buffer width
                           0));
  GIF_DATA_AD(gsDMABuf, GS_REG_TRXPOS, GS_TRXPOS(0, 0, x, y, 0)); // left to right/top to bottom
  GIF_DATA_AD(gsDMABuf, GS_REG_TRXREG, GS_TRXREG(w, h));
  GIF_DATA_AD(gsDMABuf, GS_REG_TRXDIR, GS_TRXDIR(XDIR_EE_GS));
  SEND_GS_PACKET(gsDMABuf);

  qtotal = w * h / 4;              // total number of quadwords to transfer.
  current = qtotal % MAX_TRANSFER; // work out if a partial buffer transfer is needed.
  frac = 1;                        // assume yes.
  if (!current)                    // if there is no need for partial buffer
  {
    current = MAX_TRANSFER; // start with a full buffer
    frac = 0;               // and don't do extra partial buffer first
  }
  for (i = 0; i < (qtotal / MAX_TRANSFER) + frac; i++) {
    BEGIN_GS_PACKET(gsDMABuf);
    GIF_TAG_IMG(gsDMABuf, current);
    SEND_GS_PACKET(gsDMABuf);

    SET_QWC(GIF_QWC, current);
    SET_MADR(GIF_MADR, data, 0);
    SET_CHCR(GIF_CHCR, 1, 0, 0, 0, 0, 1, 0);
    DMA_WAIT(GIF_CHCR);

    data += current * 4;
    current = MAX_TRANSFER; // after the first one, all are full buffers
  }
}
