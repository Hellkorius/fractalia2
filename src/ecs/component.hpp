#pragma once

#include <glm/glm.hpp>

struct Position {
    float x, y, z;
};

struct Velocity {
    float x, y, z;
};

struct Color {
    float r, g, b, a;
};

enum class ShapeType {
    Triangle,
    Square
};

struct Shape {
    ShapeType type;
};

struct Rotation {
    glm::vec3 axis;
    float angle;
};