### Rendering
Currently uses legacy OpenGL (2.1) with immediate mode rendering for compatibility. The rendering pipeline:
1. Clear buffers
2. Set orthographic projection (-3 to 3 X, -2 to 2 Y)
3. Render each entity using component data
4. Swap buffers