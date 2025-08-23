#pragma once

#include "event_bus.h"
#include "event_types.h"
#include "../components/component.h"
#include <flecs.h>
#include <functional>
#include <memory>
#include <vector>
#include <unordered_map>
#include <type_traits>
#include <concepts>

namespace Events {

// Forward declarations
template<typename EventType>
class ScopedEventListener;

template<typename EventType>
class ComponentEventListener;

class ECSEventSystem;

// =============================================================================
// RAII EVENT LISTENER WRAPPERS
// =============================================================================

// Basic RAII event listener - automatically unsubscribes on destruction
template<typename EventType>
class ScopedEventListener {
public:
    template<EventHandler<EventType> HandlerType>
    ScopedEventListener(EventBus& eventBus, HandlerType&& handler, const std::string& name = "")
        : eventBus_(&eventBus) {
        handle_ = eventBus.subscribe<EventType>(std::forward<HandlerType>(handler), name);
    }
    
    template<EventHandler<EventType> HandlerType, EventFilter<EventType> FilterType>
    ScopedEventListener(EventBus& eventBus, HandlerType&& handler, FilterType&& filter, const std::string& name = "")
        : eventBus_(&eventBus) {
        handle_ = eventBus.subscribeWithFilter<EventType>(std::forward<HandlerType>(handler), 
                                                         std::forward<FilterType>(filter), name);
    }
    
    // Non-copyable, moveable
    ScopedEventListener(const ScopedEventListener&) = delete;
    ScopedEventListener& operator=(const ScopedEventListener&) = delete;
    
    ScopedEventListener(ScopedEventListener&& other) noexcept 
        : eventBus_(other.eventBus_), handle_(std::move(other.handle_)) {
        other.eventBus_ = nullptr;
    }
    
    ScopedEventListener& operator=(ScopedEventListener&& other) noexcept {
        if (this != &other) {
            handle_ = std::move(other.handle_);
            eventBus_ = other.eventBus_;
            other.eventBus_ = nullptr;
        }
        return *this;
    }
    
    ~ScopedEventListener() {
        // Handle destructor takes care of unsubscribing
    }
    
    // Check if listener is still valid
    bool isValid() const { return handle_.isValid(); }
    
    // Manually unsubscribe
    void unsubscribe() { handle_.unsubscribe(); }
    
    // Get the underlying handle
    const EventListenerHandle& getHandle() const { return handle_; }

private:
    EventBus* eventBus_ = nullptr;
    EventListenerHandle handle_;
};

// =============================================================================
// LAMBDA-BASED EVENT CALLBACKS
// =============================================================================

namespace Lambda {
    // Factory functions for creating lambda-based event listeners
    
    template<typename EventType, typename LambdaType>
    requires EventHandler<LambdaType, EventType>
    ScopedEventListener<EventType> listen(EventBus& eventBus, LambdaType&& lambda, const std::string& name = "") {
        return ScopedEventListener<EventType>(eventBus, std::forward<LambdaType>(lambda), name);
    }
    
    template<typename EventType, typename LambdaType, typename FilterType>
    requires EventHandler<LambdaType, EventType> && EventFilter<FilterType, EventType>
    ScopedEventListener<EventType> listenWithFilter(EventBus& eventBus, LambdaType&& lambda, FilterType&& filter, const std::string& name = "") {
        return ScopedEventListener<EventType>(eventBus, std::forward<LambdaType>(lambda), std::forward<FilterType>(filter), name);
    }
    
    // Convenience functions for common patterns
    template<typename EventType>
    ScopedEventListener<EventType> listenOnce(EventBus& eventBus, std::function<void(const EventType&)> callback, const std::string& name = "") {
        return ScopedEventListener<EventType>(eventBus, [callback = std::move(callback)](const EventType& event) {
            callback(event);
        }, name);
    }
    
    template<typename EventType>
    ScopedEventListener<EventType> listenWithCondition(EventBus& eventBus, 
                                                      std::function<void(const EventType&)> callback,
                                                      std::function<bool(const EventType&)> condition,
                                                      const std::string& name = "") {
        return ScopedEventListener<EventType>(eventBus, 
                                            [callback = std::move(callback)](const EventType& event) {
                                                callback(event);
                                            },
                                            [condition = std::move(condition)](const Event<EventType>& event) {
                                                return condition(event.data);
                                            }, name);
    }
}

// =============================================================================
// COMPONENT-BASED EVENT LISTENERS FOR ECS INTEGRATION
// =============================================================================

// Component that holds event listener handles for automatic cleanup
template<typename EventType>
struct EventListenerComponent {
    std::vector<EventListenerHandle> handles;
    
    // Add a new listener to this component
    template<EventHandler<EventType> HandlerType>
    void addListener(EventBus& eventBus, HandlerType&& handler, const std::string& name = "") {
        handles.emplace_back(eventBus.subscribe<EventType>(std::forward<HandlerType>(handler), name));
    }
    
    // Add a filtered listener
    template<EventHandler<EventType> HandlerType, EventFilter<EventType> FilterType>
    void addFilteredListener(EventBus& eventBus, HandlerType&& handler, FilterType&& filter, const std::string& name = "") {
        handles.emplace_back(eventBus.subscribeWithFilter<EventType>(
            std::forward<HandlerType>(handler), 
            std::forward<FilterType>(filter), 
            name
        ));
    }
    
    // Remove all listeners
    void clearListeners() {
        for (auto& handle : handles) {
            handle.unsubscribe();
        }
        handles.clear();
    }
    
    // Get listener count
    size_t getListenerCount() const {
        return std::count_if(handles.begin(), handles.end(), 
                           [](const EventListenerHandle& handle) { return handle.isValid(); });
    }
};

// Entity-based event listener that automatically binds to entity lifecycle
template<typename EventType>
class ComponentEventListener {
public:
    ComponentEventListener(flecs::entity entity, EventBus& eventBus) 
        : entity_(entity), eventBus_(&eventBus) {}
    
    template<EventHandler<EventType> HandlerType>
    ComponentEventListener& listen(HandlerType&& handler, const std::string& name = "") {
        // Add listener component if it doesn't exist
        if (!entity_.has<EventListenerComponent<EventType>>()) {
            entity_.add<EventListenerComponent<EventType>>();
        }
        
        // Get the component and add the listener
        auto* component = entity_.get_mut<EventListenerComponent<EventType>>();
        component->addListener(*eventBus_, std::forward<HandlerType>(handler), name);
        
        return *this;
    }
    
    template<EventHandler<EventType> HandlerType, EventFilter<EventType> FilterType>
    ComponentEventListener& listenWithFilter(HandlerType&& handler, FilterType&& filter, const std::string& name = "") {
        // Add listener component if it doesn't exist
        if (!entity_.has<EventListenerComponent<EventType>>()) {
            entity_.add<EventListenerComponent<EventType>>();
        }
        
        // Get the component and add the listener
        auto* component = entity_.get_mut<EventListenerComponent<EventType>>();
        component->addFilteredListener(*eventBus_, 
                                     std::forward<HandlerType>(handler), 
                                     std::forward<FilterType>(filter), 
                                     name);
        
        return *this;
    }
    
    // Stop listening to this event type
    void stopListening() {
        if (entity_.has<EventListenerComponent<EventType>>()) {
            auto* component = entity_.get_mut<EventListenerComponent<EventType>>();
            component->clearListeners();
            entity_.remove<EventListenerComponent<EventType>>();
        }
    }
    
    bool isListening() const {
        return entity_.has<EventListenerComponent<EventType>>();
    }
    
    size_t getListenerCount() const {
        if (entity_.has<EventListenerComponent<EventType>>()) {
            const auto* component = entity_.get<EventListenerComponent<EventType>>();
            return component->getListenerCount();
        }
        return 0;
    }

private:
    flecs::entity entity_;
    EventBus* eventBus_;
};

// =============================================================================
// ECS EVENT SYSTEM INTEGRATION
// =============================================================================

// System that manages ECS-integrated event handling
class ECSEventSystem {
public:
    explicit ECSEventSystem(flecs::world& world, EventBus& eventBus);
    ~ECSEventSystem();
    
    void initialize();
    void shutdown();
    void update(float deltaTime);
    
    // Create a component event listener for an entity
    template<typename EventType>
    ComponentEventListener<EventType> createListener(flecs::entity entity) {
        return ComponentEventListener<EventType>(entity, eventBus_);
    }
    
    // Automatically publish ECS lifecycle events
    void enableLifecycleEvents(bool enable) { lifecycleEventsEnabled_ = enable; }
    bool areLifecycleEventsEnabled() const { return lifecycleEventsEnabled_; }
    
    // Configure which component changes to track
    template<typename ComponentType>
    void trackComponentChanges() {
        trackedComponents_.insert(std::type_index(typeid(ComponentType)));
        
        // Set up observer for this component type
        world_.observer<ComponentType>()
            .event(flecs::OnAdd)
            .each([this](flecs::entity e, ComponentType& c) {
                if (lifecycleEventsEnabled_) {
                    ComponentAddedEvent event;
                    event.entity = e;
                    event.componentType = typeid(ComponentType).name();
                    event.componentData = &c;
                    event.componentSize = sizeof(ComponentType);
                    eventBus_.publish<ComponentAddedEvent>(event);
                }
            });
        
        world_.observer<ComponentType>()
            .event(flecs::OnRemove)
            .each([this](flecs::entity e, ComponentType& c) {
                if (lifecycleEventsEnabled_) {
                    ComponentRemovedEvent event;
                    event.entity = e;
                    event.componentType = typeid(ComponentType).name();
                    eventBus_.publish<ComponentRemovedEvent>(event);
                }
            });
        
        world_.observer<ComponentType>()
            .event(flecs::OnSet)
            .each([this](flecs::entity e, ComponentType& c) {
                if (lifecycleEventsEnabled_) {
                    ComponentChangedEvent event;
                    event.entity = e;
                    event.componentType = typeid(ComponentType).name();
                    event.newData = &c;
                    event.componentSize = sizeof(ComponentType);
                    eventBus_.publish<ComponentChangedEvent>(event);
                }
            });
    }
    
    // Stop tracking component changes
    template<typename ComponentType>
    void stopTrackingComponentChanges() {
        trackedComponents_.erase(std::type_index(typeid(ComponentType)));
        // Note: Flecs observers are automatically cleaned up when world is destroyed
    }
    
    // Utility to publish input events from ECS input components
    void publishInputEvents();
    
    // Statistics
    size_t getTrackedComponentCount() const { return trackedComponents_.size(); }

private:
    void setupEntityLifecycleObservers();
    void cleanupEntityEventComponents(flecs::entity entity);
    
    flecs::world& world_;
    EventBus& eventBus_;
    
    bool lifecycleEventsEnabled_ = false;
    std::unordered_set<std::type_index> trackedComponents_;
    
    // ECS observers for cleanup
    flecs::observer entityCreatedObserver_;
    flecs::observer entityDestroyedObserver_;
};

// =============================================================================
// UTILITY FUNCTIONS AND HELPERS
// =============================================================================

namespace Utility {
    
    // Create multiple listeners at once with automatic cleanup
    template<typename... EventTypes>
    class MultiEventListener {
    public:
        template<typename... HandlerTypes>
        MultiEventListener(EventBus& eventBus, HandlerTypes&&... handlers) {
            static_assert(sizeof...(EventTypes) == sizeof...(HandlerTypes), 
                         "Number of event types must match number of handlers");
            addListeners<0>(eventBus, std::forward<HandlerTypes>(handlers)...);
        }
        
        ~MultiEventListener() = default;
        
        // Non-copyable, moveable
        MultiEventListener(const MultiEventListener&) = delete;
        MultiEventListener& operator=(const MultiEventListener&) = delete;
        MultiEventListener(MultiEventListener&&) = default;
        MultiEventListener& operator=(MultiEventListener&&) = default;
        
        bool allListenersValid() const {
            return (listeners_.at(std::type_index(typeid(EventTypes))).isValid() && ...);
        }
        
        void unsubscribeAll() {
            for (auto& [type, handle] : listeners_) {
                handle.unsubscribe();
            }
        }

    private:
        template<size_t Index, typename HandlerType, typename... RemainingHandlerTypes>
        void addListeners(EventBus& eventBus, HandlerType&& handler, RemainingHandlerTypes&&... remaining) {
            using EventType = std::tuple_element_t<Index, std::tuple<EventTypes...>>;
            listeners_[std::type_index(typeid(EventType))] = 
                eventBus.subscribe<EventType>(std::forward<HandlerType>(handler));
            
            if constexpr (sizeof...(RemainingHandlerTypes) > 0) {
                addListeners<Index + 1>(eventBus, std::forward<RemainingHandlerTypes>(remaining)...);
            }
        }
        
        std::unordered_map<std::type_index, EventListenerHandle> listeners_;
    };
    
    // Entity extension for convenient event listening
    class EntityEventHelper {
    public:
        explicit EntityEventHelper(flecs::entity entity, EventBus& eventBus) 
            : entity_(entity), eventBus_(&eventBus) {}
        
        template<typename EventType, EventHandler<EventType> HandlerType>
        EntityEventHelper& on(HandlerType&& handler, const std::string& name = "") {
            ComponentEventListener<EventType> listener(entity_, *eventBus_);
            listener.listen(std::forward<HandlerType>(handler), name);
            return *this;
        }
        
        template<typename EventType, EventHandler<EventType> HandlerType, EventFilter<EventType> FilterType>
        EntityEventHelper& onFiltered(HandlerType&& handler, FilterType&& filter, const std::string& name = "") {
            ComponentEventListener<EventType> listener(entity_, *eventBus_);
            listener.listenWithFilter(std::forward<HandlerType>(handler), std::forward<FilterType>(filter), name);
            return *this;
        }
        
        template<typename EventType>
        EntityEventHelper& off() {
            ComponentEventListener<EventType> listener(entity_, *eventBus_);
            listener.stopListening();
            return *this;
        }
        
        // Chain multiple event types
        template<typename EventType, typename... EventTypes>
        EntityEventHelper& offMultiple() {
            off<EventType>();
            if constexpr (sizeof...(EventTypes) > 0) {
                offMultiple<EventTypes...>();
            }
            return *this;
        }

    private:
        flecs::entity entity_;
        EventBus* eventBus_;
    };
    
    // Factory function to create entity event helper
    inline EntityEventHelper makeEventHelper(flecs::entity entity, EventBus& eventBus) {
        return EntityEventHelper(entity, eventBus);
    }
    
    // Batch event listener creation
    template<typename EventType>
    std::vector<ScopedEventListener<EventType>> createListeners(
        EventBus& eventBus,
        const std::vector<std::function<void(const EventType&)>>& handlers,
        const std::string& namePrefix = ""
    ) {
        std::vector<ScopedEventListener<EventType>> listeners;
        listeners.reserve(handlers.size());
        
        for (size_t i = 0; i < handlers.size(); ++i) {
            std::string name = namePrefix.empty() ? "" : namePrefix + "_" + std::to_string(i);
            listeners.emplace_back(eventBus, handlers[i], name);
        }
        
        return listeners;
    }
    
    // Event debugging utilities
    template<typename EventType>
    ScopedEventListener<EventType> createDebugListener(EventBus& eventBus, const std::string& debugName = "") {
        return ScopedEventListener<EventType>(eventBus, [debugName](const Event<EventType>& event) {
            std::cout << "[EVENT DEBUG] " << debugName << " received event of type " 
                     << typeid(EventType).name() << " at sequence " << event.sequenceId << std::endl;
        }, "Debug_" + debugName);
    }
    
    // Performance monitoring listener
    template<typename EventType>
    ScopedEventListener<EventType> createPerformanceListener(EventBus& eventBus, 
                                                            std::function<void(float)> performanceCallback) {
        return ScopedEventListener<EventType>(eventBus, [callback = std::move(performanceCallback)](const Event<EventType>& event) {
            auto now = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now - event.timestamp);
            callback(duration.count() / 1000.0f); // Convert to milliseconds
        }, "Performance_Monitor");
    }
}

} // namespace Events