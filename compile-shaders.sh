#!/bin/bash

# Create shaders directory if it doesn't exist
mkdir -p build/shaders/compiled

# Compile vertex shader
glslangValidator -V src/shaders/vertex.vert -o build/shaders/compiled/vertex.spv

# Compile fragment shader  
glslangValidator -V src/shaders/fragment.frag -o build/shaders/compiled/fragment.spv

echo "Shaders compiled successfully!"