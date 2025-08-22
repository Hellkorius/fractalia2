#include "PolygonFactory.h"

PolygonMesh PolygonFactory::createTriangle() {
    PolygonMesh triangle;
    triangle.vertices = {
        {{0.0f, -2.0f, 0.0f}, {1.0f, 1.0f, 1.0f}},
        {{2.0f, 2.0f, 0.0f}, {1.0f, 1.0f, 1.0f}},
        {{-2.0f, 2.0f, 0.0f}, {1.0f, 1.0f, 1.0f}}
    };
    triangle.indices = {0, 1, 2};
    return triangle;
}

PolygonMesh PolygonFactory::createSquare() {
    PolygonMesh square;
    square.vertices = {
        {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},
        {{0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},
        {{0.5f, 0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},
        {{-0.5f, 0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}}
    };
    square.indices = {0, 1, 2, 0, 2, 3};
    return square;
}

PolygonMesh PolygonFactory::createCube() {
    PolygonMesh cube;
    
    // Define 8 vertices of a cube (from -1 to +1 on each axis)
    cube.vertices = {
        // Front face (Z = +1)
        {{-1.0f, -1.0f,  1.0f}, {1.0f, 0.0f, 0.0f}},  // 0: Bottom-left
        {{ 1.0f, -1.0f,  1.0f}, {0.0f, 1.0f, 0.0f}},  // 1: Bottom-right  
        {{ 1.0f,  1.0f,  1.0f}, {0.0f, 0.0f, 1.0f}},  // 2: Top-right
        {{-1.0f,  1.0f,  1.0f}, {1.0f, 1.0f, 0.0f}},  // 3: Top-left
        
        // Back face (Z = -1)
        {{-1.0f, -1.0f, -1.0f}, {1.0f, 0.0f, 1.0f}},  // 4: Bottom-left
        {{ 1.0f, -1.0f, -1.0f}, {0.0f, 1.0f, 1.0f}},  // 5: Bottom-right
        {{ 1.0f,  1.0f, -1.0f}, {1.0f, 1.0f, 1.0f}},  // 6: Top-right  
        {{-1.0f,  1.0f, -1.0f}, {0.5f, 0.5f, 0.5f}},  // 7: Top-left
    };
    
    // Define 12 triangles (2 per face, 6 faces)
    cube.indices = {
        // Front face
        0, 1, 2,  2, 3, 0,
        // Back face  
        4, 7, 6,  6, 5, 4,
        // Left face
        4, 0, 3,  3, 7, 4,
        // Right face
        1, 5, 6,  6, 2, 1,
        // Top face
        3, 2, 6,  6, 7, 3,
        // Bottom face
        4, 5, 1,  1, 0, 4
    };
    
    return cube;
}