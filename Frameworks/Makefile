include ../Makefiles/Options.inc

DELETE = rm -d -r -f

all: SDL2_net.framework SDL2.framework

SDL2.dmg:
	$(info Downloading Framework  : SDL2.dmg)
	$(SILENT)curl https://github.com/libsdl-org/SDL/releases/download/release-2.30.1/SDL2-2.30.1.dmg -sfL --output $@
	$(SILENT)touch $@

SDL2_net.dmg:
	$(info Downloading Framework  : SDL2_net.dmg)
	$(SILENT)curl https://github.com/libsdl-org/SDL_net/releases/download/release-2.2.0/SDL2_net-2.2.0.dmg -sfL --output $@
	$(SILENT)touch $@

SDL2.framework: SDL2.dmg
	$(info Extracting Framework   : SDL2)
	$(SILENT)hdiutil attach -quiet SDL2.dmg
	$(SILENT)cp -r /Volumes/SDL2/SDL2.framework ./
	$(SILENT)xattr -dr com.apple.quarantine SDL2.framework
	$(SILENT)hdiutil detach -quiet /Volumes/SDL2
	$(SILENT)touch $@

SDL2_net.framework: SDL2_net.dmg
	$(info Extracting Framework   : SDL2_net)
	$(SILENT)hdiutil attach -quiet SDL2_net.dmg
	$(SILENT)cp -r /Volumes/SDL2_net/SDL2_net.framework ./
	$(SILENT)xattr -dr com.apple.quarantine SDL2_net.framework
	$(SILENT)hdiutil detach -quiet /Volumes/SDL2_net
	$(SILENT)touch $@

clean:
	$(SILENT)$(DELETE) SDL2.* SDL2_net.*
