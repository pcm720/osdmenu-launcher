# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
# $Id$


IOP_SRC_DIR := src/
IOP_OBJS_DIR := obj/
IOP_INCS := -Isrc/include

IOP_BIN  = ps2atad.irx
IOP_OBJS = ps2atad.o imports.o exports.o

IOP_CFLAGS  += -Wall -fno-builtin
IOP_LDFLAGS += -s

all:: $(IOP_BIN)

clean::
	rm -f -r $(IOP_BIN) $(IOP_OBJS_DIR)

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.iopglobal
