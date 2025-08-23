#pragma once

#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>

// Input action types
enum class InputActionType {
    DIGITAL,    // On/Off (keyboard keys, mouse buttons)
    ANALOG_1D,  // Single axis (mouse wheel, trigger)
    ANALOG_2D   // Two axes (mouse position, stick)
};

// Input binding - maps physical input to logical action
struct InputBinding {
    enum class InputType {
        KEYBOARD_KEY,
        MOUSE_BUTTON,
        MOUSE_AXIS_X,
        MOUSE_AXIS_Y,
        MOUSE_WHEEL_X,
        MOUSE_WHEEL_Y
    };
    
    InputType inputType;
    union {
        int keycode;        // For keyboard
        int mouseButton;    // For mouse buttons
        int axisIndex;      // For analog inputs
    };
    
    // Modifiers and settings
    bool requiresShift = false;
    bool requiresCtrl = false;
    bool requiresAlt = false;
    float deadzone = 0.1f;      // For analog inputs
    float sensitivity = 1.0f;   // Multiplier for analog inputs
    bool invertAxis = false;    // Invert analog input
    
    InputBinding(InputType type, int value) : inputType(type) {
        switch (type) {
            case InputType::KEYBOARD_KEY:
                keycode = value;
                break;
            case InputType::MOUSE_BUTTON:
                mouseButton = value;
                break;
            default:
                axisIndex = value;
                break;
        }
    }
};

// Input action definition
struct InputActionDefinition {
    std::string name;
    InputActionType type;
    std::string description;
    std::vector<InputBinding> defaultBindings;
};

// Input action state
struct InputActionState {
    InputActionType type;
    bool digitalValue = false;
    float analogValue1D = 0.0f;
    glm::vec2 analogValue2D{0.0f, 0.0f};
    bool justPressed = false;
    bool justReleased = false;
    float duration = 0.0f;  // How long the action has been active
};