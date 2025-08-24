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
    
    // Define the 8 vertices of a cube centered at origin
    // Each face will have distinct colors for visual differentiation
    cube.vertices = {
        // Front face (red)
        {{-0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}}, // 0: bottom-left
        {{ 0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}}, // 1: bottom-right
        {{ 0.5f,  0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}}, // 2: top-right
        {{-0.5f,  0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}}, // 3: top-left
        
        // Back face (green)
        {{-0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}}, // 4: bottom-left
        {{ 0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}}, // 5: bottom-right
        {{ 0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}}, // 6: top-right
        {{-0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}}, // 7: top-left
        
        // Left face (blue)
        {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}}, // 8: bottom-back
        {{-0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}}, // 9: bottom-front
        {{-0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}}, // 10: top-front
        {{-0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}}, // 11: top-back
        
        // Right face (yellow)
        {{ 0.5f, -0.5f,  0.5f}, {1.0f, 1.0f, 0.0f}}, // 12: bottom-front
        {{ 0.5f, -0.5f, -0.5f}, {1.0f, 1.0f, 0.0f}}, // 13: bottom-back
        {{ 0.5f,  0.5f, -0.5f}, {1.0f, 1.0f, 0.0f}}, // 14: top-back
        {{ 0.5f,  0.5f,  0.5f}, {1.0f, 1.0f, 0.0f}}, // 15: top-front
        
        // Top face (magenta)
        {{-0.5f,  0.5f,  0.5f}, {1.0f, 0.0f, 1.0f}}, // 16: front-left
        {{ 0.5f,  0.5f,  0.5f}, {1.0f, 0.0f, 1.0f}}, // 17: front-right
        {{ 0.5f,  0.5f, -0.5f}, {1.0f, 0.0f, 1.0f}}, // 18: back-right
        {{-0.5f,  0.5f, -0.5f}, {1.0f, 0.0f, 1.0f}}, // 19: back-left
        
        // Bottom face (cyan)
        {{-0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 1.0f}}, // 20: back-left
        {{ 0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 1.0f}}, // 21: back-right
        {{ 0.5f, -0.5f,  0.5f}, {0.0f, 1.0f, 1.0f}}, // 22: front-right
        {{-0.5f, -0.5f,  0.5f}, {0.0f, 1.0f, 1.0f}}  // 23: front-left
    };
    
    // Define triangles for each face (counter-clockwise winding)
    cube.indices = {
        // Front face
        0, 1, 2,   2, 3, 0,
        // Back face
        4, 7, 6,   6, 5, 4,
        // Left face
        8, 11, 10,   10, 9, 8,
        // Right face
        12, 13, 14,   14, 15, 12,
        // Top face
        16, 17, 18,   18, 19, 16,
        // Bottom face
        20, 23, 22,   22, 21, 20
    };
    
    return cube;
}