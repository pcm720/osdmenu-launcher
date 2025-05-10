.PHONY: all clean

all: osdmenu hosdmenu launcher
osdmenu: osdmenu.elf
hosdmenu: hosdmenu.elf
launcher: launcher.elf

clean:
	rm osdmenu.elf hosdmenu.elf launcher.elf
	$(MAKE) -C patcher clean
	$(MAKE) -C launcher clean

# OSDSYS Patcher
osdmenu.elf:
	$(MAKE) -C patcher clean
	$(MAKE) -C patcher/$<
	cp patcher/patcher.elf osdmenu.elf

# HDD OSD Patcher
hosdmenu.elf:
	$(MAKE) -C patcher clean
	$(MAKE) -C patcher/$< HOSD=1
	cp patcher/patcher.elf hosdmenu.elf

# Launcher
launcher.elf:
	$(MAKE) -C launcher/$<
	cp launcher/launcher.elf launcher.elf

