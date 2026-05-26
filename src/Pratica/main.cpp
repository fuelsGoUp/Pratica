#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <array>
#include <filesystem>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Camera.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"  

using namespace std;

// ---------- STRUCTS ----------
struct MeshData {
    float* vertices;
    int vertexCount;
};

struct Objeto {
    GLuint VAO;
    int vertexCount;

    glm::vec3 pos;
    glm::vec3 rot;
    glm::vec3 scale;

    // material
    glm::vec3 ka;
    glm::vec3 kd;
    glm::vec3 ks;
    float shininess;
};

// ---------- GLOBAL ----------
Objeto objetos[4];
int totalObjetos = 3;
int selecionado = 0;

GLuint loadTexture(const std::string& filePath) {
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_set_flip_vertically_on_load(true);
    namespace fs = std::filesystem;
    fs::path texFile = fs::path(__FILE__).parent_path() / filePath;

    int width, height, nrChannels;
    unsigned char* data = stbi_load(texFile.string().c_str(), &width, &height, &nrChannels, 0);
    if (data) {
        if (nrChannels == 3) {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
        } else {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        }
        glGenerateMipmap(GL_TEXTURE_2D);
    } else {
        std::cout << "Textura não encontrada: " << texFile.string() << std::endl;
    }

    stbi_image_free(data);
    glBindTexture(GL_TEXTURE_2D, 0);
    return textureID;
}

enum Modo { ROTATE, TRANSLATE, SCALE };
Modo modoAtual = ROTATE;

bool perspective = true;
bool wireframe = false;

// câmera
Camera camera(glm::vec3(0.0f, 0.0f, 5.0f), glm::vec3(0.0f, 1.0f, 0.0f), -90.0f, 0.0f);
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// luz
glm::vec3 lightPos(3.0f, 3.0f, 3.0f);

// ---------- SHADERS ----------
const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 texCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;

void main() {
    FragPos = vec3(model * vec4(position, 1.0));
    Normal = mat3(transpose(inverse(model))) * normal;
    TexCoord = texCoord;
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
)";
// Fragment shader implementando o modelo de iluminação Phong
const char* fragmentShaderSource = R"(
#version 330 core

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;

out vec4 color;

// luz
uniform vec3 lightPos;
uniform vec3 viewPos;
uniform vec3 lightColor;

uniform bool isWireframe;
uniform vec3 wireframeColor;

// material
uniform vec3 ka;
uniform vec3 kd;
uniform vec3 ks;
uniform float shininess;
uniform sampler2D texture1;

void main() {
    if (isWireframe) {
        color = vec4(wireframeColor, 1.0);
        return;
    }

    vec3 ambient = ka * lightColor;

    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = kd * diff * lightColor;

    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
    vec3 specular = ks * spec * lightColor;

    vec3 result = ambient + diffuse + specular;
    vec3 texColor = texture(texture1, TexCoord).rgb;
    color = vec4(result * texColor, 1.0);
}
)";

// ---------- LOADER OBJ ----------

// Função para carregar um arquivo OBJ, retornando um MeshData com os vértices e normais intercalados
MeshData loadOBJ(const char* path)
{
    namespace fs = std::filesystem;
    fs::path objFile = fs::path(__FILE__).parent_path() / path;
    ifstream file(objFile);
    if (!file.is_open()) {
        cout << "Erro ao abrir OBJ\n";
        return { nullptr, 0 };
    }
    // Vetores temporários para posições, normais, UVs e índices
    vector<glm::vec3> tempV;
    vector<glm::vec2> tempVT;
    vector<glm::vec3> tempVN;
    vector<int> vIndex;
    vector<int> vtIndex;
    vector<int> vnIndex;
    // Ler o arquivo linha por linha
    string line;
    while (getline(file, line)) {
        stringstream ss(line);
        string type;
        ss >> type;

        if (type == "v") {
            glm::vec3 v;
            ss >> v.x >> v.y >> v.z;
            tempV.push_back(v);
        }
        else if (type == "vt") {
            glm::vec2 uv;
            ss >> uv.x >> uv.y;
            tempVT.push_back(uv);
        }
        else if (type == "vn") {
            glm::vec3 n;
            ss >> n.x >> n.y >> n.z;
            tempVN.push_back(n);
        }
        else if (type == "f") {
            string v1, v2, v3;
            ss >> v1 >> v2 >> v3;

            vector<string> verts = { v1, v2, v3 };
            // Cada vértice tem formato "vIndex/vtIndex/vnIndex" ou "vIndex//vnIndex"
            for (auto& token : verts) {
                string a, b, c;
                stringstream vs(token);
                getline(vs, a, '/');
                getline(vs, b, '/');
                getline(vs, c, '/');

                vIndex.push_back(stoi(a) - 1);
                vtIndex.push_back(b.empty() ? -1 : stoi(b) - 1);
                vnIndex.push_back(c.empty() ? -1 : stoi(c) - 1);
            }
        }
    }

    int count = vIndex.size();
    float* vertices = new float[count * 8];
    int idx = 0;
    for (int i = 0; i < count; i++) {
        glm::vec3 v = tempV[vIndex[i]];
        glm::vec3 n = (vnIndex[i] >= 0) ? tempVN[vnIndex[i]] : glm::vec3(0, 1, 0);
        glm::vec2 uv = (vtIndex[i] >= 0) ? tempVT[vtIndex[i]] : glm::vec2(0.0f, 0.0f);

        vertices[idx++] = v.x;
        vertices[idx++] = v.y;
        vertices[idx++] = v.z;

        vertices[idx++] = n.x;
        vertices[idx++] = n.y;
        vertices[idx++] = n.z;

        vertices[idx++] = uv.x;
        vertices[idx++] = uv.y;
    }

    return { vertices, count };
}

// ---------- VAO ----------

// Cria um VAO para o mesh, intercalando posições e normais (6 floats por vértice)
GLuint createVAO(MeshData mesh)
{
    GLuint VAO, VBO;

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, mesh.vertexCount * 8 * sizeof(float), mesh.vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    return VAO;
}

// ---------- INPUT ----------
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    // Seleção de objeto e modo
    if (key == GLFW_KEY_TAB && action == GLFW_PRESS)
        selecionado = (selecionado + 1) % totalObjetos;

    if (key == GLFW_KEY_R && action == GLFW_PRESS)
        modoAtual = ROTATE;

    if (key == GLFW_KEY_T && action == GLFW_PRESS)
        modoAtual = TRANSLATE;

    if (key == GLFW_KEY_S && action == GLFW_PRESS)
        modoAtual = SCALE;

    if (key == GLFW_KEY_P && action == GLFW_PRESS)
        perspective = !perspective;

    if (key == GLFW_KEY_M && action == GLFW_PRESS)
        wireframe = !wireframe;

    // Movimento da câmera (WASD)
    float speed = 2.0f * deltaTime;
    if (key == GLFW_KEY_W && action != GLFW_RELEASE)
        camera.processKeyboard("FORWARD", deltaTime);
    if (key == GLFW_KEY_A && action != GLFW_RELEASE)
        camera.processKeyboard("LEFT", deltaTime);
    if (key == GLFW_KEY_S && action != GLFW_RELEASE)
        camera.processKeyboard("BACKWARD", deltaTime);
    if (key == GLFW_KEY_D && action != GLFW_RELEASE)
        camera.processKeyboard("RIGHT", deltaTime);

    // Rotação (X, Y, Z) - Modo R
    if (modoAtual == ROTATE) {
        if (key == GLFW_KEY_X && action != GLFW_RELEASE)
            objetos[selecionado].rot.x += speed * 3.14f;
        if (key == GLFW_KEY_Y && action != GLFW_RELEASE)
            objetos[selecionado].rot.y += speed * 3.14f;
        if (key == GLFW_KEY_Z && action != GLFW_RELEASE)
            objetos[selecionado].rot.z += speed * 3.14f;
    }

    // Translação (I, K, J, L, U, O) - Modo T
    if (modoAtual == TRANSLATE) {
        if (key == GLFW_KEY_I && action != GLFW_RELEASE)
            objetos[selecionado].pos.z -= speed;
        if (key == GLFW_KEY_K && action != GLFW_RELEASE)
            objetos[selecionado].pos.z += speed;
        if (key == GLFW_KEY_J && action != GLFW_RELEASE)
            objetos[selecionado].pos.x -= speed;
        if (key == GLFW_KEY_L && action != GLFW_RELEASE)
            objetos[selecionado].pos.x += speed;
        if (key == GLFW_KEY_U && action != GLFW_RELEASE)
            objetos[selecionado].pos.y += speed;
        if (key == GLFW_KEY_O && action != GLFW_RELEASE)
            objetos[selecionado].pos.y -= speed;
    }

    // Escala (UP, DOWN) - Modo S
    if (modoAtual == SCALE) {
        if (key == GLFW_KEY_UP && action != GLFW_RELEASE) {
            // speed é a quantidade de aumento por segundo, multiplicamos por deltaTime do programa para ter um aumento suave
            objetos[selecionado].scale.x += speed;
            objetos[selecionado].scale.y += speed;
            objetos[selecionado].scale.z += speed;
        }
        if (key == GLFW_KEY_DOWN && action != GLFW_RELEASE) {
            objetos[selecionado].scale.x -= speed;
            objetos[selecionado].scale.y -= speed;
            objetos[selecionado].scale.z -= speed;
            // Evitar escala negativa
            if (objetos[selecionado].scale.x < 0.1f)
                objetos[selecionado].scale = glm::vec3(0.1f);
        }
    }
}

// ---------- SHADER ----------
GLuint createShader()
{
    // Criar e compilar vertex shader usando a string vertexShaderSource
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vertexShaderSource, NULL);
    glCompileShader(vs);
    
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fragmentShaderSource, NULL);
    glCompileShader(fs);
    
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    return prog;
}

// ---------- MAIN ----------
int main()
{
    glfwInit();
    GLFWwindow* window = glfwCreateWindow(800,600,"Phong Viewer",NULL,NULL);
    glfwMakeContextCurrent(window);

    glfwSetKeyCallback(window, key_callback);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

    glEnable(GL_DEPTH_TEST);

    GLuint shader = createShader();
    glUseProgram(shader);

    GLuint textureID = loadTexture("image.png");
    glUniform1i(glGetUniformLocation(shader, "texture1"), 0);

    // Imprimir controles
    cout << "\n=== CONTROLES ===" << endl;
    cout << "WASD: Mover câmera" << endl;
    cout << "TAB: Mudar objeto selecionado" << endl;
    cout << "R: Modo Rotação (X/Y/Z para rotacionar)" << endl;
    cout << "T: Modo Translação (I/K/J/L/U/O para transladar)" << endl;
    cout << "S: Modo Escala (UP/DOWN para aumentar/diminuir)" << endl;
    cout << "P: Alternar perspectiva Ortográfica/Câmera" << endl;
    cout << "M: Alternar Wireframe" << endl;
    cout << "ESC: Sair" << endl;
    cout << "================\n" << endl;

    MeshData m1 = loadOBJ("obj1.obj");
    MeshData m2 = loadOBJ("obj2.obj");
    MeshData m3 = loadOBJ("obj3.obj");

    objetos[0].VAO = createVAO(m1);
    objetos[0].vertexCount = m1.vertexCount;

    objetos[1].VAO = createVAO(m2);
    objetos[1].vertexCount = m2.vertexCount;

    objetos[2].VAO = createVAO(m3);
    objetos[2].vertexCount = m3.vertexCount;

    for (int i=0;i<3;i++) {
        objetos[i].pos = glm::vec3(i*3.0f-3.0f,0,0);
        objetos[i].rot = glm::vec3(0);
        objetos[i].scale = glm::vec3(1);

        objetos[i].ka = glm::vec3(0.2f);
        objetos[i].kd = glm::vec3(0.7f);
        objetos[i].ks = glm::vec3(1.0f);
        objetos[i].shininess = 32.0f;
    }

    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        glfwPollEvents();

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureID);

        // mover luz
        float sp = 2*deltaTime;
        if (glfwGetKey(window, GLFW_KEY_1)) lightPos.x += sp;
        if (glfwGetKey(window, GLFW_KEY_2)) lightPos.x -= sp;

        glm::mat4 view = camera.getViewMatrix();
        
        // Matriz de projeção: Perspectiva ou Ortográfica
        glm::mat4 projection;
        if (perspective) {
            projection = glm::perspective(glm::radians(45.0f), 800.f/600.f, 0.1f, 100.f);
        } else {
            float scale = 10.0f;
            projection = glm::ortho(-scale, scale, -scale*0.75f, scale*0.75f, 0.1f, 100.f);
        }

        glUniformMatrix4fv(glGetUniformLocation(shader,"view"),1,GL_FALSE,glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shader,"projection"),1,GL_FALSE,glm::value_ptr(projection));

        glUniform3fv(glGetUniformLocation(shader,"lightPos"),1,glm::value_ptr(lightPos));
        glUniform3fv(glGetUniformLocation(shader,"viewPos"),1,glm::value_ptr(camera.position));
        glUniform3f(glGetUniformLocation(shader,"lightColor"),1,1,1);

        for (int i=0;i<3;i++)
        {
            glm::mat4 model(1);
            model = glm::translate(model, objetos[i].pos);
            model = glm::rotate(model, objetos[i].rot.x, glm::vec3(1,0,0));
            model = glm::rotate(model, objetos[i].rot.y, glm::vec3(0,1,0));
            model = glm::rotate(model, objetos[i].rot.z, glm::vec3(0,0,1));
            model = glm::scale(model, objetos[i].scale);

            glUniformMatrix4fv(glGetUniformLocation(shader,"model"),1,GL_FALSE,glm::value_ptr(model));
            // Enviar material para o shader
            glUniform3fv(glGetUniformLocation(shader,"ka"),1,glm::value_ptr(objetos[i].ka));
            glUniform3fv(glGetUniformLocation(shader,"kd"),1,glm::value_ptr(objetos[i].kd));
            glUniform3fv(glGetUniformLocation(shader,"ks"),1,glm::value_ptr(objetos[i].ks));
            glUniform1f(glGetUniformLocation(shader,"shininess"),objetos[i].shininess);
            glUniform1i(glGetUniformLocation(shader,"isWireframe"), GL_FALSE);

            glBindVertexArray(objetos[i].VAO);

            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glDrawArrays(GL_TRIANGLES, 0, objetos[i].vertexCount);

            if (wireframe) {
                glEnable(GL_POLYGON_OFFSET_LINE);
                glPolygonOffset(-1.0f, -1.0f);
                glLineWidth(2.0f);
                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                glUniform1i(glGetUniformLocation(shader,"isWireframe"), GL_TRUE);
                glUniform3f(glGetUniformLocation(shader,"wireframeColor"), 0.0f, 0.0f, 0.0f);
                glDrawArrays(GL_TRIANGLES, 0, objetos[i].vertexCount);
                glDisable(GL_POLYGON_OFFSET_LINE);
            }
        }
        glfwSwapBuffers(window);
    }

    glfwTerminate();
}