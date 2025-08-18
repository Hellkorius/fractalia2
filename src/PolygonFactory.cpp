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