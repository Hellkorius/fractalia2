#include "input_context_manager.h"
#include <iostream>
#include <algorithm>

InputContextManager::InputContextManager() {
}

InputContextManager::~InputContextManager() {
    cleanup();
}

bool InputContextManager::initialize() {
    if (initialized) {
        return true;
    }
    
    // Create default context
    registerContext("default", 0);
    setContextActive("default", true);
    
    initialized = true;
    return true;
}

void InputContextManager::cleanup() {
    if (!initialized) {
        return;
    }
    
    contexts.clear();
    contextStack.clear();
    activeContextName = "default";
    initialized = false;
}

void InputContextManager::registerContext(const std::string& name, int priority) {
    InputContextDefinition context;
    context.name = name;
    context.priority = priority;
    context.active = false;
    contexts[name] = context;
}

void InputContextManager::setContextActive(const std::string& contextName, bool active) {
    auto it = contexts.find(contextName);
    if (it != contexts.end()) {
        it->second.active = active;
        if (active) {
            activeContextName = contextName;
        }
    }
}

void InputContextManager::pushContext(const std::string& contextName) {
    if (contexts.find(contextName) != contexts.end()) {
        contextStack.push_back(activeContextName);
        setContextActive(contextName, true);
    }
}

void InputContextManager::popContext() {
    if (!contextStack.empty()) {
        std::string previousContext = contextStack.back();
        contextStack.pop_back();
        setContextActive(previousContext, true);
    }
}

std::string InputContextManager::getCurrentContext() const {
    return activeContextName;
}

void InputContextManager::bindAction(const std::string& contextName, const std::string& actionName, const InputBinding& binding) {
    auto contextIt = contexts.find(contextName);
    if (contextIt != contexts.end()) {
        contextIt->second.actionBindings[actionName].push_back(binding);
    }
}

void InputContextManager::unbindAction(const std::string& contextName, const std::string& actionName) {
    auto contextIt = contexts.find(contextName);
    if (contextIt != contexts.end()) {
        contextIt->second.actionBindings.erase(actionName);
    }
}

void InputContextManager::clearActionBindings(const std::string& actionName) {
    for (auto& [contextName, context] : contexts) {
        context.actionBindings.erase(actionName);
    }
}

std::vector<InputBinding> InputContextManager::getActionBindings(const std::string& actionName) const {
    std::vector<InputBinding> allBindings;
    
    // Get bindings from active contexts (highest priority first)
    auto sortedContexts = getSortedActiveContexts();
    
    for (const auto& [priority, context] : sortedContexts) {
        auto bindingIt = context->actionBindings.find(actionName);
        if (bindingIt != context->actionBindings.end()) {
            for (const auto& binding : bindingIt->second) {
                allBindings.push_back(binding);
            }
        }
    }
    
    return allBindings;
}

std::vector<InputBinding> InputContextManager::getActionBindings(const std::string& contextName, const std::string& actionName) const {
    auto contextIt = contexts.find(contextName);
    if (contextIt != contexts.end()) {
        auto bindingIt = contextIt->second.actionBindings.find(actionName);
        if (bindingIt != contextIt->second.actionBindings.end()) {
            return bindingIt->second;
        }
    }
    return {};
}

std::vector<std::string> InputContextManager::getActiveContexts() const {
    std::vector<std::string> activeContexts;
    for (const auto& [contextName, context] : contexts) {
        if (context.active) {
            activeContexts.push_back(contextName);
        }
    }
    
    // Sort by priority (highest first)
    std::sort(activeContexts.begin(), activeContexts.end(),
             [this](const std::string& a, const std::string& b) {
                 return getContextPriority(a) > getContextPriority(b);
             });
    
    return activeContexts;
}

std::vector<std::string> InputContextManager::getAllContexts() const {
    std::vector<std::string> allContexts;
    allContexts.reserve(contexts.size());
    for (const auto& [contextName, context] : contexts) {
        allContexts.push_back(contextName);
    }
    return allContexts;
}

bool InputContextManager::isContextActive(const std::string& contextName) const {
    auto it = contexts.find(contextName);
    return it != contexts.end() && it->second.active;
}

int InputContextManager::getContextPriority(const std::string& contextName) const {
    auto it = contexts.find(contextName);
    return it != contexts.end() ? it->second.priority : 0;
}

void InputContextManager::printContextState() const {
    std::cout << "=== Context Manager State ===" << std::endl;
    std::cout << "Active Context: " << activeContextName << std::endl;
    std::cout << "Context Stack: ";
    for (const auto& context : contextStack) {
        std::cout << context << " -> ";
    }
    std::cout << "current" << std::endl;
    
    std::cout << "All Contexts:" << std::endl;
    auto sortedContexts = getSortedActiveContexts();
    for (const auto& [priority, context] : sortedContexts) {
        std::cout << "  " << context->name << " (priority=" << priority << ", active=" << context->active << ")" << std::endl;
    }
}

std::vector<std::pair<int, InputContextDefinition*>> InputContextManager::getSortedActiveContexts() const {
    std::vector<std::pair<int, InputContextDefinition*>> sortedContexts;
    for (const auto& [contextName, context] : contexts) {
        if (context.active) {
            sortedContexts.emplace_back(context.priority, const_cast<InputContextDefinition*>(&context));
        }
    }
    
    std::sort(sortedContexts.begin(), sortedContexts.end(),
             [](const auto& a, const auto& b) { return a.first > b.first; });
    
    return sortedContexts;
}