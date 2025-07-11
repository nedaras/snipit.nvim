TARGET ?= Debug
OPTIMIZE_FLAG =

ifeq ($(TARGET), Release)
    OPTIMIZE_FLAG = -Doptimize=ReleaseSmall
endif

all: linux macos windows

linux:
	zig build -Dtarget=x86_64-linux-gnu $(OPTIMIZE_FLAG)
	@cp zig-out/lib/libsnipit.so lib/x86_64-linux-snipit.so

macos:
	zig build -Dtarget=aarch64-macos $(OPTIMIZE_FLAG)
	@cp zig-out/lib/libsnipit.dylib lib/aarch4-macos-snipit.dylib

windows:
	zig build -Dtarget=x86_64-windows-gnu $(OPTIMIZE_FLAG)
	@cp zig-out/bin/snipit.dll lib/x86_64-windows-snipit.dll

clean:
	@rm -rf zig-out lib/x86_64-linux-snipit.so lib/x86_64-windows-snipit.dll lib/aarch64-macos-snipit.dylib

