#include "viewport_manager.h"
#include <iostream>
#include <algorithm>

void ViewportManager::createViewport(const std::string& name, CameraID cameraID, const glm::vec2& offset, const glm::vec2& size) {
    Viewport viewport;
    viewport.name = name;
    viewport.cameraID = cameraID;
    viewport.offset = offset;
    viewport.size = size;
    
    createViewport(viewport);
}

void ViewportManager::createViewport(const Viewport& viewport) {
    viewports[viewport.name] = viewport;
}

void ViewportManager::removeViewport(const std::string& name) {
    viewports.erase(name);
}

void ViewportManager::setViewportActive(const std::string& name, bool active) {
    auto it = viewports.find(name);
    if (it != viewports.end()) {
        it->second.active = active;
    }
}

bool ViewportManager::hasViewport(const std::string& name) const {
    return viewports.find(name) != viewports.end();
}

Viewport* ViewportManager::getViewport(const std::string& name) {
    auto it = viewports.find(name);
    return (it != viewports.end()) ? &it->second : nullptr;
}

const Viewport* ViewportManager::getViewport(const std::string& name) const {
    auto it = viewports.find(name);
    return (it != viewports.end()) ? &it->second : nullptr;
}

std::vector<Viewport*> ViewportManager::getActiveViewports() {
    std::vector<Viewport*> active;
    
    for (auto& [name, viewport] : viewports) {
        if (viewport.active) {
            active.push_back(&viewport);
        }
    }
    
    return active;
}

std::vector<const Viewport*> ViewportManager::getActiveViewports() const {
    std::vector<const Viewport*> active;
    
    for (const auto& [name, viewport] : viewports) {
        if (viewport.active) {
            active.push_back(&viewport);
        }
    }
    
    return active;
}

std::vector<Viewport*> ViewportManager::getViewportsByRenderOrder() {
    std::vector<Viewport*> sorted = getActiveViewports();
    
    std::sort(sorted.begin(), sorted.end(), [](const Viewport* a, const Viewport* b) {
        return a->renderOrder < b->renderOrder;
    });
    
    return sorted;
}

std::vector<const Viewport*> ViewportManager::getViewportsByRenderOrder() const {
    std::vector<const Viewport*> sorted = getActiveViewports();
    
    std::sort(sorted.begin(), sorted.end(), [](const Viewport* a, const Viewport* b) {
        return a->renderOrder < b->renderOrder;
    });
    
    return sorted;
}

void ViewportManager::printViewportInfo() const {
    std::cout << "Viewports (" << viewports.size() << " total):\n";
    
    for (const auto& [name, viewport] : viewports) {
        std::cout << "  " << name << ":\n";
        std::cout << "    Camera ID: " << viewport.cameraID << "\n";
        std::cout << "    Active: " << (viewport.active ? "Yes" : "No") << "\n";
        std::cout << "    Offset: (" << viewport.offset.x << ", " << viewport.offset.y << ")\n";
        std::cout << "    Size: (" << viewport.size.x << ", " << viewport.size.y << ")\n";
        std::cout << "    Render Order: " << viewport.renderOrder << "\n";
        
        glm::vec4 screenRect = viewport.getScreenRect(screenSize);
        std::cout << "    Screen Rect: (" << screenRect.x << ", " << screenRect.y << ", " << screenRect.z << ", " << screenRect.w << ")\n";
    }
}

Viewport* ViewportManager::findViewportAtScreenPoint(const glm::vec2& screenPoint) {
    for (auto& [name, viewport] : viewports) {
        if (viewport.active && viewport.containsPoint(screenPoint, screenSize)) {
            return &viewport;
        }
    }
    return nullptr;
}

const Viewport* ViewportManager::findViewportAtScreenPoint(const glm::vec2& screenPoint) const {
    for (const auto& [name, viewport] : viewports) {
        if (viewport.active && viewport.containsPoint(screenPoint, screenSize)) {
            return &viewport;
        }
    }
    return nullptr;
}