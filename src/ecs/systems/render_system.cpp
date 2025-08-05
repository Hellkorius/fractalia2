#include "../component.hpp"
#include "../../vulkan_renderer.h"
#include <flecs.h>
#include <glm/glm.hpp>
#include <vector>
#include <tuple>

class RenderSystem {
private:
    VulkanRenderer* renderer;
    flecs::query<Position, Shape, Color> renderQuery;

public:
    RenderSystem(VulkanRenderer* vulkanRenderer, flecs::world& world) 
        : renderer(vulkanRenderer), renderQuery(world.query<Position, Shape, Color>()) {}

    void update() {
        if (!renderer) return;

        std::vector<std::tuple<glm::vec3, VulkanRenderer::ShapeType, glm::vec4>> renderEntities;
        renderEntities.reserve(32);
        
        renderQuery.each([&](flecs::entity e, Position& pos, Shape& shape, Color& color) {
            VulkanRenderer::ShapeType vkShapeType = (shape.type == ShapeType::Triangle) ? 
                VulkanRenderer::ShapeType::Triangle : VulkanRenderer::ShapeType::Square;
            renderEntities.emplace_back(
                glm::vec3(pos.x, pos.y, pos.z),
                vkShapeType,
                glm::vec4(color.r, color.g, color.b, color.a)
            );
        });
        
        renderer->updateEntities(renderEntities);
    }
};