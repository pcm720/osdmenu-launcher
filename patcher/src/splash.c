#include "splash.h"
#include "gs.h"
#include "splash_bmp.h"

// Initializes GS and displays FMCB splash screen
void gsDisplaySplash(GSVideoMode mode) {
  int splashY = 185;
  if (mode == GS_MODE_PAL) {
    splashY = 247;
  }

  gsInit(mode);
  gsClearScreen();
  gsPrintBitmap((640 - splashWidth) / 2, splashY, splashWidth, splashHeight, splash);
}
