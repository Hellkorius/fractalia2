#pragma once

#include <vector>
#include <glm/glm.hpp>

struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
};

struct PolygonMesh {
    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;
};

class PolygonFactory {
public:
    static PolygonMesh createTriangle();
    static PolygonMesh createSquare();
};