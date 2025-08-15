#pragma once

#include <queue>
#include <mutex>
#include <atomic>
#include <optional>
#include <array>
#include <string_view>

// Movement command data structure
struct MovementCommand {
    enum class Type : int {
        Petal = 0,
        Orbit = 1, 
        Wave = 2,
        TriangleFormation = 3,
        RandomStep = 4,
        COUNT // For validation
    };
    
    Type targetType{Type::Petal};
    bool angelMode{false};
    double timestamp{0.0}; // When command was created
    
    // Validation
    bool isValid() const {
        return static_cast<int>(targetType) >= 0 && 
               static_cast<int>(targetType) < static_cast<int>(Type::COUNT);
    }
};

// Thread-safe command queue
class MovementCommandQueue {
public:
    MovementCommandQueue();
    ~MovementCommandQueue() = default;
    
    // Thread-safe command enqueueing (from input thread)
    bool enqueue(const MovementCommand& command);
    
    // Thread-safe command dequeueing (from render thread)
    std::optional<MovementCommand> dequeue();
    
    // Check if queue has commands (atomic read)
    bool hasCommands() const;
    
    // Get queue size (for debugging/monitoring)
    size_t size() const;
    
    // Clear all pending commands
    void clear();
    
private:
    static constexpr size_t MAX_COMMANDS = 64; // Reasonable limit
    
    mutable std::mutex queueMutex;
    std::queue<MovementCommand> commands;
    std::atomic<bool> hasCommandsFlag{false};
    
    // Statistics for monitoring
    std::atomic<size_t> totalEnqueued{0};
    std::atomic<size_t> totalProcessed{0};
};

// Movement command processor with proper error handling
class MovementCommandProcessor {
public:
    explicit MovementCommandProcessor(class GPUEntityManager* gpuManager);
    ~MovementCommandProcessor() = default;
    
    // Process all pending commands (called from render thread at sync point)
    void processCommands();
    
    // Get command queue for enqueueing
    MovementCommandQueue& getCommandQueue() { return commandQueue; }
    
    // Error handling
    enum class ProcessResult {
        Success,
        NoCommands,
        InvalidCommand,
        GPUManagerNull,
        UpdateFailed
    };
    
    ProcessResult getLastResult() const { return lastResult; }
    
    // Performance monitoring
    struct Stats {
        size_t totalCommandsProcessed{0};
        size_t invalidCommands{0};
        size_t failedUpdates{0};
        double lastProcessTime{0.0};
    };
    
    const Stats& getStats() const { return stats; }
    void resetStats() { stats = {}; }
    
private:
    MovementCommandQueue commandQueue;
    GPUEntityManager* gpuEntityManager;
    
    // Performance optimizations
    static constexpr std::array<std::string_view, 5> MOVEMENT_NAMES = {
        "PETAL", "ORBIT", "WAVE", "TRIANGLE FORMATION", "RANDOM STEP"
    };
    
    // Error handling
    std::atomic<ProcessResult> lastResult{ProcessResult::Success};
    Stats stats;
    
    // Helper methods
    bool validateCommand(const MovementCommand& command);
    bool executeMovementUpdate(const MovementCommand& command);
    void logCommandExecution(const MovementCommand& command, bool success);
};