#include "movement_module.h"
#include "../core/service_locator.h"
#include <stdexcept>
#include <chrono>
#include <cmath>

const std::string MovementModule::MODULE_NAME = "MovementModule";

MovementModule::MovementModule(GPUEntityManager* gpuManager)
    : gpuEntityManager_(gpuManager)
    , world_(nullptr) {
}

MovementModule::~MovementModule() {
    shutdown();
}

bool MovementModule::initialize(flecs::world& world) {
    if (initialized_) {
        return true;
    }

    try {
        world_ = &world;
        
        // Initialize command processor if we have a GPU manager
        if (gpuEntityManager_) {
            commandProcessor_ = std::make_unique<MovementCommandProcessor>(gpuEntityManager_);
        }
        
        // Setup movement processing phases
        setupMovementPhases();
        
        // Register movement systems with proper phases
        registerMovementSystems();
        
        // Reset stats
        resetStats();
        
        initialized_ = true;
        return true;
        
    } catch (const std::exception& e) {
        // Log error (in production, use proper logging)
        shutdown();
        return false;
    }
}

void MovementModule::shutdown() {
    if (!initialized_) {
        return;
    }

    try {
        cleanupSystems();
        
        commandProcessor_.reset();
        world_ = nullptr;
        initialized_ = false;
        
    } catch (const std::exception& e) {
        // Log error but continue shutdown
    }
}

void MovementModule::update(float deltaTime) {
    if (!initialized_ || !world_) {
        return;
    }

    auto updateStart = std::chrono::high_resolution_clock::now();
    
    try {
        // Process movement commands if we have a command processor
        if (commandProcessor_) {
            processMovementCommands();
        }
        
        // Update movement patterns
        updateMovementPatterns(deltaTime);
        
        // Update entity physics
        updateEntityPhysics(deltaTime);
        
        // Update performance stats
        auto updateEnd = std::chrono::high_resolution_clock::now();
        auto updateDuration = std::chrono::duration<float, std::milli>(updateEnd - updateStart).count();
        
        stats_.lastUpdateTime = updateDuration;
        
        // Simple moving average for average update time
        stats_.averageUpdateTime = (stats_.averageUpdateTime * 0.95f) + (updateDuration * 0.05f);
        
    } catch (const std::exception& e) {
        // Log error (in production, use proper logging)
    }
}

const std::string& MovementModule::getName() const {
    return MODULE_NAME;
}

void MovementModule::setGPUEntityManager(GPUEntityManager* gpuManager) {
    gpuEntityManager_ = gpuManager;
    
    if (initialized_ && gpuManager && !commandProcessor_) {
        commandProcessor_ = std::make_unique<MovementCommandProcessor>(gpuManager);
    }
}

bool MovementModule::enqueueMovementCommand(const MovementCommand& command) {
    if (!commandProcessor_) {
        return false;
    }
    
    bool result = commandProcessor_->getCommandQueue().enqueue(command);
    if (result) {
        stats_.commandsEnqueued++;
    }
    return result;
}

void MovementModule::processMovementCommands() {
    if (!commandProcessor_) {
        return;
    }
    
    size_t commandsBefore = commandProcessor_->getStats().totalCommandsProcessed;
    commandProcessor_->processCommands();
    size_t commandsAfter = commandProcessor_->getStats().totalCommandsProcessed;
    
    stats_.commandsProcessed += (commandsAfter - commandsBefore);
}

void MovementModule::updateMovementPatterns(float deltaTime) {
    if (!world_) {
        return;
    }
    
    // Use the static system function for consistency
    MovementSystems::updateMovementPatterns(*world_, deltaTime);
}

void MovementModule::resetAllMovementPatterns() {
    if (!world_) {
        return;
    }
    
    // Reset all movement pattern states
    world_->query<MovementPattern>().each([](flecs::entity e, MovementPattern& pattern) {
        pattern.currentTime = 0.0f;
        pattern.phase = 0.0f;
        // Keep other parameters like amplitude, frequency, center intact
    });
}

void MovementModule::updateEntityPhysics(float deltaTime) {
    if (!world_) {
        return;
    }
    
    // Use the static system function for consistency
    MovementSystems::updatePhysics(*world_, deltaTime);
}

void MovementModule::resetStats() {
    stats_ = {};
}

void MovementModule::setupMovementPhases() {
    // Create movement update phase that runs after input but before rendering
    auto movementPhase = world_->entity("MovementPhase")
        .add(flecs::Phase)
        .depends_on(flecs::OnUpdate);
        
    // Create physics phase that runs after movement
    auto physicsPhase = world_->entity("PhysicsPhase")
        .add(flecs::Phase)
        .depends_on(movementPhase);
        
    // Create movement sync phase that runs after physics
    auto movementSyncPhase = world_->entity("MovementSyncPhase")
        .add(flecs::Phase)
        .depends_on(physicsPhase);
}

void MovementModule::registerMovementSystems() {
    auto movementPhase = world_->entity("MovementPhase");
    auto physicsPhase = world_->entity("PhysicsPhase");
    auto movementSyncPhase = world_->entity("MovementSyncPhase");
    
    // Register movement pattern update system
    movementUpdateSystem_ = world_->system<Transform, MovementPattern>()
        .kind(movementPhase)
        .each(movementUpdateSystemCallback);
        
    // Register physics update system
    physicsUpdateSystem_ = world_->system<Transform, Velocity>()
        .kind(physicsPhase)
        .each(physicsUpdateSystemCallback);
        
    // Register movement synchronization system
    movementSyncSystem_ = world_->system<Transform, Renderable, MovementPattern>()
        .kind(movementSyncPhase)
        .each(movementSyncSystemCallback);
        
    if (!movementUpdateSystem_.is_valid() || !physicsUpdateSystem_.is_valid() || !movementSyncSystem_.is_valid()) {
        throw std::runtime_error("Failed to register movement systems");
    }
}

void MovementModule::cleanupSystems() {
    if (movementUpdateSystem_.is_valid()) {
        movementUpdateSystem_.destruct();
    }
    if (physicsUpdateSystem_.is_valid()) {
        physicsUpdateSystem_.destruct();
    }
    if (movementSyncSystem_.is_valid()) {
        movementSyncSystem_.destruct();
    }
}

// System callback implementations
void MovementModule::movementUpdateSystemCallback(flecs::entity e, Transform& transform, MovementPattern& pattern) {
    float deltaTime = e.world().delta_time();
    
    pattern.currentTime += deltaTime;
    
    // Simple random walk implementation
    if (pattern.movementType == MovementType::RandomWalk) {
        float phase = pattern.phase + pattern.currentTime * pattern.frequency;
        
        // Use sine waves with phase offset for smooth random-like movement
        glm::vec3 offset;
        offset.x = sin(phase) * pattern.amplitude;
        offset.y = cos(phase * 1.3f) * pattern.amplitude * 0.7f;
        offset.z = sin(phase * 0.8f) * pattern.amplitude * 0.5f;
        
        transform.position = pattern.center + offset;
    }
}

void MovementModule::physicsUpdateSystemCallback(flecs::entity e, Transform& transform, Velocity& velocity) {
    float deltaTime = e.world().delta_time();
    
    // Apply velocity to position
    transform.position += velocity.linear * deltaTime;
    
    // Apply angular velocity to rotation
    glm::vec3 angularDelta = velocity.angular * deltaTime;
    glm::quat deltaRotation = glm::quat(angularDelta);
    transform.rotation = deltaRotation * transform.rotation;
    
    // Simple damping
    velocity.linear *= 0.98f;
    velocity.angular *= 0.95f;
}

void MovementModule::movementSyncSystemCallback(flecs::entity e, Transform& transform, Renderable& renderable, MovementPattern& pattern) {
    // This system prepares data for GPU synchronization
    // In a full implementation, this would update GPU entity buffers
    // For now, we just ensure consistency between components
    
    // Update model matrix in renderable component
    glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), transform.position);
    glm::mat4 rotationMatrix = glm::mat4_cast(transform.rotation);
    glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), transform.scale);
    
    renderable.modelMatrix = translationMatrix * rotationMatrix * scaleMatrix;
}

// MovementSystems namespace implementation
namespace MovementSystems {
    void updateMovementPatterns(flecs::world& world, float deltaTime) {
        world.query<Transform, MovementPattern>().each([deltaTime](flecs::entity e, Transform& transform, MovementPattern& pattern) {
            pattern.currentTime += deltaTime;
            
            if (pattern.movementType == MovementType::RandomWalk) {
                float phase = pattern.phase + pattern.currentTime * pattern.frequency;
                
                glm::vec3 offset;
                offset.x = sin(phase) * pattern.amplitude;
                offset.y = cos(phase * 1.3f) * pattern.amplitude * 0.7f;
                offset.z = sin(phase * 0.8f) * pattern.amplitude * 0.5f;
                
                transform.position = pattern.center + offset;
            }
        });
    }
    
    void updatePhysics(flecs::world& world, float deltaTime) {
        world.query<Transform, Velocity>().each([deltaTime](flecs::entity e, Transform& transform, Velocity& velocity) {
            transform.position += velocity.linear * deltaTime;
            
            glm::vec3 angularDelta = velocity.angular * deltaTime;
            glm::quat deltaRotation = glm::quat(angularDelta);
            transform.rotation = deltaRotation * transform.rotation;
            
            velocity.linear *= 0.98f;
            velocity.angular *= 0.95f;
        });
    }
    
    void synchronizeWithGPU(flecs::world& world, GPUEntityManager* gpuManager) {
        if (!gpuManager) {
            return;
        }
        
        std::vector<flecs::entity> entities;
        world.query<Transform, Renderable, MovementPattern>().each([&entities](flecs::entity e, Transform& transform, Renderable& renderable, MovementPattern& pattern) {
            entities.push_back(e);
        });
        
        if (!entities.empty()) {
            gpuManager->addEntitiesFromECS(entities);
            gpuManager->uploadPendingEntities();
        }
    }
}

// MovementModuleAccess namespace implementation
namespace MovementModuleAccess {
    std::shared_ptr<MovementModule> getMovementModule(flecs::world& world) {
        auto worldManager = ServiceLocator::instance().getService<WorldManager>();
        if (!worldManager) {
            return nullptr;
        }
        
        return worldManager->getModule<MovementModule>("MovementModule");
    }
    
    bool enqueueMovementCommand(flecs::world& world, const MovementCommand& command) {
        auto module = getMovementModule(world);
        return module ? module->enqueueMovementCommand(command) : false;
    }
    
    void processMovementCommands(flecs::world& world) {
        auto module = getMovementModule(world);
        if (module) {
            module->processMovementCommands();
        }
    }
}