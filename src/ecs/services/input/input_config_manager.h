#pragma once

#include <string>

// Forward declarations
class InputActionSystem;
class InputContextManager;

// Input configuration manager - handles loading/saving configuration and defaults
class InputConfigManager {
public:
    InputConfigManager();
    ~InputConfigManager();
    
    // Initialization
    bool initialize(InputActionSystem* actionSystem, InputContextManager* contextManager);
    void cleanup();
    
    // Configuration file operations
    void loadInputConfig(const std::string& configFile);
    void saveInputConfig(const std::string& configFile);
    void resetToDefaults();
    
    // Default setup
    void createDefaultContexts();
    void createDefaultActions();

private:
    // Module references
    InputActionSystem* actionSystem = nullptr;
    InputContextManager* contextManager = nullptr;
    
    bool initialized = false;
    
    // Helper methods for default setup
    void setupMovementActions();
    void setupMouseActions();
    void setupSystemActions();
};