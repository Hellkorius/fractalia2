#pragma once

#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <iostream>
#include <fstream>
#include <thread>
#include <mutex>
#include <iomanip>
#include <algorithm>
#include <limits>

// High-resolution timer for performance profiling
class ProfileTimer {
private:
    std::chrono::high_resolution_clock::time_point startTime;
    std::chrono::high_resolution_clock::time_point endTime;
    bool running{false};
    
public:
    void start() {
        startTime = std::chrono::high_resolution_clock::now();
        running = true;
    }
    
    void stop() {
        if (running) {
            endTime = std::chrono::high_resolution_clock::now();
            running = false;
        }
    }
    
    float getMilliseconds() const {
        if (running) {
            auto now = std::chrono::high_resolution_clock::now();
            return std::chrono::duration<float, std::milli>(now - startTime).count();
        }
        return std::chrono::duration<float, std::milli>(endTime - startTime).count();
    }
    
    float getMicroseconds() const {
        if (running) {
            auto now = std::chrono::high_resolution_clock::now();
            return std::chrono::duration<float, std::micro>(now - startTime).count();
        }
        return std::chrono::duration<float, std::micro>(endTime - startTime).count();
    }
    
    bool isRunning() const { return running; }
};

// RAII profiler scope for automatic timing
class ProfileScope {
private:
    class Profiler* profiler;
    std::string name;
    ProfileTimer timer;
    
public:
    ProfileScope(class Profiler* p, const std::string& scopeName);
    ~ProfileScope();
};

// Performance data collector and analyzer
class Profiler {
private:
    struct ProfileData {
        std::string name;
        float totalTime{0.0f};
        float minTime{std::numeric_limits<float>::max()};
        float maxTime{0.0f};
        size_t callCount{0};
        std::vector<float> recentTimes;
        static constexpr size_t MAX_RECENT = 100;
        
        void addSample(float time) {
            totalTime += time;
            minTime = std::min(minTime, time);
            maxTime = std::max(maxTime, time);
            callCount++;
            
            if (recentTimes.size() >= MAX_RECENT) {
                recentTimes.erase(recentTimes.begin());
            }
            recentTimes.push_back(time);
        }
        
        float getAverageTime() const {
            return callCount > 0 ? totalTime / callCount : 0.0f;
        }
        
        float getRecentAverageTime() const {
            if (recentTimes.empty()) return 0.0f;
            float sum = 0.0f;
            for (float time : recentTimes) {
                sum += time;
            }
            return sum / recentTimes.size();
        }
    };
    
    std::unordered_map<std::string, std::unique_ptr<ProfileData>> profiles;
    mutable std::mutex profileMutex;
    bool enabled{true};
    
    // Frame timing
    ProfileTimer frameTimer;
    float targetFrameTime{16.67f}; // 60 FPS
    size_t frameCount{0};
    
    // Memory tracking
    size_t peakMemoryUsage{0};
    size_t currentMemoryUsage{0};
    
public:
    static Profiler& getInstance() {
        static Profiler instance;
        return instance;
    }
    
    void setEnabled(bool enable) { enabled = enable; }
    bool isEnabled() const { return enabled; }
    
    // Manual profiling
    void beginProfile(const std::string& name) {
        if (!enabled) return;
        
        std::lock_guard<std::mutex> lock(profileMutex);
        auto it = profiles.find(name);
        if (it == profiles.end()) {
            profiles[name] = std::make_unique<ProfileData>();
            profiles[name]->name = name;
        }
    }
    
    void endProfile(const std::string& name, float timeMs) {
        if (!enabled) return;
        
        std::lock_guard<std::mutex> lock(profileMutex);
        auto it = profiles.find(name);
        if (it != profiles.end()) {
            it->second->addSample(timeMs);
        }
    }
    
    // Scope-based profiling
    ProfileScope createScope(const std::string& name) {
        return ProfileScope(this, name);
    }
    
    // Frame timing
    void beginFrame() {
        frameTimer.start();
    }
    
    void endFrame() {
        frameTimer.stop();
        float frameTime = frameTimer.getMilliseconds();
        frameCount++;
        
        // Track frame timing
        endProfile("Frame", frameTime);
        
        // Log performance warnings
        if (frameTime > targetFrameTime * 1.5f) {
            std::cout << "Performance Warning: Frame took " << frameTime 
                      << "ms (target: " << targetFrameTime << "ms)" << std::endl;
        }
    }
    
    void setTargetFrameTime(float ms) { targetFrameTime = ms; }
    
    // Memory tracking
    void updateMemoryUsage(size_t bytes) {
        currentMemoryUsage = bytes;
        peakMemoryUsage = std::max(peakMemoryUsage, bytes);
    }
    
    // Reporting
    struct ProfileReport {
        std::string name;
        float averageTime;
        float recentAverageTime;
        float minTime;
        float maxTime;
        size_t callCount;
        float percentOfFrame;
    };
    
    std::vector<ProfileReport> generateReport() const {
        std::lock_guard<std::mutex> lock(profileMutex);
        std::vector<ProfileReport> report;
        
        // Get frame time for percentage calculations
        float frameTime = 16.67f; // Default
        auto frameIt = profiles.find("Frame");
        if (frameIt != profiles.end()) {
            frameTime = frameIt->second->getRecentAverageTime();
        }
        
        for (const auto& [name, data] : profiles) {
            if (data->callCount > 0) {
                ProfileReport entry;
                entry.name = name;
                entry.averageTime = data->getAverageTime();
                entry.recentAverageTime = data->getRecentAverageTime();
                entry.minTime = data->minTime;
                entry.maxTime = data->maxTime;
                entry.callCount = data->callCount;
                entry.percentOfFrame = (entry.recentAverageTime / frameTime) * 100.0f;
                
                report.push_back(entry);
            }
        }
        
        // Sort by recent average time (descending)
        std::sort(report.begin(), report.end(), 
                 [](const ProfileReport& a, const ProfileReport& b) {
                     return a.recentAverageTime > b.recentAverageTime;
                 });
        
        return report;
    }
    
    void printReport() const {
        auto report = generateReport();
        
        std::cout << "\n=== Performance Report ===" << std::endl;
        std::cout << "Profile Name" << std::setw(20) << "Avg(ms)" << std::setw(12) 
                  << "Recent(ms)" << std::setw(12) << "Min(ms)" << std::setw(12) 
                  << "Max(ms)" << std::setw(12) << "Calls" << std::setw(12) 
                  << "% Frame" << std::endl;
        std::cout << std::string(80, '-') << std::endl;
        
        for (const auto& entry : report) {
            std::cout << std::left << std::setw(20) << entry.name
                      << std::right << std::fixed << std::setprecision(2)
                      << std::setw(12) << entry.averageTime
                      << std::setw(12) << entry.recentAverageTime
                      << std::setw(12) << entry.minTime
                      << std::setw(12) << entry.maxTime
                      << std::setw(12) << entry.callCount
                      << std::setw(11) << entry.percentOfFrame << "%"
                      << std::endl;
        }
        
        std::cout << "\nMemory Usage:" << std::endl;
        std::cout << "  Current: " << (currentMemoryUsage / 1024 / 1024) << " MB" << std::endl;
        std::cout << "  Peak: " << (peakMemoryUsage / 1024 / 1024) << " MB" << std::endl;
        std::cout << "  Frames: " << frameCount << std::endl;
        std::cout << "=========================" << std::endl;
    }
    
    void exportToCSV(const std::string& filename) const {
        std::ofstream file(filename);
        if (!file.is_open()) return;
        
        auto report = generateReport();
        
        file << "Name,AverageTime,RecentAverageTime,MinTime,MaxTime,CallCount,PercentOfFrame" << std::endl;
        
        for (const auto& entry : report) {
            file << entry.name << ","
                 << entry.averageTime << ","
                 << entry.recentAverageTime << ","
                 << entry.minTime << ","
                 << entry.maxTime << ","
                 << entry.callCount << ","
                 << entry.percentOfFrame << std::endl;
        }
        
        file.close();
    }
    
    // Reset statistics
    void reset() {
        std::lock_guard<std::mutex> lock(profileMutex);
        profiles.clear();
        frameCount = 0;
        peakMemoryUsage = 0;
        currentMemoryUsage = 0;
    }
    
    // Quick stats access
    float getFrameTime() const {
        std::lock_guard<std::mutex> lock(profileMutex);
        auto it = profiles.find("Frame");
        if (it != profiles.end() && it->second->callCount > 0) {
            float frameTime = it->second->getRecentAverageTime();
            return frameTime > 0.0f ? frameTime : targetFrameTime;
        }
        return targetFrameTime; // Return target frame time as fallback
    }
    
    size_t getFrameCount() const { return frameCount; }
    size_t getCurrentMemoryUsage() const { return currentMemoryUsage; }
    size_t getPeakMemoryUsage() const { return peakMemoryUsage; }
};

// RAII ProfileScope implementation
inline ProfileScope::ProfileScope(Profiler* p, const std::string& scopeName) 
    : profiler(p), name(scopeName) {
    if (profiler && profiler->isEnabled()) {
        profiler->beginProfile(name);
        timer.start();
    }
}

inline ProfileScope::~ProfileScope() {
    if (profiler && profiler->isEnabled()) {
        timer.stop();
        profiler->endProfile(name, timer.getMilliseconds());
    }
}

// Convenience macros for profiling
#define PROFILE_SCOPE(name) auto _prof_scope = Profiler::getInstance().createScope(name)
#define PROFILE_FUNCTION() PROFILE_SCOPE(__FUNCTION__)
#define PROFILE_BEGIN_FRAME() Profiler::getInstance().beginFrame()
#define PROFILE_END_FRAME() Profiler::getInstance().endFrame()