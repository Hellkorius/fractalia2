#include "../component.hpp"
#include <flecs.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

void movement_system(flecs::entity e, Position& pos, Velocity& vel) {
    const float deltaTime = e.world().delta_time();
    
    pos.x += vel.x * deltaTime;
    pos.y += vel.y * deltaTime;
    pos.z += vel.z * deltaTime;
    
    // Bounce off screen edges
    if (pos.x > 2.0f || pos.x < -2.0f) vel.x = -vel.x;
    if (pos.y > 1.5f || pos.y < -1.5f) vel.y = -vel.y;
}

void rotation_system(flecs::entity e, Position& pos, Rotation& rot) {
    const float deltaTime = e.world().delta_time();
    
    // Update rotation angle
    rot.angle += deltaTime * 2.0f; // Rotate at 2 radians per second
    
    // Keep angle in [0, 2π] range
    if (rot.angle > 2.0f * M_PI) {
        rot.angle -= 2.0f * M_PI;
    }
    
    // Create rotation matrix and apply to position for orbital movement
    glm::mat4 rotationMatrix = glm::rotate(glm::mat4(1.0f), rot.angle * 0.5f, rot.axis);
    glm::vec4 rotatedPos = rotationMatrix * glm::vec4(0.5f, 0.0f, 0.0f, 1.0f);
    
    // Add orbital motion to base position
    pos.x = rotatedPos.x;
    pos.y = rotatedPos.y;
}