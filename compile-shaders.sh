#!/bin/bash

# Create shaders directories if they don't exist
mkdir -p build/shaders/compiled
mkdir -p src/shaders/compiled

# Compile vertex shader
glslangValidator -V src/shaders/vertex.vert -o src/shaders/compiled/vertex.vert.spv
cp src/shaders/compiled/vertex.vert.spv build/shaders/

# Compile fragment shader  
glslangValidator -V src/shaders/fragment.frag -o src/shaders/compiled/fragment.frag.spv
cp src/shaders/compiled/fragment.frag.spv build/shaders/

# Compile compute shader (random walk movement)
glslangValidator -V src/shaders/movement_random.comp -o src/shaders/compiled/movement_random.comp.spv
cp src/shaders/compiled/movement_random.comp.spv build/shaders/


echo "Shaders compiled successfully!"

