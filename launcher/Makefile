EE_BIN_PKD = launcher.elf
EE_BIN = launcher_unc.elf

EE_OBJS = main.o common.o init.o loader.o
EE_OBJS += handler_bdm.o
EE_OBJS += handler_cdrom.o history.o game_id.o
EE_OBJS += handler_fmcb.o
EE_OBJS += handler_mc.o
EE_OBJS += handler_pfs.o

EE_LDFLAGS = -s
EE_LIBS = -lcdvd -lpatches -lkernel -ldebug -lfileXio -lgskit -ldmakit -lmc -lpoweroff
# Basic modules
IRX_FILES += iomanX.irx fileXio.irx poweroff.irx
# BDM
IRX_FILES += bdm.irx bdmfs_fatfs.irx
# MC/MMCE/MX4SIO
IRX_FILES += sio2man.irx mcman.irx mcserv.irx
IRX_FILES += mmceman.irx
IRX_FILES += mx4sio_bd_mini.irx
# USB
IRX_FILES += usbd_mini.irx usbmass_bd_mini.irx
# DEV9
IRX_FILES += ps2dev9.irx
IRX_FILES += ata_bd.irx ps2atad.irx ps2hdd.irx ps2fs.irx
IRX_FILES += smap_udpbd.irx
IRX_FILES += iLinkman.irx IEEE1394_bd_mini.irx
# XPARAM
IRX_FILES += xparam.irx

ELF_FILES += loader.elf
RES_FILES += icon_A.sys icon_C.sys icon_J.sys

# C compiler flags
EE_CFLAGS := -D_EE -O2 -G0 -Wall $(EE_CFLAGS)

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
EE_INCS = -I../common -I$(PS2SDK)/ee/include -I$(PS2SDK)/common/include -I$(PS2SDK)/sbv/include -Iinclude -I$(PS2SDK)/ports/include -I$(PS2DEV)/gsKit/include
EE_LDFLAGS += -L$(PS2DEV)/gsKit/lib

EE_OBJS += $(IRX_FILES:.irx=_irx.o)
EE_OBJS += $(ELF_FILES:.elf=_elf.o)
EE_OBJS += $(RES_FILES:.sys=_sys.o)
EE_OBJS := $(EE_OBJS:%=$(EE_OBJS_DIR)%)

.PHONY: all clean

all: $(EE_BIN_PKD)

$(EE_BIN_PKD): $(EE_BIN)
	ps2-packer $< $@

clean:
	$(MAKE) -C loader clean
	$(MAKE) -C iop/xparam clean
	$(MAKE) -C iop/smap_udpbd clean
	rm -rf $(EE_OBJS_DIR) $(EE_BIN) $(EE_BIN_PKD)

BIN2C = $(PS2SDK)/bin/bin2c

# IRX files
%_irx.c:
	$(BIN2C) $(PS2SDK)/iop/irx/$(*:$(EE_SRC_DIR)%=%).irx $@ $(*:$(EE_SRC_DIR)%=%)_irx

# smap_udpbd.irx
iop/smap_udpbd/smap_udpbd.irx: iop/smap_udpbd
	$(MAKE) -C $<

%smap_udpbd_irx.c: iop/smap_udpbd/smap_udpbd.irx
	$(BIN2C) iop/smap_udpbd/$(*:$(EE_SRC_DIR)%=%)smap_udpbd.irx $@ $(*:$(EE_SRC_DIR)%=%)smap_udpbd_irx

# xparam.irx
iop/xparam/xparam.irx: iop/xparam
	$(MAKE) -C $<

%xparam_irx.c: iop/xparam/xparam.irx
	$(BIN2C) iop/xparam/$(*:$(EE_SRC_DIR)%=%)xparam.irx $@ $(*:$(EE_SRC_DIR)%=%)xparam_irx

# mmceman module, temporary override until it lands in the SDK
%mmceman_irx.c:
	$(BIN2C) iop/mmceman/mmceman.irx $@ $(*:$(EE_SRC_DIR)%=%)mmceman_irx

# ELF loader
loader.elf:
	$(MAKE) -C loader/$<

%loader_elf.c: loader.elf
	$(BIN2C) $(*:$(EE_SRC_DIR)%=loader/%)loader.elf $@ $(*:$(EE_SRC_DIR)%=%)loader_elf

# Resource files
%_sys.c:
	$(BIN2C) res/$(*:$(EE_SRC_DIR)%=%).sys $@ $(*:$(EE_SRC_DIR)%=%)_sys

$(EE_ASM_DIR):
	@mkdir -p $@

$(EE_OBJS_DIR):
	@mkdir -p $@

$(EE_OBJS_DIR)%.o: $(EE_SRC_DIR)%.c | $(EE_OBJS_DIR)
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
