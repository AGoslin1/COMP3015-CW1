#include "scenebasic_uniform.h"

#include <cstdio>
#include <cstdlib>

#include <string>
using std::string;

#include <sstream>
#include <iostream>
using std::cerr;
using std::endl;

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "helper/glutils.h"
#include "helper/texture.h"

#include <GLFW/glfw3.h>

using glm::vec3;
using glm::vec4;
using glm::mat4;
using glm::mat3;

static GLuint gDiffuseTex = 0;
static GLuint gGroundTex = 0; // NEW: ground texture handle


// Simple free-fly camera state for WASD + mouse look
static glm::vec3 cameraPos = glm::vec3(0.0f, 2.0f, 4.0f);
static glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
static glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
static float yaw = -90.0f; // facing -Z
static float pitch = 0.0f;
static double lastX = 0.0, lastY = 0.0;
static bool firstMouse = true;

// Skybox geometry (36 vertices)
static const float skyboxVertices[] = {
    -1.0f,  1.0f, -1.0f,
    -1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,

    -1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,

     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,

    -1.0f, -1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,

    -1.0f,  1.0f, -1.0f,
     1.0f,  1.0f, -1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f, -1.0f,

    -1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f,  1.0f
};

static void initSkyboxVAO(GLuint& vao, GLuint& vbo) {
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glBindVertexArray(0);
}

SceneBasic_Uniform::SceneBasic_Uniform() :
    tPrev(0),
    plane(50.0f, 50.0f, 1, 1),
    teapot(14, glm::mat4(1.0f))
{
    mesh = ObjMesh::load("media/hecarim1.obj", true);
}

void SceneBasic_Uniform::initScene()
{
    compile();
    glEnable(GL_DEPTH_TEST);

    // initialize camera/view from camera state
    view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
    projection = mat4(1.0f);
    angle = 0.0f;

    prog.setUniform("Light.L", vec3(10.0f));
    prog.setUniform("Light.La", vec3(0.5f));
    prog.setUniform("Fog.MaxDist", 100.0f);
    prog.setUniform("Fog.MinDist", 5.0f);
    prog.setUniform("Fog.Color", vec3(0.5f, 0.5f, 0.5f));

    GLFWwindow* win = glfwGetCurrentContext();
    if (win) {
        glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        double x, y;
        glfwGetCursorPos(win, &x, &y);
        lastX = x; lastY = y;
        firstMouse = true;
    }

    // Load media/lambert4.png via helper
    gDiffuseTex = Texture::loadTexture("media/lambert4.png");
    prog.setUniform("UseTexture", gDiffuseTex != 0);
    if (gDiffuseTex) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, gDiffuseTex);
        prog.setUniform("DiffuseTex", 0);
    }

    // Load ground texture (NEW)
    gGroundTex = Texture::loadTexture("media/ground.jpg");
    // --- Skybox setup ---
    // IMPORTANT: place your six images under media/skybox and name:
    // sky_posx.jpg sky_negx.jpg sky_posy.jpg sky_negy.jpg sky_posz.jpg sky_negz.jpg
    skyboxTex = Texture::loadCubeMap("media/skybox/sky", ".jpg");
    initSkyboxVAO(skyboxVAO, skyboxVBO);
}

void SceneBasic_Uniform::compile()
{
    try {
        prog.compileShader("shader/basic_uniform.vert");
        prog.compileShader("shader/basic_uniform.frag");
        prog.link();
        prog.use();
    }
    catch (GLSLProgramException& e) {
        cerr << e.what() << endl;
        exit(EXIT_FAILURE);
    }
}

void SceneBasic_Uniform::update(float t)
{
    float deltaT = t - tPrev;
    if (tPrev == 0.0f) deltaT = 0.0f;
    tPrev = t;
    angle += 0.1f * deltaT;
    if (angle > glm::two_pi<float>())angle -= glm::two_pi<float>();

    GLFWwindow* win = glfwGetCurrentContext();
    if (win) {
        // Movement (WASD + Q/E up/down)
        float cameraSpeed = 3.5f * (deltaT > 0.0f ? deltaT : 0.016f); // units per second
        if (glfwGetKey(win, GLFW_KEY_W) == GLFW_PRESS)
            cameraPos += cameraSpeed * cameraFront;
        if (glfwGetKey(win, GLFW_KEY_S) == GLFW_PRESS)
            cameraPos -= cameraSpeed * cameraFront;
        if (glfwGetKey(win, GLFW_KEY_A) == GLFW_PRESS)
            cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
        if (glfwGetKey(win, GLFW_KEY_D) == GLFW_PRESS)
            cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
        if (glfwGetKey(win, GLFW_KEY_Q) == GLFW_PRESS)
            cameraPos -= cameraUp * cameraSpeed;
        if (glfwGetKey(win, GLFW_KEY_E) == GLFW_PRESS)
            cameraPos += cameraUp * cameraSpeed;

        // Mouse look
        double xpos, ypos;
        glfwGetCursorPos(win, &xpos, &ypos);
        if (firstMouse) {
            lastX = xpos;
            lastY = ypos;
            firstMouse = false;
        }

        float xoffset = float(xpos - lastX);
        float yoffset = float(lastY - ypos); // reversed: y ranges top->down

        lastX = xpos;
        lastY = ypos;

        const float sensitivity = 0.12f;
        xoffset *= sensitivity;
        yoffset *= sensitivity;

        yaw += xoffset;
        pitch += yoffset;
        if (pitch > 89.0f) pitch = 89.0f;
        if (pitch < -89.0f) pitch = -89.0f;

        glm::vec3 front;
        front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        front.y = sin(glm::radians(pitch));
        front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        cameraFront = glm::normalize(front);

        // Rebuild view matrix from updated camera
        view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
    }
}

void SceneBasic_Uniform::render()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // draw skybox first using the same program
    glDepthFunc(GL_LEQUAL);
    prog.use();
    // remove translation component from view so skybox appears stationary
    mat4 viewNoTrans = glm::mat4(glm::mat3(view));
    prog.setUniform("ViewNoTrans", viewNoTrans);
    prog.setUniform("Projection", projection);
    prog.setUniform("RenderSkybox", true);
    glActiveTexture(GL_TEXTURE0 + 5);
    glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxTex);
    prog.setUniform("Skybox", 5);
    glBindVertexArray(skyboxVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
    prog.setUniform("RenderSkybox", false);
    glDepthFunc(GL_LESS);

    vec3 lightWorld = vec3(8.0f * cos(angle), 6.0f, 8.0f * sin(angle));
    vec4 lightPosView = vec4(view * vec4(lightWorld, 1.0f));
    prog.setUniform("Light.Position", lightPosView);

    // compute spotlight direction (world-space): point from light to scene origin (or any target)
    vec3 spotDirWorld = glm::normalize(vec3(0.0f, 0.0f, 0.0f) - lightWorld);

    // transform direction to view-space (w = 0 for direction)
    vec3 spotDirView = vec3(view * vec4(spotDirWorld, 0.0f));
    prog.setUniform("Light.SpotDirection", spotDirView);

    // set cutoff/exponent (cutoff as cosine of angle)
    prog.setUniform("Light.SpotCutoff", cos(glm::radians(18.0f)));   // inner cone angle ~18 degrees
    prog.setUniform("Light.SpotExponent", 20.0f);                     // sharpness

    // ... existing material uniforms and drawing code follows ...
    prog.setUniform("Material.Kd", vec3(0.8f));
    prog.setUniform("Material.Ks", vec3(0.3f));
    prog.setUniform("Material.Ka", vec3(0.1f));
    prog.setUniform("Material.Shininess", 128.0f);

    // Bind texture before drawing model (keeps using unit 0)
    if (gDiffuseTex) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, gDiffuseTex);
        prog.setUniform("UseTexture", true);
        prog.setUniform("DiffuseTex", 0);
    }
    else {
        prog.setUniform("UseTexture", false);
    }

    model = mat4(1.0f);
    model = glm::translate(model, vec3(0.0f, 1.5f, 0.0f));
    model = glm::scale(model, vec3(0.015f));
    setMatrices();
    if (mesh) mesh->render();

    // Render ground using its own texture (unit 1) if available
    prog.setUniform("Material.Kd", vec3(0.7f));
    prog.setUniform("Material.Ks", vec3(0.0f));
    prog.setUniform("Material.Ka", vec3(0.2f));
    prog.setUniform("Material.Shininess", 180.0f);

    if (gGroundTex) {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, gGroundTex);
        prog.setUniform("UseTexture", true);
        prog.setUniform("DiffuseTex", 1);
    }
    else {
        prog.setUniform("UseTexture", false);
    }

    model = mat4(1.0f);
    setMatrices();
    plane.render();
}

void SceneBasic_Uniform::resize(int w, int h)
{
    glViewport(0, 0, w, h);
    width = w;
    height = h;
    projection = glm::perspective(glm::radians(70.0f), (float)w / h, 0.3f, 100.0f);
}
void SceneBasic_Uniform::setMatrices()
{
    mat4 mv = view * model;
    prog.setUniform("ModelViewMatrix", mv);
    prog.setUniform("NormalMatrix", glm::mat3(vec3(mv[0]), vec3(mv[1]), vec3(mv[2])));
    prog.setUniform("MVP", projection * mv);
}