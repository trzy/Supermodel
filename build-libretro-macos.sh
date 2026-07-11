#!/bin/sh -e
# Build the Supermodel libretro core on macOS (arm64), for use in retro-trainer.
#
# Two non-obvious requirements, learned the hard way:
#
#  1. Apple's /usr/bin/clang, NOT Homebrew LLVM. Homebrew libc++ emits a
#     reference to std::__hash_memory (from the libretro throttle map's
#     std::unordered_map) that the system libc++ this links against does not
#     export -> undefined symbol at link. Apple's toolchain is self-consistent.
#
#  2. The dylib resolves SDL2.framework / SDL2_net.framework via @loader_path
#     (its own dir), so both frameworks must sit next to it in lib/. They live
#     in Frameworks/; we symlink them into lib/ if missing.
#
# Usage:  ./build-libretro-macos.sh        (from the repo root)
# Output: lib/supermodel_libretro.dylib

cd "$(dirname "$0")"

# SDL frameworks next to the dylib for runtime @loader_path resolution.
[ -e lib/SDL2.framework ]     || ln -sfn ../Frameworks/SDL2.framework     lib/SDL2.framework
[ -e lib/SDL2_net.framework ] || ln -sfn ../Frameworks/SDL2_net.framework lib/SDL2_net.framework

make -f Makefiles/Makefile.OSX LIBRETRO=1 \
     CC=/usr/bin/clang CXX=/usr/bin/clang LD=/usr/bin/clang \
     lib/supermodel_libretro.dylib -j"$(sysctl -n hw.ncpu)"

echo "Built: $(cd "$(dirname "$0")" && pwd)/lib/supermodel_libretro.dylib"
file lib/supermodel_libretro.dylib
