#!/bin/bash

set -e

BUILD_DIR="build"
CMAKE_TOOLCHAIN_FILE="mingw-w64-toolchain.cmake"

echo "Creating mingw toolchain file..."
cat > $CMAKE_TOOLCHAIN_FILE << 'EOF'
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)

set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
EOF

echo "Creating build directory..."
rm -rf $BUILD_DIR
mkdir -p $BUILD_DIR

echo "Running CMake configuration..."
cd $BUILD_DIR
cmake .. -DCMAKE_TOOLCHAIN_FILE=../$CMAKE_TOOLCHAIN_FILE -DCMAKE_BUILD_TYPE=Release

echo "Building project..."
make -j$(nproc)

echo "Build completed successfully!"
echo "Windows executable: $BUILD_DIR/fractalia2.exe"

echo "Copying required DLLs..."

# Copy SDL3 DLL
if [ -f ../vendored/SDL3-3.1.6/x86_64-w64-mingw32/bin/SDL3.dll ]; then
    cp ../vendored/SDL3-3.1.6/x86_64-w64-mingw32/bin/SDL3.dll .
    echo "SDL3.dll copied to build directory"
fi

echo "Copying build folder to /mnt/f/Projects/Fractalia2..."
DEST_DIR="/mnt/f/Projects/Fractalia2"
mkdir -p "$DEST_DIR"
cp -r . "$DEST_DIR/build"
echo "Build folder copied to $DEST_DIR/build"


