#include "PolygonFactory.h"

PolygonMesh PolygonFactory::createTriangle() {
    PolygonMesh triangle;
    triangle.vertices = {
        {{0.0f, -2.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
        {{2.0f, 2.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
        {{-2.0f, 2.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}}
    };
    triangle.indices = {0, 1, 2};
    return triangle;
}

PolygonMesh PolygonFactory::createSquare() {
    PolygonMesh square;
    square.vertices = {
        {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        {{0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        {{0.5f, 0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        {{-0.5f, 0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}
    };
    square.indices = {0, 1, 2, 0, 2, 3};
    return square;
}

PolygonMesh PolygonFactory::createCube() {
    PolygonMesh cube;
    
    // Define vertices for each face with proper normals
    // Each face gets its own vertices for correct normal calculation
    cube.vertices = {
        // Front face (Z+) - normal: (0, 0, 1)
        {{-0.5f, -0.5f,  0.5f}, {0.8f, 0.8f, 0.8f}, {0.0f, 0.0f, 1.0f}}, // 0
        {{ 0.5f, -0.5f,  0.5f}, {0.8f, 0.8f, 0.8f}, {0.0f, 0.0f, 1.0f}}, // 1
        {{ 0.5f,  0.5f,  0.5f}, {0.8f, 0.8f, 0.8f}, {0.0f, 0.0f, 1.0f}}, // 2
        {{-0.5f,  0.5f,  0.5f}, {0.8f, 0.8f, 0.8f}, {0.0f, 0.0f, 1.0f}}, // 3
        
        // Back face (Z-) - normal: (0, 0, -1)
        {{ 0.5f, -0.5f, -0.5f}, {0.6f, 0.6f, 0.6f}, {0.0f, 0.0f, -1.0f}}, // 4
        {{-0.5f, -0.5f, -0.5f}, {0.6f, 0.6f, 0.6f}, {0.0f, 0.0f, -1.0f}}, // 5
        {{-0.5f,  0.5f, -0.5f}, {0.6f, 0.6f, 0.6f}, {0.0f, 0.0f, -1.0f}}, // 6
        {{ 0.5f,  0.5f, -0.5f}, {0.6f, 0.6f, 0.6f}, {0.0f, 0.0f, -1.0f}}, // 7
        
        // Left face (X-) - normal: (-1, 0, 0)
        {{-0.5f, -0.5f, -0.5f}, {0.4f, 0.4f, 0.4f}, {-1.0f, 0.0f, 0.0f}}, // 8
        {{-0.5f, -0.5f,  0.5f}, {0.4f, 0.4f, 0.4f}, {-1.0f, 0.0f, 0.0f}}, // 9
        {{-0.5f,  0.5f,  0.5f}, {0.4f, 0.4f, 0.4f}, {-1.0f, 0.0f, 0.0f}}, // 10
        {{-0.5f,  0.5f, -0.5f}, {0.4f, 0.4f, 0.4f}, {-1.0f, 0.0f, 0.0f}}, // 11
        
        // Right face (X+) - normal: (1, 0, 0)
        {{ 0.5f, -0.5f,  0.5f}, {0.7f, 0.7f, 0.7f}, {1.0f, 0.0f, 0.0f}}, // 12
        {{ 0.5f, -0.5f, -0.5f}, {0.7f, 0.7f, 0.7f}, {1.0f, 0.0f, 0.0f}}, // 13
        {{ 0.5f,  0.5f, -0.5f}, {0.7f, 0.7f, 0.7f}, {1.0f, 0.0f, 0.0f}}, // 14
        {{ 0.5f,  0.5f,  0.5f}, {0.7f, 0.7f, 0.7f}, {1.0f, 0.0f, 0.0f}}, // 15
        
        // Top face (Y+) - normal: (0, 1, 0)
        {{-0.5f,  0.5f,  0.5f}, {0.9f, 0.9f, 0.9f}, {0.0f, 1.0f, 0.0f}}, // 16
        {{ 0.5f,  0.5f,  0.5f}, {0.9f, 0.9f, 0.9f}, {0.0f, 1.0f, 0.0f}}, // 17
        {{ 0.5f,  0.5f, -0.5f}, {0.9f, 0.9f, 0.9f}, {0.0f, 1.0f, 0.0f}}, // 18
        {{-0.5f,  0.5f, -0.5f}, {0.9f, 0.9f, 0.9f}, {0.0f, 1.0f, 0.0f}}, // 19
        
        // Bottom face (Y-) - normal: (0, -1, 0)
        {{-0.5f, -0.5f, -0.5f}, {0.3f, 0.3f, 0.3f}, {0.0f, -1.0f, 0.0f}}, // 20
        {{ 0.5f, -0.5f, -0.5f}, {0.3f, 0.3f, 0.3f}, {0.0f, -1.0f, 0.0f}}, // 21
        {{ 0.5f, -0.5f,  0.5f}, {0.3f, 0.3f, 0.3f}, {0.0f, -1.0f, 0.0f}}, // 22
        {{-0.5f, -0.5f,  0.5f}, {0.3f, 0.3f, 0.3f}, {0.0f, -1.0f, 0.0f}}  // 23
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