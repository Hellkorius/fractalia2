#include "gpu_sync_system.hpp"
#include "../../vulkan_renderer.h"
#include "../gpu_entity_manager.h"
#include <iostream>
#include <vector>

namespace GPUSyncSystem {
    
    static VulkanRenderer* s_renderer = nullptr;
    
    void initialize(flecs::world& world, VulkanRenderer* renderer) {
        s_renderer = renderer;
        
        // Create custom phase for GPU operations - this ensures proper ordering
        auto gpuUploadPhase = world.entity("GPUUpload")
            .add(flecs::Phase)
            .depends_on(flecs::OnUpdate);  // After entity creation, before rendering
        
        // GPU entity upload system - critical timing preserved
        world.system("GPUEntityUploadSystem")
            .kind(gpuUploadPhase)  // Run in GPU upload phase
            .run([](flecs::iter& it) {
                auto* gpuSync = it.world().get_mut<GPUEntitySync>();
                auto* appState = it.world().get<ApplicationState>();
                if (gpuSync && appState && s_renderer) {
                    gpuEntityUploadSystem(it.world().entity(), *gpuSync, *appState);
                }
            });
        
        // Note: Observer removed - upload completion handled directly in upload system
        
        std::cout << "GPU Sync System initialized with proper phase ordering" << std::endl;
    }
    
    void gpuEntityUploadSystem(flecs::entity e, GPUEntitySync& gpuSync, const ApplicationState& appState) {
        if (!s_renderer || !s_renderer->getGPUEntityManager()) {
            std::cout << "GPU Sync System: Renderer or GPU manager not available" << std::endl;
            return;
        }
        
        // Update delta time for GPU compute
        gpuSync.deltaTime = appState.globalDeltaTime;
        s_renderer->setDeltaTime(gpuSync.deltaTime);
        
        // GPU sync system now mainly handles delta time updates
        // Direct uploads are handled in control systems
        
        // Only upload if we have pending entities
        if (gpuSync.needsUpload && gpuSync.pendingCount > 0) {
            // Process all entities with GPUUploadPending and collect them for GPU manager
            std::vector<flecs::entity> pendingEntities;
            
            // Simplified approach: iterate through entities with GPUUploadPending tag directly
            e.world().each([&](flecs::entity entity) {
                if (entity.has<GPUUploadPending>() && 
                    entity.has<Transform>() && 
                    entity.has<Renderable>() && 
                    entity.has<MovementPattern>()) {
                    pendingEntities.push_back(entity);
                }
            });
            
            // Convert and upload entities to GPU
            if (!pendingEntities.empty()) {
                // Entity is already aliased to flecs::entity, so direct conversion works
                std::vector<Entity> entityHandles;
                for (auto entity : pendingEntities) {
                    entityHandles.push_back(entity);
                }
                
                // Upload entities to GPU via the manager - this adds to pending queue
                s_renderer->getGPUEntityManager()->addEntitiesFromECS(entityHandles);
                
                // This preserves the original GPU fence synchronization timing
                s_renderer->uploadPendingGPUEntities();
                
                // Mark entities as uploaded and remove pending marker
                for (auto entity : pendingEntities) {
                    entity.remove<GPUUploadPending>();
                    entity.add<GPUUploadComplete>();
                    std::cout << "Entity " << entity.id() << " uploaded to GPU successfully" << std::endl;
                }
                
                // Reset sync state
                gpuSync.needsUpload = false;
                gpuSync.pendingCount = 0;
                
                std::cout << "Uploaded " << pendingEntities.size() << " entities to GPU" << std::endl;
            }
        }
    }
    
}