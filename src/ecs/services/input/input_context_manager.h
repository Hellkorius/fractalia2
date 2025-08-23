#pragma once

#include "input_types.h"
#include <string>
#include <unordered_map>
#include <vector>

// Input context definition - groups bindings with priority
struct InputContextDefinition {
    std::string name;
    std::unordered_map<std::string, std::vector<InputBinding>> actionBindings;
    int priority = 0;  // Higher priority contexts override lower ones
    bool active = true;
};

// Input context manager - handles context switching, priority, and binding resolution
class InputContextManager {
public:
    InputContextManager();
    ~InputContextManager();
    
    // Initialization
    bool initialize();
    void cleanup();
    
    // Context registration and management
    void registerContext(const std::string& name, int priority = 0);
    void setContextActive(const std::string& contextName, bool active);
    void pushContext(const std::string& contextName);  // Temporarily activate
    void popContext();  // Return to previous context stack
    std::string getCurrentContext() const;
    
    // Binding management
    void bindAction(const std::string& contextName, const std::string& actionName, const InputBinding& binding);
    void unbindAction(const std::string& contextName, const std::string& actionName);
    void clearActionBindings(const std::string& actionName);
    
    // Binding resolution
    std::vector<InputBinding> getActionBindings(const std::string& actionName) const;
    std::vector<InputBinding> getActionBindings(const std::string& contextName, const std::string& actionName) const;
    
    // Debug and introspection
    std::vector<std::string> getActiveContexts() const;
    std::vector<std::string> getAllContexts() const;
    bool isContextActive(const std::string& contextName) const;
    int getContextPriority(const std::string& contextName) const;
    void printContextState() const;
    
    // Access to context definitions (for configuration)
    const std::unordered_map<std::string, InputContextDefinition>& getContextDefinitions() const { return contexts; }
    std::unordered_map<std::string, InputContextDefinition>& getContextDefinitions() { return contexts; }

private:
    // Context data
    std::unordered_map<std::string, InputContextDefinition> contexts;
    std::vector<std::string> contextStack;
    std::string activeContextName = "default";
    
    bool initialized = false;
    
    // Helper methods
    std::vector<std::pair<int, InputContextDefinition*>> getSortedActiveContexts() const;
};