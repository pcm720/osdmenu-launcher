# DISCLAIMER: By disabling the Free McBoot splash screen, the user acknowledges
# and agrees that they are in violation of Rule 2 of the Free McBoot License.
# The user accepts full responsibility for any legal actions that may be taken
# as a result of this violation, including but not limited to being sued.
# Proceed at your own risk, and may the legal forces be ever in your favor.
ENABLE_SPLASH ?= 1

GIT_VERSION := $(shell git describe --always --dirty --tags --exclude nightly)

EE_BIN_PKD = patcher.elf
EE_BIN = patcher_unc.elf

EE_INCS = -I../common -I$(PS2SDK)/ee/include -I$(PS2SDK)/common/include -I$(PS2SDK)/sbv/include -Iinclude -I$(PS2SDK)/ports/include

EE_LINKFILE = linkfile
EE_LIBS = -lpatches

EE_OBJS = main.o settings.o init.o loader.o fmcb_patches.o osdmenu_patches.o

# C compiler flags
EE_CFLAGS := -D_EE -O2 -G0 -Wall $(EE_CFLAGS) -DGIT_VERSION="\"${GIT_VERSION}\""
EE_LDFLAGS += -Wl,-zmax-page-size=128 -Wl,--gc-sections -s
EE_CFLAGS += -fdata-sections -ffunction-sections

# Reduce binary size by using newlib-nano
EE_NEWLIB_NANO = 1
NEWLIB_NANO = 1

ifeq ($(ENABLE_SPLASH), 1)
 EE_OBJS += splash.o
 EE_CFLAGS += -DENABLE_SPLASH
 EE_LIBS += -ldma
endif

ifdef CONF_PATH
 EE_CFLAGS += -DCONF_PATH=\"$(CONF_PATH)\"
endif
ifdef LAUNCHER_PATH
 EE_CFLAGS += -DLAUNCHER_PATH=\"$(LAUNCHER_PATH)\"
endif
ifdef DKWDRV_PATH
 EE_CFLAGS += -DDKWDRV_PATH=\"$(DKWDRV_PATH)\"
endif

EE_OBJS_DIR = obj/
EE_ASM_DIR = asm/
EE_SRC_DIR = src/

EE_OBJS += $(IRX_FILES:.irx=_irx.o)
EE_OBJS += $(ELF_FILES:.elf=_elf.o)
EE_OBJS := $(EE_OBJS:%=$(EE_OBJS_DIR)%)

.PHONY: all clean

all: $(EE_BIN_PKD)

$(EE_BIN_PKD): $(EE_BIN)
	ps2-packer $< $@

clean:
	rm -rf $(EE_OBJS_DIR) $(EE_BIN) $(EE_BIN_PKD)

BIN2C = $(PS2SDK)/bin/bin2c

# IRX files
%_irx.c:
	$(BIN2C) $(PS2SDK)/iop/irx/$(*:$(EE_SRC_DIR)%=%).irx $@ $(*:$(EE_SRC_DIR)%=%)_irx

$(EE_ASM_DIR):
	@mkdir -p $@

$(EE_OBJS_DIR):
	@mkdir -p $@

$(EE_OBJS_DIR)%.o: $(EE_SRC_DIR)%.c | $(EE_OBJS_DIR)
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
