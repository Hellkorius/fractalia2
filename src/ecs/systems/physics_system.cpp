#include "../component.hpp"
#include <flecs.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

void movement_system(flecs::entity e, Transform& transform, Velocity& vel) {
    const float deltaTime = e.world().delta_time();
    
    glm::vec3 newPos = transform.position;
    newPos += vel.linear * deltaTime;
    
    // Bounce off screen edges
    if (newPos.x > 2.0f || newPos.x < -2.0f) vel.linear.x = -vel.linear.x;
    if (newPos.y > 1.5f || newPos.y < -1.5f) vel.linear.y = -vel.linear.y;
    
    // Update transform
    transform.setPosition(newPos);
    
    // Apply angular velocity to rotation
    if (glm::length(vel.angular) > 0.0f) {
        glm::vec3 newRot = transform.rotation + vel.angular * deltaTime;
        transform.setRotation(newRot);
    }
}

void lifetime_system(flecs::entity e, Lifetime& lifetime) {
    const float deltaTime = e.world().delta_time();
    
    if (lifetime.maxAge > 0.0f) {
        lifetime.currentAge += deltaTime;
        
        if (lifetime.autoDestroy && lifetime.currentAge >= lifetime.maxAge) {
            e.destruct();
        }
    }
}