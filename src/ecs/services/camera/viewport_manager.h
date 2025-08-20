#pragma once

#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include <vector>

using CameraID = uint32_t;

struct Viewport {
    std::string name;
    CameraID cameraID = 0;
    
    glm::vec2 offset{0.0f, 0.0f};
    glm::vec2 size{1.0f, 1.0f};
    
    bool active = true;
    int renderOrder = 0;
    glm::vec4 clearColor{0.0f, 0.0f, 0.0f, 1.0f};
    bool clearColorBuffer = true;
    bool clearDepthBuffer = true;
    
    glm::vec4 getScreenRect(const glm::vec2& screenSize) const {
        return glm::vec4(
            offset.x * screenSize.x,
            offset.y * screenSize.y,
            size.x * screenSize.x,
            size.y * screenSize.y
        );
    }
    
    bool containsPoint(const glm::vec2& screenPoint, const glm::vec2& screenSize) const {
        glm::vec4 rect = getScreenRect(screenSize);
        return screenPoint.x >= rect.x && screenPoint.x <= rect.x + rect.z &&
               screenPoint.y >= rect.y && screenPoint.y <= rect.y + rect.w;
    }
};

class ViewportManager {
public:
    ViewportManager() = default;
    ~ViewportManager() = default;

    void createViewport(const std::string& name, CameraID cameraID, const glm::vec2& offset, const glm::vec2& size);
    void createViewport(const Viewport& viewport);
    void removeViewport(const std::string& name);
    void setViewportActive(const std::string& name, bool active);
    
    bool hasViewport(const std::string& name) const;
    Viewport* getViewport(const std::string& name);
    const Viewport* getViewport(const std::string& name) const;
    
    std::vector<Viewport*> getActiveViewports();
    std::vector<const Viewport*> getActiveViewports() const;
    std::vector<Viewport*> getViewportsByRenderOrder();
    std::vector<const Viewport*> getViewportsByRenderOrder() const;
    
    void printViewportInfo() const;
    
    void setScreenSize(const glm::vec2& size) { screenSize = size; }
    glm::vec2 getScreenSize() const { return screenSize; }
    
    Viewport* findViewportAtScreenPoint(const glm::vec2& screenPoint);
    const Viewport* findViewportAtScreenPoint(const glm::vec2& screenPoint) const;

private:
    std::unordered_map<std::string, Viewport> viewports;
    glm::vec2 screenSize{800.0f, 600.0f};
};