// file: main.cpp
#include "animated_model.hpp"
#include "model_loader.hpp"
#include "shader.hpp"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>

#include <memory>

void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

std::unique_ptr<AnimatedModel> AnimModel;

struct Settings
{
    std::string WindowTitle = "OpenGL Skeletal Animation";
    int WindowWidth = 800;
    int WindowHeight = 600;
    float FOV = 75.0f;
    glm::vec3 CameraPos = glm::vec3(-1.2f, 1.5f, 1.2f);
    glm::vec3 LightDir = glm::normalize(glm::vec3(0.5f, 1.0f, 1.0f));
    glm::vec3 LightColor = glm::vec3(1.0f, 1.0f, 0.8f);
    glm::vec3 AmbientColor = glm::vec3(1.0f, 1.0f, 1.0f);
    float AmbientIntensity = 0.5f;
    float SpecularShininess = 32.0f;
    float SpecularIntensity = 0.5;
    bool PlayAnimation = true;
    unsigned int CurrentAnimation = 1;
} Settings;

int main()
{
    // glfw: initialize and configure
    // ------------------------------
    glfwInit();

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GL_FALSE);
#endif

    // glfw window creation
    // --------------------
    GLFWwindow* window = glfwCreateWindow(Settings.WindowWidth, Settings.WindowHeight, Settings.WindowTitle.c_str(), nullptr, nullptr);

    if (window == nullptr)
    {
        std::cerr << "ERROR::GLFW: Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    // disable vsync
    glfwSwapInterval(0);

    glfwSetKeyCallback(window, KeyCallback);

    // tell GLFW to capture our mouse
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // glad: load all OpenGL function pointers
    // ---------------------------------------
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cerr << "ERROR::GLAD: Failed to initialize GLAD" << std::endl;
        return -1;
    }

    AnimModel = std::make_unique<AnimatedModel>();
    ModelLoader& loader = ModelLoader::GetInstance();
    loader.LoadFromFile("assets/vanguard.glb", *AnimModel);
    AnimModel->SetCurrentAnimation(Settings.CurrentAnimation);

    float aspectRatio = static_cast<GLfloat>(Settings.WindowWidth) / static_cast<GLfloat>(Settings.WindowHeight);
    glm::mat4 projectionMatrix = glm::perspective(glm::radians(Settings.FOV), aspectRatio, 0.1f, 100.0f);

    glm::mat4 viewMatrix = glm::lookAt(Settings.CameraPos, glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    glm::mat4 translationMatrix, rotationMatrix, scaleMatrix;
    translationMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.0f));
    rotationMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(0.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    scaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(0.01f));
    glm::mat4 modelMatrix = translationMatrix * rotationMatrix * scaleMatrix;

    Shader defaultShader("shaders/default.vs", "shaders/default.fs");
    defaultShader.Use();
    defaultShader.SetMat4("projection", projectionMatrix);
    defaultShader.SetMat4("view", viewMatrix);
    defaultShader.SetMat4("model", modelMatrix);
    defaultShader.SetVec3("cameraPos", Settings.CameraPos);
    defaultShader.SetVec3("lightDir", Settings.LightDir);
    defaultShader.SetVec3("lightColor", Settings.LightColor);
    defaultShader.SetVec3("ambientColor", Settings.AmbientColor);
    defaultShader.SetFloat("ambientIntensity", Settings.AmbientIntensity);
    defaultShader.SetFloat("specularShininess", Settings.SpecularShininess);
    defaultShader.SetFloat("specularIntensity", Settings.SpecularIntensity);

    // setup OpenGL
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    // game loop
    // -----------
    float currentTime = 0.0f;
    float lastTime    = 0.0f;
    float deltaTime   = 0.0f;

    while (!glfwWindowShouldClose(window))
    {
        currentTime = glfwGetTime();
        deltaTime = currentTime - lastTime;
        lastTime = currentTime;

        // update
        // ------
        if (Settings.PlayAnimation)
            AnimModel->UpdateAnimation(deltaTime);

        // render
        // ------
        glClearColor(0.2f, 0.3f, 0.4f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        AnimModel->SetBoneTransformations(defaultShader);
        AnimModel->Draw(defaultShader);

        // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        // -------------------------------------------------------------------------------
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // optional: de-allocate all resources once they've outlived their purpose:
    // ------------------------------------------------------------------------
    AnimModel.reset();

    // glfw: terminate, clearing all previously allocated GLFW resources.
    // ------------------------------------------------------------------
    glfwTerminate();
    return 0;
}

// glfw: whenever a keyboard key is pressed, this callback is called
// -----------------------------------------------------------------
void KeyCallback(GLFWwindow* window, int key, int /* scancode */, int action, int /* mods */)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
    else if (key == GLFW_KEY_SPACE && action == GLFW_PRESS)
    {
        Settings.CurrentAnimation = (Settings.CurrentAnimation + 1) % AnimModel->GetNumAnimations();
        AnimModel->SetCurrentAnimation(Settings.CurrentAnimation);
    }
    else if (key == GLFW_KEY_P && action == GLFW_PRESS)
        Settings.PlayAnimation = !Settings.PlayAnimation;
}
