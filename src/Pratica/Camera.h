#ifndef CAMERA_H
#define CAMERA_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <vector>

class Camera {
public:
    // Vetores de estado da câmera
    glm::vec3 position;
    glm::vec3 front;
    glm::vec3 up;
    glm::vec3 right;
    glm::vec3 worldUp;

    // Ângulos de Euler para rotação
    float yaw;
    float pitch;

    // Configurações de movimentação
    float movementSpeed;
    float mouseSensitivity;

    // Construtor com valores padrão (apontando para a origem)
    Camera(glm::vec3 startPos = glm::vec3(0.0f, 0.0f, -3.0f), 
           glm::vec3 startUp = glm::vec3(0.0f, 1.0f, 0.0f), 
           float startYaw = 90.0f, 
           float startPitch = 0.0f);

    // Retorna a matriz de View calculada
    glm::mat4 getViewMatrix();

    // Processa entrada de teclado (WASD)
    void processKeyboard(const std::string& direction, float deltaTime);

    // Processa movimento do mouse (Pitch/Yaw)
    void processMouseMovement(float xoffset, float yoffset, bool constrainPitch = true);

private:
    // Atualiza os vetores Front, Right e Up com base nos ângulos atuais
    void updateCameraVectors();
};

#endif