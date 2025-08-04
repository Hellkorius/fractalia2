#!/bin/bash

set -e

BUILD_DIR="build-linux"

echo "Creating build directory..."
rm -rf $BUILD_DIR
mkdir -p $BUILD_DIR

echo "Running CMake configuration for Linux..."
cd $BUILD_DIR
cmake .. -DCMAKE_BUILD_TYPE=Release

echo "Building project..."
make -j$(nproc)

echo "Build completed successfully!"
echo "Linux executable: $BUILD_DIR/fractalia2"