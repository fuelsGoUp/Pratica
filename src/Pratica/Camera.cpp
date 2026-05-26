#include "Camera.h"

Camera::Camera(glm::vec3 startPos, glm::vec3 startUp, float startYaw, float startPitch) :
front(glm::vec3(0.0,0.0,1.0)), movementSpeed(2.5f), mouseSensitivity(0.1f)
{
    position = startPos;
    worldUp = startUp;
    yaw = startYaw;
    pitch = startPitch;
    updateCameraVectors();
}

glm::mat4 Camera::getViewMatrix()
{
    return glm::lookAt(position, position + front, up);
}

void Camera::processKeyboard(const std::string &direction, float deltaTime)
{
    float velocity = movementSpeed * deltaTime;
    if (direction == "FORWARD") { position += front * velocity;}
    if (direction == "BACKWARD") { position -= front * velocity;}
    if (direction == "LEFT") { position -= right * velocity;}
    if (direction == "RIGHT") { position += right * velocity;}
}

void Camera::processMouseMovement(float xoffset, float yoffset, bool constrainPitch)
{
}

void Camera::updateCameraVectors()
{
    glm::vec3 newFront;
    newFront.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    newFront.y = sin(glm::radians(pitch));
    newFront.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    front = glm::normalize(newFront);
    right = glm::normalize(glm::cross(front,worldUp));
    up = glm::normalize(glm::cross(right,front));
}
