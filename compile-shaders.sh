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

# Compile shadow vertex
glslangValidator -V src/shaders/shadow_depth.vert -o src/shaders/compiled/shadow_depth.vert.spv
cp src/shaders/compiled/shadow_depth.vert.spv build/shaders/

# Compile shadow fragment
glslangValidator -V src/shaders/shadow_depth.frag -o src/shaders/compiled/shadow_depth.frag.spv
cp src/shaders/compiled/shadow_depth.frag.spv build/shaders/

# Compile sun particle compute
glslangValidator -V src/shaders/sun_particles.comp -o src/shaders/compiled/sun_particles.comp.spv
cp src/shaders/compiled/sun_particles.comp.spv build/shaders/

# Compile sun particle fragment
glslangValidator -V src/shaders/sun_particles.frag -o src/shaders/compiled/sun_particles.frag.spv
cp src/shaders/compiled/sun_particles.frag.spv build/shaders/

# Compile sun disc vert
glslangValidator -V src/shaders/sun_disc.vert -o src/shaders/compiled/sun_disc.vert.spv
cp src/shaders/compiled/sun_disc.vert.spv build/shaders/

# Compile god rays vert
glslangValidator -V src/shaders/god_rays.vert -o src/shaders/compiled/god_rays.vert.spv
cp src/shaders/compiled/god_rays.vert.spv build/shaders/

# Compile god rays frag
glslangValidator -V src/shaders/god_rays.frag -o src/shaders/compiled/god_rays.frag.spv
cp src/shaders/compiled/god_rays.frag.spv build/shaders/

# Compile sun disk frag
glslangValidator -V src/shaders/sun_disc.frag -o src/shaders/compiled/sun_disc.frag.spv
cp src/shaders/compiled/sun_disc.frag.spv build/shaders/

# Compile sun particles vertex
glslangValidator -V src/shaders/sun_particles.vert -o src/shaders/compiled/sun_particles.vert.spv
cp src/shaders/compiled/sun_particles.vert.spv build/shaders/

# Compile compute shader (random walk movement)
glslangValidator -V src/shaders/movement_random.comp -o src/shaders/compiled/movement_random.comp.spv
cp src/shaders/compiled/movement_random.comp.spv build/shaders/

# Compile compute shader (physics)
glslangValidator -V src/shaders/physics.comp -o src/shaders/compiled/physics.comp.spv
cp src/shaders/compiled/physics.comp.spv build/shaders/

# Compile new sun system shaders
glslangValidator -V src/shaders/sun_system.comp -o src/shaders/compiled/sun_system.comp.spv
cp src/shaders/compiled/sun_system.comp.spv build/shaders/

glslangValidator -V src/shaders/sun_system.vert -o src/shaders/compiled/sun_system.vert.spv
cp src/shaders/compiled/sun_system.vert.spv build/shaders/

glslangValidator -V src/shaders/sun_system.frag -o src/shaders/compiled/sun_system.frag.spv
cp src/shaders/compiled/sun_system.frag.spv build/shaders/

# Export shaders to Windows build folder
WINDOWS_DEST="/mnt/f/Projects/Fractalia2/build/shaders"
if mkdir -p "$WINDOWS_DEST" 2>/dev/null; then
    cp src/shaders/compiled/*.spv "$WINDOWS_DEST/"
    echo "Shaders exported to Windows build folder: $WINDOWS_DEST"
else
    echo "Warning: Could not export to Windows build folder (path may not exist)"
fi

echo "Shaders compiled successfully!"

