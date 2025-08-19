#!/bin/bash

set -e

BUILD_DIR="build"
CMAKE_TOOLCHAIN_FILE="mingw-w64-toolchain.cmake"

echo "=== Fast Build Mode ==="

# Check if toolchain file exists, create if needed
if [ ! -f "$CMAKE_TOOLCHAIN_FILE" ]; then
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
fi

# Create build directory if it doesn't exist
if [ ! -d "$BUILD_DIR" ]; then
    echo "Creating build directory..."
    mkdir -p $BUILD_DIR
fi

cd $BUILD_DIR

# Configure only if needed (no CMakeCache.txt or CMakeLists.txt is newer)
if [ ! -f "CMakeCache.txt" ] || [ "../CMakeLists.txt" -nt "CMakeCache.txt" ]; then
    echo "Configuring project..."
    # Build configuration with optimizations
    CMAKE_ARGS="-DCMAKE_TOOLCHAIN_FILE=../$CMAKE_TOOLCHAIN_FILE -DCMAKE_BUILD_TYPE=Release -DFAST_BUILD=ON"
    
    # Enable ccache if available for faster rebuilds
    if command -v ccache >/dev/null 2>&1; then
        echo "Using ccache for faster rebuilds..."
        CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache"
    fi
    
    # Try to use Ninja generator if available (faster than Make)
    if command -v ninja >/dev/null 2>&1; then
        echo "Using Ninja generator for faster builds..."
        cmake .. -G Ninja $CMAKE_ARGS
    else
        cmake .. $CMAKE_ARGS
    fi
else
    echo "Skipping configuration (up to date)"
fi

# Determine best build system and parallel jobs
NPROC=$(nproc)
PARALLEL_JOBS=$((NPROC + 2))  # Use more jobs than cores for I/O bound tasks

# Check if ninja is available and use it (faster than make)
if command -v ninja >/dev/null 2>&1 && [ -f "build.ninja" ]; then
    echo "Building with Ninja ($PARALLEL_JOBS jobs)..."
    ninja -j$PARALLEL_JOBS
elif [ -f "Makefile" ]; then
    echo "Building with Make ($PARALLEL_JOBS jobs)..."
    make -j$PARALLEL_JOBS
else
    echo "No build system found, running cmake configure first..."
    cmake .. -DCMAKE_TOOLCHAIN_FILE=../$CMAKE_TOOLCHAIN_FILE -DCMAKE_BUILD_TYPE=Release -DFAST_BUILD=ON
    make -j$PARALLEL_JOBS
fi

echo "Fast build completed!"
echo "Windows executable: $BUILD_DIR/fractalia2.exe"
echo ""
echo "Note: This fast build skips DLL copying."
echo "If you need DLLs, run ./build.sh instead."

echo "Copying build folder to /mnt/f/Projects/Fractalia2..."
DEST_DIR="/mnt/f/Projects/Fractalia2"
mkdir -p "$DEST_DIR"
cp -r . "$DEST_DIR/build"
echo "Build folder copied to $DEST_DIR/build"