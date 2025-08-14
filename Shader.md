### Shader Development
1. Edit GLSL files in `src/shaders/` (vertex.vert, fragment.frag, movement.comp)
2. Run `./compile-shaders.sh` to compile to SPIR-V
3. Rebuild project to include updated shaders
4. **Note**: Keyframe shader path is `shaders/compiled/movement.spv`