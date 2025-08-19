#include "input_system.h"
#include "camera_system.h"
#include "../core/service_locator.h"
#include "../services/camera_service.h"
#include <iostream>

// Static InputContext instance owned by InputManager
static InputContext g_inputContext;

// Input processing system function - runs on entities with all input components
void input_processing_system(flecs::entity e, InputState& state, KeyboardInput& keyboard, 
                             MouseInput& mouse, InputEvents& events) {
    // Suppress unused parameter warnings
    (void)keyboard;
    (void)mouse;
    (void)events;
    
    // Update frame info
    state.deltaTime = e.world().delta_time();
    state.frameNumber++;
    
    // Note: Frame state clearing is handled manually in main.cpp after all input processing
}

// Input manager functions
namespace InputManager {
    
    InputContext& getContext() {
        return g_inputContext;
    }
    
    flecs::entity createInputEntity(flecs::world& world) {
        auto entity = world.entity("InputManager")
            .set<InputState>({})
            .set<KeyboardInput>({})
            .set<MouseInput>({})
            .set<InputEvents>({});
            
        std::cout << "Input entity created" << std::endl;
        return entity;
    }
    
    void setWindow(SDL_Window* window) {
        g_inputContext.window = window;
    }
    
    static void handleKeyboardEvent(const SDL_Event& event, KeyboardInput& keyboard) {
        int scancode = event.key.scancode;
        if (scancode < 0 || static_cast<size_t>(scancode) >= KeyboardInput::MAX_KEYS) {
            return;
        }
        
        bool isDown = (event.type == SDL_EVENT_KEY_DOWN);
        bool wasDown = keyboard.keys[scancode];
        
        keyboard.keys[scancode] = isDown;
        
        // Track frame-specific state changes
        if (isDown && !wasDown) {
            keyboard.keysPressed[scancode] = true;
        } else if (!isDown && wasDown) {
            keyboard.keysReleased[scancode] = true;
        }
        
        // Update modifier states
        keyboard.shift = (SDL_GetModState() & SDL_KMOD_SHIFT) != 0;
        keyboard.ctrl = (SDL_GetModState() & SDL_KMOD_CTRL) != 0;
        keyboard.alt = (SDL_GetModState() & SDL_KMOD_ALT) != 0;
    }
    
    static void handleMouseEvent(const SDL_Event& event, MouseInput& mouse) {
        switch (event.type) {
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP: {
                int button = event.button.button - 1; // SDL uses 1-based indexing
                if (button < 0 || static_cast<size_t>(button) >= MouseInput::MAX_BUTTONS) {
                    return;
                }
                
                bool isDown = (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN);
                bool wasDown = mouse.buttons[button];
                
                mouse.buttons[button] = isDown;
                
                if (isDown && !wasDown) {
                    mouse.buttonsPressed[button] = true;
                } else if (!isDown && wasDown) {
                    mouse.buttonsReleased[button] = true;
                }
                
                // Update position and previous position to stay consistent with motion events
                glm::vec2 buttonPos(event.button.x, event.button.y);
                mouse.position = buttonPos;
                g_inputContext.previousMousePos = buttonPos;
                break;
            }
            
            case SDL_EVENT_MOUSE_MOTION: {
                glm::vec2 newPos(event.motion.x, event.motion.y);
                
                if (g_inputContext.mouseInitialized) {
                    mouse.deltaPosition = newPos - g_inputContext.previousMousePos;
                } else {
                    mouse.deltaPosition = glm::vec2(0.0f, 0.0f);
                    g_inputContext.mouseInitialized = true;
                }
                
                mouse.position = newPos;
                g_inputContext.previousMousePos = newPos;
                break;
            }
            
            case SDL_EVENT_MOUSE_WHEEL:
                mouse.wheelDelta = glm::vec2(event.wheel.x, event.wheel.y);
                break;
        }
    }
    
    void processSDLEvents(flecs::world& world) {
        auto inputEntity = world.lookup("InputManager");
        if (!inputEntity.is_valid()) {
            return;
        }
        
        auto* inputState = inputEntity.get_mut<InputState>();
        auto* keyboard = inputEntity.get_mut<KeyboardInput>();
        auto* mouse = inputEntity.get_mut<MouseInput>();
        auto* events = inputEntity.get_mut<InputEvents>();
        
        if (!inputState || !keyboard || !mouse || !events) {
            return;
        }
        
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            // Add event to event buffer for systems that need raw events
            InputEvents::Event inputEvent;
            bool addEvent = true;
            
            switch (event.type) {
                case SDL_EVENT_QUIT:
                    inputState->quit = true;
                    inputEvent.type = InputEvents::Event::QUIT;
                    break;
                    
                case SDL_EVENT_KEY_DOWN:
                case SDL_EVENT_KEY_UP:
                    if (inputState->processKeyboard) {
                        handleKeyboardEvent(event, *keyboard);
                        inputEvent.type = (event.type == SDL_EVENT_KEY_DOWN) ? 
                            InputEvents::Event::KEY_DOWN : InputEvents::Event::KEY_UP;
                        inputEvent.keyEvent.key = event.key.scancode;
                        inputEvent.keyEvent.repeat = event.key.repeat;
                    }
                    break;
                    
                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                case SDL_EVENT_MOUSE_BUTTON_UP:
                    if (inputState->processMouse) {
                        handleMouseEvent(event, *mouse);
                        inputEvent.type = (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) ?
                            InputEvents::Event::MOUSE_BUTTON_DOWN : InputEvents::Event::MOUSE_BUTTON_UP;
                        inputEvent.mouseButtonEvent.button = event.button.button - 1; // SDL uses 1-based indexing
                        inputEvent.mouseButtonEvent.position = glm::vec2(event.button.x, event.button.y);
                    }
                    break;
                    
                case SDL_EVENT_MOUSE_MOTION:
                    if (inputState->processMouse) {
                        handleMouseEvent(event, *mouse);
                        inputEvent.type = InputEvents::Event::MOUSE_MOTION;
                        inputEvent.mouseMotionEvent.position = glm::vec2(event.motion.x, event.motion.y);
                        inputEvent.mouseMotionEvent.delta = glm::vec2(event.motion.xrel, event.motion.yrel);
                    }
                    break;
                    
                case SDL_EVENT_MOUSE_WHEEL:
                    if (inputState->processMouse) {
                        handleMouseEvent(event, *mouse);
                        inputEvent.type = InputEvents::Event::MOUSE_WHEEL;
                        inputEvent.mouseWheelEvent.delta = glm::vec2(event.wheel.x, event.wheel.y);
                    }
                    break;
                    
                case SDL_EVENT_WINDOW_RESIZED:
                    inputEvent.type = InputEvents::Event::WINDOW_RESIZE;
                    inputEvent.windowResizeEvent.width = event.window.data1;
                    inputEvent.windowResizeEvent.height = event.window.data2;
                    break;
                    
                default:
                    addEvent = false;
                    break;
            }
            
            if (addEvent) {
                events->addEvent(inputEvent);
            }
        }
        
        // Update mouse world position
        mouse->worldPosition = screenToWorld(mouse->position, world);
    }
    
    glm::vec2 screenToWorld(const glm::vec2& screenPos, flecs::world& world) {
        // Get actual window size
        glm::vec2 screenSize(800.0f, 600.0f); // Default fallback
        if (g_inputContext.window) {
            int width, height;
            // Mouse coords are in logical window space, so use logical size
            SDL_GetWindowSize(g_inputContext.window, &width, &height);
            screenSize = glm::vec2(static_cast<float>(width), static_cast<float>(height));
        }
        
        // Use camera service for proper screen-to-world transformation
        auto cameraService = ServiceLocator::instance().getService<CameraService>();
        glm::vec2 worldPos = cameraService->screenToWorld(glm::vec3(screenPos, 0.0f), screenSize);
        return worldPos;
    }
}

// Helper function implementations
namespace InputQuery {
    
    static flecs::entity getInputEntity(flecs::world& world) {
        return world.lookup("InputManager");
    }
    
    bool isKeyDown(flecs::world& world, int scancode) {
        auto inputEntity = getInputEntity(world);
        if (!inputEntity.is_valid()) return false;
        
        const auto* keyboard = inputEntity.get<KeyboardInput>();
        return keyboard ? keyboard->isKeyDown(scancode) : false;
    }
    
    bool isKeyPressed(flecs::world& world, int scancode) {
        auto inputEntity = getInputEntity(world);
        if (!inputEntity.is_valid()) return false;
        
        const auto* keyboard = inputEntity.get<KeyboardInput>();
        return keyboard ? keyboard->isKeyPressed(scancode) : false;
    }
    
    bool isKeyReleased(flecs::world& world, int scancode) {
        auto inputEntity = getInputEntity(world);
        if (!inputEntity.is_valid()) return false;
        
        const auto* keyboard = inputEntity.get<KeyboardInput>();
        return keyboard ? keyboard->isKeyReleased(scancode) : false;
    }
    
    bool isMouseButtonDown(flecs::world& world, int button) {
        auto inputEntity = getInputEntity(world);
        if (!inputEntity.is_valid()) return false;
        
        const auto* mouse = inputEntity.get<MouseInput>();
        return mouse ? mouse->isButtonDown(button) : false;
    }
    
    bool isMouseButtonPressed(flecs::world& world, int button) {
        auto inputEntity = getInputEntity(world);
        if (!inputEntity.is_valid()) return false;
        
        const auto* mouse = inputEntity.get<MouseInput>();
        return mouse ? mouse->isButtonPressed(button) : false;
    }
    
    bool isMouseButtonReleased(flecs::world& world, int button) {
        auto inputEntity = getInputEntity(world);
        if (!inputEntity.is_valid()) return false;
        
        const auto* mouse = inputEntity.get<MouseInput>();
        return mouse ? mouse->isButtonReleased(button) : false;
    }
    
    glm::vec2 getMousePosition(flecs::world& world) {
        auto inputEntity = getInputEntity(world);
        if (!inputEntity.is_valid()) return glm::vec2(0.0f);
        
        const auto* mouse = inputEntity.get<MouseInput>();
        return mouse ? mouse->position : glm::vec2(0.0f);
    }
    
    glm::vec2 getMouseWorldPosition(flecs::world& world) {
        auto inputEntity = getInputEntity(world);
        if (!inputEntity.is_valid()) return glm::vec2(0.0f);
        
        const auto* mouse = inputEntity.get<MouseInput>();
        return mouse ? mouse->worldPosition : glm::vec2(0.0f);
    }
    
    glm::vec2 getMouseDelta(flecs::world& world) {
        auto inputEntity = getInputEntity(world);
        if (!inputEntity.is_valid()) return glm::vec2(0.0f);
        
        const auto* mouse = inputEntity.get<MouseInput>();
        return mouse ? mouse->deltaPosition : glm::vec2(0.0f);
    }
    
    glm::vec2 getMouseWheelDelta(flecs::world& world) {
        auto inputEntity = getInputEntity(world);
        if (!inputEntity.is_valid()) return glm::vec2(0.0f);
        
        const auto* mouse = inputEntity.get<MouseInput>();
        return mouse ? mouse->wheelDelta : glm::vec2(0.0f);
    }
    
    bool shouldQuit(flecs::world& world) {
        auto inputEntity = getInputEntity(world);
        if (!inputEntity.is_valid()) return false;
        
        const auto* inputState = inputEntity.get<InputState>();
        return inputState ? inputState->quit : false;
    }
}