# ECS Events System

## Overview
This folder implements a comprehensive event system that bridges ECS lifecycle events, input handling, and inter-system communication. (Central event dispatching architecture for decoupled system communication)

## File Structure

### event_bus.h
**Inputs**: Template event types, handler functions, filter predicates, processing mode preferences, subscription configurations.
**Outputs**: EventListenerHandle objects for subscription management, queued/immediate event dispatch to registered handlers, thread-safe event processing with priority ordering. Manages complete event lifecycle from publication through handler execution with automatic cleanup and statistics tracking.

### event_listeners.h  
**Inputs**: EventBus instances, Flecs entities, handler lambdas, filter conditions, subscription lifetime parameters.
**Outputs**: RAII wrapper classes (ScopedEventListener, ComponentEventListener) that automatically manage subscription lifecycles, ECS integration components that bind event handling to entity destruction, utility helpers for multi-event listening patterns. Provides component-based event listening that integrates with Flecs entity lifecycle.

### event_types.h
**Inputs**: SDL input events, ECS component changes, system state transitions, rendering pipeline notifications.
**Outputs**: Strongly-typed event structures covering input (keyboard/mouse), entity lifecycle (create/destroy/component changes), physics (collisions/triggers), camera (position/zoom/view changes), system events (initialization/shutdown), rendering (frame/pass events), and performance monitoring. Defines complete event taxonomy for engine subsystems.