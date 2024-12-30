#!/bin/bash
# Script ran on "22.04.1-Ubuntu"

# Globals
APP_DIR=Supermodel.AppDir

# Build Supermodel (NOTE: only seems to work if we enable NET_BOARD)
cd ..
make -f Makefiles/Makefile.UNIX NET_BOARD=1 APP_IMAGE=1

# Move back to the AppImage folder
cd ./AppImage
mkdir -p ./${APP_DIR}

# Download appimagetool (for creating the image)
wget https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage -c
chmod +x appimagetool-x86_64.AppImage

# Copy binary into folder
mkdir -p ./${APP_DIR}/usr/bin
cp ../bin/supermodel ./${APP_DIR}/usr/bin/Supermodel

# Copy AppImage assets into the folder
cp ./AppRun ./${APP_DIR}/AppRun
cp ./Supermodel.desktop ./${APP_DIR}/Supermodel.desktop
cp ./Supermodel.png ./${APP_DIR}/Supermodel.png

# Copy Supermodel assets
mkdir -p ./${APP_DIR}/usr/Assets
cp ../Assets/* ./${APP_DIR}/usr/Assets/

# Copy Config
mkdir -p ./${APP_DIR}/usr/Config
cp ../Config/* ./${APP_DIR}/usr/Config/

# Copy required libraries
# NOTE: There may be some unnecessary libraries in here
mkdir -p ./${APP_DIR}/usr/lib
cp /lib/x86_64-linux-gnu/libbsd.so.0 ./${APP_DIR}/usr/lib/
cp /lib/x86_64-linux-gnu/libdrm.so.2 ./${APP_DIR}/usr/lib/
cp /lib/x86_64-linux-gnu/libffi.so.8 ./${APP_DIR}/usr/lib/
cp /lib/x86_64-linux-gnu/libFLAC.so.8 ./${APP_DIR}/usr/lib/
cp /lib/x86_64-linux-gnu/libGLX.so.0 ./${APP_DIR}/usr/lib/
cp /lib/x86_64-linux-gnu/liblz4.so.1 ./${APP_DIR}/usr/lib/
cp /lib/x86_64-linux-gnu/liblzma.so.5 ./${APP_DIR}/usr/lib/
cp /lib/x86_64-linux-gnu/libmd.so.0 ./${APP_DIR}/usr/lib/
cp /lib/x86_64-linux-gnu/libogg.so.0 ./${APP_DIR}/usr/lib/
cp /lib/x86_64-linux-gnu/libopus.so.0 ./${APP_DIR}/usr/lib/
cp /lib/x86_64-linux-gnu/libSDL2-2.0.so.0 ./${APP_DIR}/usr/lib/
cp /lib/x86_64-linux-gnu/libSDL2_net-2.0.so.0 ./${APP_DIR}/usr/lib/
cp /lib/x86_64-linux-gnu/libvorbisenc.so.2 ./${APP_DIR}/usr/lib/
cp /lib/x86_64-linux-gnu/libvorbis.so.0 ./${APP_DIR}/usr/lib/
cp /lib/x86_64-linux-gnu/libxcb-randr.so.0 ./${APP_DIR}/usr/lib/
cp /lib/x86_64-linux-gnu/libxcb.so.1 ./${APP_DIR}/usr/lib/
cp /lib/x86_64-linux-gnu/libxkbcommon.so.0 ./${APP_DIR}/usr/lib/

# Build the app-image
./appimagetool-x86_64.AppImage Supermodel.AppDir
