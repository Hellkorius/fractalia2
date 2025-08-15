#!/bin/bash

# Create shaders directories if they don't exist
mkdir -p build/shaders/compiled
mkdir -p src/shaders/compiled

# Compile vertex shader
glslangValidator -V src/shaders/vertex.vert -o src/shaders/compiled/vertex.spv
cp src/shaders/compiled/vertex.spv build/shaders/compiled/

# Compile fragment shader  
glslangValidator -V src/shaders/fragment.frag -o src/shaders/compiled/fragment.spv
cp src/shaders/compiled/fragment.spv build/shaders/compiled/



echo "Shaders compiled successfully!"