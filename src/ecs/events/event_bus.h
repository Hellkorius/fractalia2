#pragma once

#include <functional>
#include <vector>
#include <queue>
#include <unordered_map>
#include <memory>
#include <typeindex>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <concepts>
#include <thread>
#include <algorithm>
#include <chrono>

namespace Events {

// Forward declarations
template<typename EventType>
class TypedEventHandler;

class EventBus;

// Event priority levels for controlling dispatch order
enum class EventPriority : uint8_t {
    Immediate = 0,    // Highest priority - dispatched immediately
    High = 1,         // High priority - dispatched before normal events
    Normal = 2,       // Default priority - standard event processing
    Low = 3,          // Low priority - dispatched after normal events
    Deferred = 4      // Lowest priority - deferred to next frame
};

// Event processing mode
enum class ProcessingMode {
    Immediate,        // Process event right away (blocking)
    Deferred,         // Queue event for later processing
    Conditional       // Use priority to determine immediate vs deferred
};

// Base event interface for type erasure
class BaseEvent {
public:
    virtual ~BaseEvent() = default;
    virtual std::type_index getType() const = 0;
    virtual std::unique_ptr<BaseEvent> clone() const = 0;
    
    EventPriority priority = EventPriority::Normal;
    std::chrono::steady_clock::time_point timestamp = std::chrono::steady_clock::now();
    uint64_t sequenceId = 0;
    bool consumed = false;
    
    // Event metadata for filtering and debugging
    std::string source;
    std::unordered_map<std::string, std::string> metadata;
};

// Typed event wrapper
template<typename T>
class Event : public BaseEvent {
public:
    T data;
    
    template<typename... Args>
    explicit Event(Args&&... args) : data(std::forward<Args>(args)...) {}
    
    std::type_index getType() const override {
        return std::type_index(typeid(T));
    }
    
    std::unique_ptr<BaseEvent> clone() const override {
        return std::make_unique<Event<T>>(data);
    }
};

// Event handler concept
template<typename T, typename EventType>
concept EventHandler = std::invocable<T, const Event<EventType>&> || 
                      std::invocable<T, const EventType&>;

// Event filter concept
template<typename T, typename EventType>
concept EventFilter = std::invocable<T, const Event<EventType>&> &&
                     std::convertible_to<std::invoke_result_t<T, const Event<EventType>&>, bool>;

// Type-safe event listener handle
class EventListenerHandle {
public:
    EventListenerHandle() = default;
    EventListenerHandle(uint64_t id, std::type_index type, EventBus* bus)
        : id_(id), type_(type), bus_(bus), valid_(true) {}
    
    EventListenerHandle(const EventListenerHandle&) = delete;
    EventListenerHandle& operator=(const EventListenerHandle&) = delete;
    
    EventListenerHandle(EventListenerHandle&& other) noexcept
        : id_(other.id_), type_(other.type_), bus_(other.bus_), valid_(other.valid_.load()) {
        other.valid_ = false;
        other.bus_ = nullptr;
    }
    
    EventListenerHandle& operator=(EventListenerHandle&& other) noexcept {
        if (this != &other) {
            unsubscribe();
            id_ = other.id_;
            type_ = other.type_;
            bus_ = other.bus_;
            valid_ = other.valid_.load();
            other.valid_ = false;
            other.bus_ = nullptr;
        }
        return *this;
    }
    
    ~EventListenerHandle() {
        unsubscribe();
    }
    
    bool isValid() const { return valid_.load(); }
    uint64_t getId() const { return id_; }
    std::type_index getType() const { return type_; }
    
    void unsubscribe();
    
private:
    uint64_t id_ = 0;
    std::type_index type_ = std::type_index(typeid(void));
    EventBus* bus_ = nullptr;
    std::atomic<bool> valid_{false};
};

// Event listener container
struct EventListener {
    uint64_t id;
    std::function<void(const BaseEvent&)> handler;
    std::function<bool(const BaseEvent&)> filter;
    EventPriority minPriority = EventPriority::Deferred;
    EventPriority maxPriority = EventPriority::Immediate;
    bool oneShot = false;
    std::chrono::steady_clock::time_point expiryTime = std::chrono::steady_clock::time_point::max();
    std::string name; // For debugging
    std::atomic<bool> enabled{true};
    
    bool shouldHandle(const BaseEvent& event) const {
        if (!enabled.load()) return false;
        if (event.priority < minPriority || event.priority > maxPriority) return false;
        if (std::chrono::steady_clock::now() > expiryTime) return false;
        return !filter || filter(event);
    }
};

// Event queue entry for deferred processing
struct QueuedEvent {
    std::unique_ptr<BaseEvent> event;
    std::type_index type;
    
    QueuedEvent(std::unique_ptr<BaseEvent> e, std::type_index t)
        : event(std::move(e)), type(t) {}
    
    // Priority queue comparator (higher priority = lower number = processed first)
    bool operator<(const QueuedEvent& other) const {
        if (event->priority != other.event->priority) {
            return event->priority > other.event->priority; // Reverse for min-heap
        }
        return event->sequenceId > other.event->sequenceId; // FIFO for same priority
    }
};

// Thread-safe, high-performance event bus
class EventBus {
public:
    EventBus();
    ~EventBus();
    
    // Event publishing
    template<typename EventType, typename... Args>
    void publish(ProcessingMode mode = ProcessingMode::Conditional, Args&&... args) {
        auto event = std::make_unique<Event<EventType>>(std::forward<Args>(args)...);
        event->sequenceId = nextSequenceId_.fetch_add(1);
        
        const auto eventType = std::type_index(typeid(EventType));
        
        // Determine processing mode
        bool immediate = false;
        switch (mode) {
            case ProcessingMode::Immediate:
                immediate = true;
                break;
            case ProcessingMode::Deferred:
                immediate = false;
                break;
            case ProcessingMode::Conditional:
                immediate = (event->priority <= EventPriority::High);
                break;
        }
        
        if (immediate) {
            dispatchImmediate(*event, eventType);
        } else {
            queueDeferred(std::move(event), eventType);
        }
    }
    
    template<typename EventType>
    void publish(const EventType& eventData, ProcessingMode mode = ProcessingMode::Conditional) {
        publish<EventType>(mode, eventData);
    }
    
    template<typename EventType>
    void publish(EventType&& eventData, ProcessingMode mode = ProcessingMode::Conditional) {
        publish<EventType>(mode, std::move(eventData));
    }
    
    // Event subscription
    template<typename EventType, EventHandler<EventType> HandlerType>
    EventListenerHandle subscribe(HandlerType&& handler, const std::string& name = "") {
        return subscribeWithFilter<EventType>(std::forward<HandlerType>(handler), nullptr, name);
    }
    
    template<typename EventType, EventHandler<EventType> HandlerType, EventFilter<EventType> FilterType>
    EventListenerHandle subscribeWithFilter(HandlerType&& handler, FilterType&& filter, 
                                           const std::string& name = "") {
        
        auto listener = std::make_shared<EventListener>();
        listener->id = nextListenerId_.fetch_add(1);
        listener->name = name.empty() ? ("Listener_" + std::to_string(listener->id)) : name;
        
        // Wrap handler to handle both Event<T> and T parameter types
        if constexpr (std::invocable<HandlerType, const Event<EventType>&>) {
            listener->handler = [handler = std::forward<HandlerType>(handler)](const BaseEvent& baseEvent) {
                const auto& typedEvent = static_cast<const Event<EventType>&>(baseEvent);
                handler(typedEvent);
            };
        } else {
            listener->handler = [handler = std::forward<HandlerType>(handler)](const BaseEvent& baseEvent) {
                const auto& typedEvent = static_cast<const Event<EventType>&>(baseEvent);
                handler(typedEvent.data);
            };
        }
        
        // Wrap filter if provided
        if constexpr (!std::is_null_pointer_v<std::decay_t<FilterType>>) {
            listener->filter = [filter = std::forward<FilterType>(filter)](const BaseEvent& baseEvent) -> bool {
                const auto& typedEvent = static_cast<const Event<EventType>&>(baseEvent);
                return filter(typedEvent);
            };
        }
        
        const auto eventType = std::type_index(typeid(EventType));
        
        {
            std::unique_lock lock(listenersMutex_);
            listeners_[eventType].push_back(listener);
        }
        
        return EventListenerHandle(listener->id, eventType, this);
    }
    
    // Subscription with priority filtering
    template<typename EventType, EventHandler<EventType> HandlerType>
    EventListenerHandle subscribeWithPriority(HandlerType&& handler, 
                                             EventPriority minPriority = EventPriority::Deferred,
                                             EventPriority maxPriority = EventPriority::Immediate,
                                             const std::string& name = "") {
        auto handle = subscribe<EventType>(std::forward<HandlerType>(handler), name);
        
        // Set priority range
        {
            std::shared_lock lock(listenersMutex_);
            auto& typeListeners = listeners_[std::type_index(typeid(EventType))];
            for (auto& listener : typeListeners) {
                if (listener->id == handle.getId()) {
                    listener->minPriority = minPriority;
                    listener->maxPriority = maxPriority;
                    break;
                }
            }
        }
        
        return handle;
    }
    
    // One-shot subscription (automatically unsubscribes after first event)
    template<typename EventType, EventHandler<EventType> HandlerType>
    EventListenerHandle subscribeOnce(HandlerType&& handler, const std::string& name = "") {
        auto handle = subscribe<EventType>(std::forward<HandlerType>(handler), name);
        
        // Mark as one-shot
        {
            std::shared_lock lock(listenersMutex_);
            auto& typeListeners = listeners_[std::type_index(typeid(EventType))];
            for (auto& listener : typeListeners) {
                if (listener->id == handle.getId()) {
                    listener->oneShot = true;
                    break;
                }
            }
        }
        
        return handle;
    }
    
    // Timed subscription (automatically expires after duration)
    template<typename EventType, EventHandler<EventType> HandlerType>
    EventListenerHandle subscribeFor(HandlerType&& handler, 
                                    std::chrono::steady_clock::duration duration,
                                    const std::string& name = "") {
        auto handle = subscribe<EventType>(std::forward<HandlerType>(handler), name);
        
        // Set expiry time
        auto expiryTime = std::chrono::steady_clock::now() + duration;
        {
            std::shared_lock lock(listenersMutex_);
            auto& typeListeners = listeners_[std::type_index(typeid(EventType))];
            for (auto& listener : typeListeners) {
                if (listener->id == handle.getId()) {
                    listener->expiryTime = expiryTime;
                    break;
                }
            }
        }
        
        return handle;
    }
    
    // Unsubscribe by handle
    bool unsubscribe(uint64_t listenerId, std::type_index eventType);
    
    // Enable/disable listener
    bool setListenerEnabled(uint64_t listenerId, std::type_index eventType, bool enabled);
    
    // Event processing
    void processDeferred(size_t maxEvents = 0); // Process queued events (0 = all)
    void processUntilEmpty(); // Process all queued events
    size_t getQueueSize() const;
    
    // Event filtering
    template<typename EventType>
    void setGlobalFilter(std::function<bool(const Event<EventType>&)> filter) {
        std::unique_lock lock(globalFiltersMutex_);
        if (filter) {
            globalFilters_[std::type_index(typeid(EventType))] = 
                [filter](const BaseEvent& baseEvent) -> bool {
                    const auto& typedEvent = static_cast<const Event<EventType>&>(baseEvent);
                    return filter(typedEvent);
                };
        } else {
            globalFilters_.erase(std::type_index(typeid(EventType)));
        }
    }
    
    template<typename EventType>
    void removeGlobalFilter() {
        std::unique_lock lock(globalFiltersMutex_);
        globalFilters_.erase(std::type_index(typeid(EventType)));
    }
    
    // Statistics and debugging
    struct Statistics {
        std::atomic<uint64_t> eventsPublished{0};
        std::atomic<uint64_t> eventsProcessed{0};
        std::atomic<uint64_t> eventsFiltered{0};
        std::atomic<uint64_t> immediateEvents{0};
        std::atomic<uint64_t> deferredEvents{0};
        std::atomic<size_t> activeListeners{0};
        std::atomic<size_t> queueSize{0};
        
        void reset() {
            eventsPublished = 0;
            eventsProcessed = 0;
            eventsFiltered = 0;
            immediateEvents = 0;
            deferredEvents = 0;
            activeListeners = 0;
            queueSize = 0;
        }
    };
    
    const Statistics& getStatistics() const { return stats_; }
    void resetStatistics() { stats_.reset(); }
    
    // Get listener count for debugging
    size_t getListenerCount() const;
    size_t getListenerCount(std::type_index eventType) const;
    
    // Clear all events and listeners
    void clear();
    void clearEvents(); // Clear only queued events
    void clearListeners(); // Clear only listeners
    
    // Thread safety utilities
    void enableThreadSafety(bool enable) { threadSafetyEnabled_ = enable; }
    bool isThreadSafetyEnabled() const { return threadSafetyEnabled_; }

private:
    // Internal dispatch methods
    void dispatchImmediate(const BaseEvent& event, std::type_index eventType);
    void queueDeferred(std::unique_ptr<BaseEvent> event, std::type_index eventType);
    bool passesGlobalFilter(const BaseEvent& event, std::type_index eventType) const;
    void cleanupExpiredListeners();
    
    // Thread safety
    mutable std::shared_mutex listenersMutex_;
    mutable std::shared_mutex queueMutex_;
    mutable std::shared_mutex globalFiltersMutex_;
    std::atomic<bool> threadSafetyEnabled_{true};
    
    // Event listeners grouped by type
    std::unordered_map<std::type_index, std::vector<std::shared_ptr<EventListener>>> listeners_;
    
    // Deferred event queue (priority queue)
    std::priority_queue<QueuedEvent> deferredQueue_;
    
    // Global event filters
    std::unordered_map<std::type_index, std::function<bool(const BaseEvent&)>> globalFilters_;
    
    // ID generation
    std::atomic<uint64_t> nextListenerId_{1};
    std::atomic<uint64_t> nextSequenceId_{1};
    
    // Statistics
    mutable Statistics stats_;
    
    // Cleanup management
    std::chrono::steady_clock::time_point lastCleanup_ = std::chrono::steady_clock::now();
    static constexpr std::chrono::seconds CLEANUP_INTERVAL{30}; // Cleanup expired listeners every 30s
};

// Global event bus instance (optional convenience)
namespace Global {
    EventBus& getEventBus();
    void setEventBus(std::unique_ptr<EventBus> eventBus);
}

} // namespace Events