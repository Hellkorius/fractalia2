#pragma once

#include <chrono>
#include <string>

// Template-based statistics provider interface to unify statistics collection
template<typename StatsType>
class StatisticsProvider {
public:
    virtual ~StatisticsProvider() = default;
    
    // Core statistics interface
    virtual StatsType getStats() const = 0;
    virtual void updateStats() = 0;
    virtual void resetStats() = 0;
    
    // Statistics metadata
    virtual std::string getStatsDescription() const = 0;
    virtual std::chrono::steady_clock::time_point getLastUpdateTime() const = 0;
    
    // Optional performance monitoring
    virtual bool isUnderPressure() const { return false; }
    virtual bool tryOptimize() { return true; }
    
protected:
    // Helper for updating timestamp in derived classes
    void updateLastUpdateTime() {
        lastUpdateTime = std::chrono::steady_clock::now();
    }
    
private:
    std::chrono::steady_clock::time_point lastUpdateTime{};
};

// Base statistics structure that common fields can inherit from
struct BaseStats {
    std::chrono::steady_clock::time_point collectionTime{};
    bool isValid = false;
    
    void markValid() {
        isValid = true;
        collectionTime = std::chrono::steady_clock::now();
    }
    
    void markInvalid() {
        isValid = false;
    }
};

// Statistics aggregator for combining multiple providers
template<typename... StatsTypes>
class StatisticsAggregator {
public:
    // Update all registered providers
    void updateAll() {
        (updateProvider<StatsTypes>(), ...);
    }
    
    // Get stats from specific provider type
    template<typename StatsType>
    StatsType getStats(StatisticsProvider<StatsType>* provider) const {
        if (provider) {
            return provider->getStats();
        }
        return StatsType{}; // Return default-constructed stats
    }
    
    // Register a statistics provider
    template<typename StatsType>
    void registerProvider(StatisticsProvider<StatsType>* provider) {
        // Store provider reference for updates
        // Implementation would depend on specific storage needs
    }

private:
    template<typename StatsType>
    void updateProvider() {
        // Update specific provider type
        // Implementation would depend on how providers are stored
    }
};