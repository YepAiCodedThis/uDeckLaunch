VERSION_MAJOR	:=	1
VERSION_MINOR	:=	2
VERSION_MICRO	:=	5
export VERSION	:=	$(VERSION_MAJOR).$(VERSION_MINOR).$(VERSION_MICRO)

export UL_DEFS	:=	-DUL_MAJOR=$(VERSION_MAJOR) -DUL_MINOR=$(VERSION_MINOR) -DUL_MICRO=$(VERSION_MICRO) -DUL_VERSION=\"$(VERSION)\"

OUT_DIR				:=	SdOut
OUT_DIR_ZIP			:=	uLaunch-v$(VERSION)

.PHONY: all fresh clean libs arc usystem uloader umenu umanager umanager-pack umanager-installer-nro installer package

all: package

fresh: clean all

clean:
	@$(MAKE) clean -C projects/uSystem
	@$(MAKE) clean -C projects/uLoader
	@$(MAKE) clean -C projects/uMenu
	@$(MAKE) clean -C projects/uManager
	@rm -rf $(OUT_DIR)
	@rm -rf $(OUT_DIR_ZIP).7z $(OUT_DIR_ZIP).zip

libs:
	@$(MAKE) -C libs/Plutonium/
	@$(MAKE) -C libs/libnx-ext/libnx-ext/
	@$(MAKE) -C libs/libnx-ext/libnx-ipcext/

arc:
	@python arc/arc.py gen_db default+./libs/uCommon/include/ul/ul_Results.rc.hpp
	@python arc/arc.py gen_cpp rc UL ./libs/uCommon/include/ul/ul_Results.gen.hpp

usystem: arc libs
	@$(MAKE) -C projects/uSystem
	@mkdir -p $(OUT_DIR)/atmosphere/contents/0100000000001000
	@cp projects/uSystem/out/nintendo_nx_arm64_armv8a/release/uSystem.nsp $(OUT_DIR)/atmosphere/contents/0100000000001000/exefs.nsp
	@mkdir -p $(OUT_DIR)/ulaunch/bin/uSystem
	@cp projects/uSystem/out/nintendo_nx_arm64_armv8a/release/uSystem.nsp $(OUT_DIR)/ulaunch/bin/uSystem/exefs.nsp

uloader: arc libs
	@$(MAKE) -C projects/uLoader
	@mkdir -p $(OUT_DIR)/ulaunch/bin/uLoader
	@mkdir -p $(OUT_DIR)/ulaunch/bin/uLoader/applet
	@cp projects/uLoader/uLoader.nso $(OUT_DIR)/ulaunch/bin/uLoader/applet/main
	@cp projects/uLoader/uLoader_applet.npdm $(OUT_DIR)/ulaunch/bin/uLoader/applet/main.npdm
	@mkdir -p $(OUT_DIR)/ulaunch/bin/uLoader/application
	@cp projects/uLoader/uLoader.nso $(OUT_DIR)/ulaunch/bin/uLoader/application/main
	@cp projects/uLoader/uLoader_application.npdm $(OUT_DIR)/ulaunch/bin/uLoader/application/main.npdm

umenu: arc libs
	@$(MAKE) -C projects/uMenu
	@mkdir -p $(OUT_DIR)/ulaunch/bin/uMenu
	@mkdir -p $(OUT_DIR)/ulaunch/lang/uMenu
	@cp projects/uMenu/uMenu.nso $(OUT_DIR)/ulaunch/bin/uMenu/main
	@cp projects/uMenu/uMenu.npdm $(OUT_DIR)/ulaunch/bin/uMenu/main.npdm
	@cp assets/Logo.png projects/uMenu/romfs/Logo.png
	@cp assets/Icon.png projects/uMenu/romfs/Icon.png
	@rm -rf projects/uMenu/romfs/privacy
	@mkdir -p projects/uMenu/romfs/privacy
	@cp assets/privacy/*.jpg projects/uMenu/romfs/privacy/
	@rm -rf projects/uMenu/romfs/default
	@mkdir -p projects/uMenu/romfs/default/ui
	@cp assets/ui/UI.json projects/uMenu/romfs/default/ui/UI.json
	@build_romfs projects/uMenu/romfs $(OUT_DIR)/ulaunch/bin/uMenu/romfs.bin

installer: umenu umanager-pack umanager-installer-nro

umanager-pack:
	@powershell -ExecutionPolicy Bypass -File tools/pack-installer-romfs.ps1

umanager-installer-nro:
	@cp assets/Logo.png projects/uManager/romfs/Logo.png
	@$(MAKE) -C projects/uManager
	@mkdir -p $(OUT_DIR)/switch/uDeckLaunch
	@cp projects/uManager/uManager.nro $(OUT_DIR)/switch/uDeckLaunch/uDeckLaunch.nro
	@echo "Installer: $(OUT_DIR)/switch/uDeckLaunch/uDeckLaunch.nro"

umanager: arc libs
	@cp assets/Logo.png projects/uManager/romfs/Logo.png
	@$(MAKE) -C projects/uManager
	@mkdir -p $(OUT_DIR)/ulaunch/lang/uManager
	@mkdir -p $(OUT_DIR)/switch
	@cp projects/uManager/uManager.nro $(OUT_DIR)/switch/uManager.nro

package: arc usystem uloader umenu umanager
	@rm -rf $(OUT_DIR_ZIP).7z $(OUT_DIR_ZIP).zip
	@cd $(OUT_DIR) && 7z a ../$(OUT_DIR_ZIP).7z atmosphere ulaunch switch
	@cd $(OUT_DIR) && zip -r ../$(OUT_DIR_ZIP).zip atmosphere ulaunch switch
	@echo "Packaged $(OUT_DIR) into $(OUT_DIR_ZIP).7z & $(OUT_DIR_ZIP).zip!"
