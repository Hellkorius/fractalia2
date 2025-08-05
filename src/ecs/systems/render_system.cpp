#include "../component.hpp"
#include "../render_batch.hpp"
#include "../../vulkan_renderer.h"
#include <flecs.h>
#include <glm/glm.hpp>
#include <vector>
#include <tuple>
#include <chrono>
#include <unordered_map>

class RenderSystem {
private:
    VulkanRenderer* renderer;
    flecs::query<Transform, Renderable> renderQuery;
    BatchRenderer batchRenderer;
    
    // Change detection optimization
    struct EntityChangeInfo {
        uint32_t transformVersion{0};
        uint32_t renderableVersion{0};
    };
    std::unordered_map<uint32_t, EntityChangeInfo> changeTracker;
    
    // Performance monitoring
    std::chrono::high_resolution_clock::time_point lastUpdateTime;
    float averageUpdateTime{0.0f};
    size_t frameCount{0};
    
public:
    RenderSystem(VulkanRenderer* vulkanRenderer, flecs::world& world) 
        : renderer(vulkanRenderer), 
          renderQuery(world.query<Transform, Renderable>()),
          lastUpdateTime(std::chrono::high_resolution_clock::now()) {}

    void update() {
        auto startTime = std::chrono::high_resolution_clock::now();
        
        if (!renderer) return;

        batchRenderer.beginFrame();
        
        // Process all renderable entities
        renderQuery.each([&](flecs::entity e, Transform& transform, Renderable& renderable) {
            uint32_t entityId = static_cast<uint32_t>(e.id());
            
            // Optional: Change detection optimization
            // For maximum performance with thousands of entities
            /*
            auto it = changeTracker.find(entityId);
            if (it != changeTracker.end()) {
                // Check if entity has changed since last frame
                if (it->second.transformVersion == transform.version && 
                    it->second.renderableVersion == renderable.version) {
                    return; // Skip unchanged entities
                }
            }
            */
            
            batchRenderer.addEntity(transform, renderable, entityId);
            
            // Update change tracking
            // changeTracker[entityId] = {transform.version, renderable.version};
        });
        
        batchRenderer.endFrame();
        
        // Convert batches to renderer format
        updateRenderer();
        
        // Update performance stats
        auto endTime = std::chrono::high_resolution_clock::now();
        float frameTime = std::chrono::duration<float, std::milli>(endTime - startTime).count();
        averageUpdateTime = (averageUpdateTime * frameCount + frameTime) / (frameCount + 1);
        frameCount++;
        lastUpdateTime = endTime;
    }
    
    // Get performance statistics
    float getAverageUpdateTime() const { return averageUpdateTime; }
    const BatchRenderer::Stats& getBatchStats() const { return batchRenderer.getStats(); }
    size_t getTrackedEntityCount() const { return changeTracker.size(); }
    
private:
    void updateRenderer() {
        if (!batchRenderer.hasRenderData()) {
            renderer->updateEntities({});
            return;
        }
        
        // Convert batch data to legacy renderer format
        // TODO: Upgrade renderer to use batch data directly for better performance
        std::vector<std::tuple<glm::vec3, VulkanRenderer::ShapeType, glm::vec4>> renderEntities;
        renderEntities.reserve(batchRenderer.getTotalInstanceCount());
        
        batchRenderer.forEachBatch([&](Renderable::ShapeType shapeType, const RenderBatch& batch) {
            VulkanRenderer::ShapeType vkShapeType = (shapeType == Renderable::ShapeType::Triangle) ? 
                VulkanRenderer::ShapeType::Triangle : VulkanRenderer::ShapeType::Square;
                
            for (const auto& instance : batch.getInstances()) {
                // Extract position from transform matrix
                glm::vec3 position(instance.transform[3]);
                renderEntities.emplace_back(position, vkShapeType, instance.color);
            }
        });
        
        renderer->updateEntities(renderEntities);
    }
};