#include "movement_command_system.h"
#include "gpu_entity_manager.h"
#include <iostream>
#include <chrono>
#include <algorithm>

// MovementCommandQueue implementation
MovementCommandQueue::MovementCommandQueue() {
    // Pre-reserve queue capacity for performance
    // Note: std::queue doesn't have reserve, but underlying container might
}

bool MovementCommandQueue::enqueue(const MovementCommand& command) {
    if (!command.isValid()) {
        std::cerr << "MovementCommandQueue: Rejected invalid command" << std::endl;
        return false;
    }
    
    std::lock_guard<std::mutex> lock(queueMutex);
    
    // Check queue size limit to prevent unbounded growth
    if (commands.size() >= MAX_COMMANDS) {
        std::cerr << "MovementCommandQueue: Queue full, dropping oldest command" << std::endl;
        commands.pop(); // Drop oldest command
    }
    
    commands.push(command);
    hasCommandsFlag.store(true, std::memory_order_release);
    totalEnqueued.fetch_add(1, std::memory_order_relaxed);
    
    return true;
}

std::optional<MovementCommand> MovementCommandQueue::dequeue() {
    std::lock_guard<std::mutex> lock(queueMutex);
    
    if (commands.empty()) {
        hasCommandsFlag.store(false, std::memory_order_release);
        return std::nullopt;
    }
    
    MovementCommand command = commands.front();
    commands.pop();
    totalProcessed.fetch_add(1, std::memory_order_relaxed);
    
    // Update flag atomically
    hasCommandsFlag.store(!commands.empty(), std::memory_order_release);
    
    return command;
}

bool MovementCommandQueue::hasCommands() const {
    return hasCommandsFlag.load(std::memory_order_acquire);
}

size_t MovementCommandQueue::size() const {
    std::lock_guard<std::mutex> lock(queueMutex);
    return commands.size();
}

void MovementCommandQueue::clear() {
    std::lock_guard<std::mutex> lock(queueMutex);
    std::queue<MovementCommand> empty;
    commands.swap(empty); // Efficient clear
    hasCommandsFlag.store(false, std::memory_order_release);
}

// MovementCommandProcessor implementation
MovementCommandProcessor::MovementCommandProcessor(GPUEntityManager* gpuManager) 
    : gpuEntityManager(gpuManager) {
    
    if (!gpuEntityManager) {
        std::cerr << "MovementCommandProcessor: Warning - GPUEntityManager is null" << std::endl;
    }
}

void MovementCommandProcessor::processCommands() {
    if (!gpuEntityManager) {
        lastResult.store(ProcessResult::GPUManagerNull, std::memory_order_release);
        return;
    }
    
    // Early exit if no commands (fast atomic check)
    if (!commandQueue.hasCommands()) {
        lastResult.store(ProcessResult::NoCommands, std::memory_order_release);
        return;
    }
    
    auto startTime = std::chrono::high_resolution_clock::now();
    size_t commandsProcessedThisFrame = 0;
    ProcessResult result = ProcessResult::Success;
    
    // Process all available commands in this frame
    while (auto commandOpt = commandQueue.dequeue()) {
        const MovementCommand& command = *commandOpt;
        
        if (!validateCommand(command)) {
            stats.invalidCommands++;
            result = ProcessResult::InvalidCommand;
            std::cerr << "MovementCommandProcessor: Invalid command rejected" << std::endl;
            continue;
        }
        
        if (!executeMovementUpdate(command)) {
            stats.failedUpdates++;
            result = ProcessResult::UpdateFailed;
            std::cerr << "MovementCommandProcessor: Failed to execute movement update" << std::endl;
            continue;
        }
        
        logCommandExecution(command, true);
        commandsProcessedThisFrame++;
        stats.totalCommandsProcessed++;
        
        // Limit commands processed per frame to avoid frame drops
        if (commandsProcessedThisFrame >= 4) { // Reasonable limit
            break;
        }
    }
    
    // Update timing statistics
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration<double, std::milli>(endTime - startTime);
    stats.lastProcessTime = duration.count();
    
    lastResult.store(result, std::memory_order_release);
}

bool MovementCommandProcessor::validateCommand(const MovementCommand& command) {
    // Basic validation
    if (!command.isValid()) {
        return false;
    }
    
    // Additional validation could include:
    // - Timestamp checking
    // - Rate limiting
    // - Command sequence validation
    
    return true;
}

bool MovementCommandProcessor::executeMovementUpdate(const MovementCommand& command) {
    if (!gpuEntityManager) {
        return false;
    }
    
    try {
        int movementType = static_cast<int>(command.targetType);
        gpuEntityManager->updateAllMovementTypes(movementType, command.angelMode);
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "MovementCommandProcessor: Exception during movement update: " << e.what() << std::endl;
        return false;
    }
    catch (...) {
        std::cerr << "MovementCommandProcessor: Unknown exception during movement update" << std::endl;
        return false;
    }
}

void MovementCommandProcessor::logCommandExecution(const MovementCommand& command, bool success) {
    if (!success) {
        return;
    }
    
    // Use compile-time lookup table for efficiency
    const auto& movementName = MOVEMENT_NAMES[static_cast<size_t>(command.targetType)];
    const int movementType = static_cast<int>(command.targetType);
    
    if (command.angelMode) {
        std::cout << "Executed ANGEL MODE transition to " << movementName << " (" << movementType 
                  << ") - biblical 2-second transition via origin" << std::endl;
    } else {
        std::cout << "Executed organic transition to " << movementName << " (" << movementType 
                  << ") - direct movement to target positions" << std::endl;
    }
}