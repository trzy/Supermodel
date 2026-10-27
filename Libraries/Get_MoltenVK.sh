#!/bin/sh
# Download the official macOS runtime and headers without Homebrew.
set -eu

library_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
version=1.4.2
archive="$library_dir/MoltenVK-macos-$version.tar"
destination="$library_dir/MoltenVK-$version"
sha256=f95765a6229cb7b915990a2890ce12ebe36a730b021545d3d52ae69ce4c4024e
url="https://github.com/KhronosGroup/MoltenVK/releases/download/v$version/MoltenVK-macos.tar"

if [ -f "$destination/.complete" ] && [ -f "$destination/libMoltenVK.dylib" ] &&
   [ -f "$destination/LICENSE" ] && [ -f "$destination/NOTICE.txt" ] &&
   [ -f "$destination/include/vulkan/vulkan.h" ]; then
    touch "$destination/.complete"
    printf 'Using MoltenVK %s in %s\n' "$version" "$destination"
    exit 0
fi

if [ ! -f "$archive" ]; then
    printf 'Downloading MoltenVK %s...\n' "$version"
    curl --fail --location --retry 3 --output "$archive.part" "$url"
    mv "$archive.part" "$archive"
fi

if command -v shasum >/dev/null 2>&1; then
    printf '%s  %s\n' "$sha256" "$archive" | shasum -a 256 -c -
else
    printf '%s  %s\n' "$sha256" "$archive" | sha256sum -c -
fi

mkdir -p "$destination"
tar -xOf "$archive" MoltenVK/MoltenVK/dynamic/dylib/macOS/libMoltenVK.dylib > "$destination/libMoltenVK.dylib.part"
tar -xOf "$archive" MoltenVK/LICENSE > "$destination/LICENSE.part"
tar -xf "$archive" -C "$destination" --strip-components=2 MoltenVK/MoltenVK/include
mv "$destination/libMoltenVK.dylib.part" "$destination/libMoltenVK.dylib"
mv "$destination/LICENSE.part" "$destination/LICENSE"
printf 'MoltenVK %s\nSource: %s\nArchive SHA256: %s\n' "$version" "$url" "$sha256" > "$destination/NOTICE.txt"
touch "$destination/.complete"
printf 'MoltenVK runtime, headers and license ready in %s\n' "$destination"
