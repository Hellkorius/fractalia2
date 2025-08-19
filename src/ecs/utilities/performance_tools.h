#pragma once

#include <chrono>
#include <iostream>
#include <iomanip>
#include <unordered_map>
#include <vector>
#include <string>
#include <fstream>
#include <memory>
#include <flecs.h>

namespace PerformanceTools {

class FrameTimer {
private:
    std::chrono::high_resolution_clock::time_point start_time_;
    std::chrono::high_resolution_clock::time_point last_frame_time_;
    
    std::vector<float> frame_times_;
    size_t current_index_ = 0;
    bool buffer_full_ = false;
    
    static constexpr size_t FRAME_HISTORY_SIZE = 120; // 2 seconds at 60 FPS
    
    float total_time_ = 0.0f;
    size_t frame_count_ = 0;
    
public:
    FrameTimer() {
        frame_times_.resize(FRAME_HISTORY_SIZE);
        reset();
    }
    
    void reset() {
        start_time_ = std::chrono::high_resolution_clock::now();
        last_frame_time_ = start_time_;
        current_index_ = 0;
        buffer_full_ = false;
        total_time_ = 0.0f;
        frame_count_ = 0;
        std::fill(frame_times_.begin(), frame_times_.end(), 0.0f);
    }
    
    float tick() {
        auto current_time = std::chrono::high_resolution_clock::now();
        float delta = std::chrono::duration<float>(current_time - last_frame_time_).count();
        
        frame_times_[current_index_] = delta;
        current_index_ = (current_index_ + 1) % FRAME_HISTORY_SIZE;
        
        if (!buffer_full_ && current_index_ == 0) {
            buffer_full_ = true;
        }
        
        total_time_ += delta;
        frame_count_++;
        
        last_frame_time_ = current_time;
        return delta;
    }
    
    float getAverageFrameTime() const {
        if (frame_count_ == 0) return 0.0f;
        return total_time_ / frame_count_;
    }
    
    float getRecentAverageFrameTime() const {
        size_t count = buffer_full_ ? FRAME_HISTORY_SIZE : current_index_;
        if (count == 0) return 0.0f;
        
        float sum = 0.0f;
        for (size_t i = 0; i < count; ++i) {
            sum += frame_times_[i];
        }
        return sum / count;
    }
    
    float getFPS() const {
        float avg = getRecentAverageFrameTime();
        return avg > 0.0f ? 1.0f / avg : 0.0f;
    }
    
    float getMinFrameTime() const {
        size_t count = buffer_full_ ? FRAME_HISTORY_SIZE : current_index_;
        if (count == 0) return 0.0f;
        
        float min_time = frame_times_[0];
        for (size_t i = 1; i < count; ++i) {
            if (frame_times_[i] < min_time) {
                min_time = frame_times_[i];
            }
        }
        return min_time;
    }
    
    float getMaxFrameTime() const {
        size_t count = buffer_full_ ? FRAME_HISTORY_SIZE : current_index_;
        if (count == 0) return 0.0f;
        
        float max_time = frame_times_[0];
        for (size_t i = 1; i < count; ++i) {
            if (frame_times_[i] > max_time) {
                max_time = frame_times_[i];
            }
        }
        return max_time;
    }
    
    float getTotalTime() const {
        return total_time_;
    }
    
    size_t getFrameCount() const {
        return frame_count_;
    }
    
    void printStats() const {
        std::cout << "Frame Time Stats:" << std::endl;
        std::cout << "  Average: " << std::fixed << std::setprecision(3) 
                  << getAverageFrameTime() * 1000.0f << "ms" << std::endl;
        std::cout << "  Recent:  " << getRecentAverageFrameTime() * 1000.0f << "ms" << std::endl;
        std::cout << "  Min:     " << getMinFrameTime() * 1000.0f << "ms" << std::endl;
        std::cout << "  Max:     " << getMaxFrameTime() * 1000.0f << "ms" << std::endl;
        std::cout << "  FPS:     " << std::setprecision(1) << getFPS() << std::endl;
        std::cout << "  Frames:  " << frame_count_ << std::endl;
    }
};

class PerformanceMonitor {
private:
    struct SystemStats {
        std::string name;
        float total_time = 0.0f;
        float min_time = std::numeric_limits<float>::max();
        float max_time = 0.0f;
        size_t call_count = 0;
        std::chrono::high_resolution_clock::time_point start_time;
    };
    
    std::unordered_map<std::string, SystemStats> system_stats_;
    FrameTimer frame_timer_;
    bool enabled_ = true;
    bool logging_enabled_ = false;
    std::ofstream log_file_;
    
public:
    PerformanceMonitor() = default;
    
    ~PerformanceMonitor() {
        if (log_file_.is_open()) {
            log_file_.close();
        }
    }
    
    void enable(bool enabled = true) {
        enabled_ = enabled;
    }
    
    void enableLogging(const std::string& filename) {
        if (log_file_.is_open()) {
            log_file_.close();
        }
        
        log_file_.open(filename);
        if (log_file_.is_open()) {
            logging_enabled_ = true;
            log_file_ << "timestamp,system_name,duration_ms,frame_time_ms,fps" << std::endl;
        }
    }
    
    void disableLogging() {
        logging_enabled_ = false;
        if (log_file_.is_open()) {
            log_file_.close();
        }
    }
    
    void startFrame() {
        frame_timer_.tick();
    }
    
    void startSystem(const std::string& name) {
        if (!enabled_) return;
        
        auto& stats = system_stats_[name];
        stats.name = name;
        stats.start_time = std::chrono::high_resolution_clock::now();
    }
    
    void endSystem(const std::string& name) {
        if (!enabled_) return;
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto& stats = system_stats_[name];
        
        float duration = std::chrono::duration<float, std::milli>(
            end_time - stats.start_time).count();
        
        stats.total_time += duration;
        stats.call_count++;
        
        if (duration < stats.min_time) {
            stats.min_time = duration;
        }
        if (duration > stats.max_time) {
            stats.max_time = duration;
        }
        
        if (logging_enabled_ && log_file_.is_open()) {
            auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            
            log_file_ << timestamp << "," << name << "," << duration << ","
                      << frame_timer_.getRecentAverageFrameTime() * 1000.0f << ","
                      << frame_timer_.getFPS() << std::endl;
        }
    }
    
    void reset() {
        system_stats_.clear();
        frame_timer_.reset();
    }
    
    void printReport() const {
        if (!enabled_) {
            std::cout << "Performance monitoring disabled" << std::endl;
            return;
        }
        
        std::cout << "\n=== Performance Report ===" << std::endl;
        
        frame_timer_.printStats();
        
        if (!system_stats_.empty()) {
            std::cout << "\nSystem Performance:" << std::endl;
            std::cout << "System                        | Avg (ms) | Min (ms) | Max (ms) | Calls" << std::endl;
            std::cout << "----------------------------------------------------------------------" << std::endl;
            
            for (const auto& [name, stats] : system_stats_) {
                float avg_time = stats.call_count > 0 ? stats.total_time / stats.call_count : 0.0f;
                
                std::cout << std::left << std::setw(30) << name
                          << "| " << std::setw(8) << std::fixed << std::setprecision(3) << avg_time
                          << " | " << std::setw(8) << stats.min_time
                          << " | " << std::setw(8) << stats.max_time
                          << " | " << std::setw(6) << stats.call_count << std::endl;
            }
        }
        
        std::cout << "===========================" << std::endl;
    }
    
    float getSystemAverageTime(const std::string& name) const {
        auto it = system_stats_.find(name);
        if (it != system_stats_.end() && it->second.call_count > 0) {
            return it->second.total_time / it->second.call_count;
        }
        return 0.0f;
    }
    
    float getFPS() const {
        return frame_timer_.getFPS();
    }
    
    float getFrameTime() const {
        return frame_timer_.getRecentAverageFrameTime() * 1000.0f;
    }
    
    const FrameTimer& getFrameTimer() const {
        return frame_timer_;
    }
};

class ScopedProfiler {
private:
    PerformanceMonitor& monitor_;
    std::string name_;
    
public:
    ScopedProfiler(PerformanceMonitor& monitor, const std::string& name)
        : monitor_(monitor), name_(name) {
        monitor_.startSystem(name_);
    }
    
    ~ScopedProfiler() {
        monitor_.endSystem(name_);
    }
};

#define PROFILE_SCOPE(monitor, name) \
    ScopedProfiler _scope_prof(monitor, name)

class MemoryTracker {
private:
    struct MemoryStats {
        size_t current_usage = 0;
        size_t peak_usage = 0;
        size_t allocation_count = 0;
        size_t deallocation_count = 0;
    };
    
    MemoryStats stats_;
    bool enabled_ = false;
    
public:
    void enable(bool enabled = true) {
        enabled_ = enabled;
    }
    
    void recordAllocation(size_t size) {
        if (!enabled_) return;
        
        stats_.current_usage += size;
        stats_.allocation_count++;
        
        if (stats_.current_usage > stats_.peak_usage) {
            stats_.peak_usage = stats_.current_usage;
        }
    }
    
    void recordDeallocation(size_t size) {
        if (!enabled_) return;
        
        stats_.current_usage = (stats_.current_usage >= size) ? 
                               stats_.current_usage - size : 0;
        stats_.deallocation_count++;
    }
    
    void reset() {
        stats_ = MemoryStats{};
    }
    
    size_t getCurrentUsage() const { return stats_.current_usage; }
    size_t getPeakUsage() const { return stats_.peak_usage; }
    size_t getAllocationCount() const { return stats_.allocation_count; }
    size_t getDeallocationCount() const { return stats_.deallocation_count; }
    
    void printStats() const {
        std::cout << "Memory Stats:" << std::endl;
        std::cout << "  Current:     " << (stats_.current_usage / 1024) << " KB" << std::endl;
        std::cout << "  Peak:        " << (stats_.peak_usage / 1024) << " KB" << std::endl;
        std::cout << "  Allocations: " << stats_.allocation_count << std::endl;
        std::cout << "  Deallocations: " << stats_.deallocation_count << std::endl;
        std::cout << "  Net allocations: " << (stats_.allocation_count - stats_.deallocation_count) << std::endl;
    }
};

class ECSPerformanceAnalyzer {
private:
    flecs::world& world_;
    PerformanceMonitor& monitor_;
    
public:
    ECSPerformanceAnalyzer(flecs::world& world, PerformanceMonitor& monitor)
        : world_(world), monitor_(monitor) {}
    
    void analyzeSystemPerformance() {
        auto systems = world_.query_builder<>()
            .with(flecs::System)
            .build();
        
        std::cout << "\n=== ECS System Analysis ===" << std::endl;
        
        systems.each([this](flecs::entity e) {
            if (e.has<flecs::System>()) {
                std::string name = e.name() ? e.name().c_str() : "Unnamed System";
                float avg_time = monitor_.getSystemAverageTime(name);
                
                std::cout << "System: " << name << " - Avg: " 
                          << std::fixed << std::setprecision(3) << avg_time << "ms" << std::endl;
            }
        });
        
        std::cout << "Total systems: " << systems.count() << std::endl;
        std::cout << "==========================" << std::endl;
    }
    
    void analyzeEntityCount() {
        size_t total_entities = world_.count();
        
        std::cout << "\n=== Entity Analysis ===" << std::endl;
        std::cout << "Total entities: " << total_entities << std::endl;
        
        std::cout << "Component breakdown:" << std::endl;
        std::cout << "  Transform: " << world_.count<Transform>() << std::endl;
        std::cout << "  Renderable: " << world_.count<Renderable>() << std::endl;
        std::cout << "  MovementPattern: " << world_.count<MovementPattern>() << std::endl;
        std::cout << "  Velocity: " << world_.count<Velocity>() << std::endl;
        std::cout << "  Lifetime: " << world_.count<Lifetime>() << std::endl;
        
        std::cout << "======================" << std::endl;
    }
    
    void fullAnalysis() {
        analyzeEntityCount();
        analyzeSystemPerformance();
        monitor_.printReport();
    }
};

} // namespace PerformanceTools