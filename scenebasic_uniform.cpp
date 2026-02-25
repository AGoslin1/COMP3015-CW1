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

#include <ctime>
#include <cmath>
#include <cstdlib>
#include <vector>

using glm::vec3;
using glm::vec4;
using glm::mat4;
using glm::mat3;

static GLuint gDiffuseTex = 0;

//fly free camera
static glm::vec3 cameraPos = glm::vec3(0.0f, 2.0f, 4.0f);
static glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
static glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
static float yaw = -90.0f; 
static float pitch = 0.0f;
static double lastX = 0.0, lastY = 0.0;
static bool firstMouse = true;

//skybox geometry
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

//per minion birth timers
static std::vector<float> gMinionBirthTimes;

// Spotlight helper
struct Spotlight {
    vec3 pos;
    vec3 target;
    vec3 dir;       
    float speed;      
    float changeTimer;
    float changeInterval;
    float range;      
    float cutoffCos;  
};

//number of lights active
static const int NUM_LIGHTS = 8;
static std::vector<Spotlight> gSpotlights;

static float randomRange(float a, float b) {
    float r = (float)std::rand() / (float)RAND_MAX;
    return a + r * (b - a);
}

SceneBasic_Uniform::SceneBasic_Uniform() :
    tPrev(0),
    plane(50.0f, 50.0f, 1, 1),
    teapot(14, glm::mat4(1.0f))
{
    mesh = ObjMesh::load("media/hecarim1.obj", true);
    minionMesh = ObjMesh::load("media/RedMinion.obj", true);
    //gameplay defaults
    collectedCount = 0;
    spawnTimer = 0.0f;
    nextSpawnInterval = 3.0f;
    maxMinions = 8;
    hecarimPos = glm::vec3(0.0f, 1.5f, 0.0f); 
    hecarimYaw = 0.0f;
    thirdPersonLocked = true; //3rd person 
    prevTabPressed = false;
    prevQPressed = false;

    // win flag
    gameWon = false;

    //spawn randomness
    std::srand((unsigned int)std::time(nullptr));
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

    //load textures
    gDiffuseTex = Texture::loadTexture("media/lambert4.png");
    prog.setUniform("UseTexture", gDiffuseTex != 0);
    if (gDiffuseTex) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, gDiffuseTex);
        prog.setUniform("DiffuseTex", 0);
    }

    groundTex = Texture::loadTexture("media/ground.jpg");
    minionTex = Texture::loadTexture("media/RedMinion.png");

    //skybox
    skyboxTex = Texture::loadCubeMap("media/skybox/sky", ".jpg");
    initSkyboxVAO(skyboxVAO, skyboxVBO);

    //initialize spotlights to NUM_LIGHTS
    gSpotlights.clear();
    gSpotlights.resize(NUM_LIGHTS);
    for (int i = 0; i < NUM_LIGHTS; ++i) {
        Spotlight& s = gSpotlights[i];
        float half = 25.0f;
        if (i == 0) {
            //center spotlight
            s.pos = vec3(0.0f, 14.0f, 0.0f);
            s.target = vec3(0.0f, 0.0f, 0.0f);
            s.dir = glm::normalize(vec3(s.target.x - s.pos.x, -s.pos.y, s.target.z - s.pos.z));
            s.speed = 0.0f;
            s.changeInterval = 99999.0f;
            s.changeTimer = s.changeInterval;
            s.range = 18.0f;
            s.cutoffCos = cos(glm::radians(35.0f));
        }
        else {
            //moving random spotlights
            s.pos = vec3(randomRange(-half, half), randomRange(5.0f, 9.0f), randomRange(-half, half));
            s.target = vec3(randomRange(-half, half), 0.0f, randomRange(-half, half));
            s.dir = glm::normalize(vec3(s.target.x - s.pos.x, -s.pos.y, s.target.z - s.pos.z));
            s.speed = randomRange(8.0f, 20.0f);
            s.changeInterval = randomRange(0.2f, 0.9f);
            s.changeTimer = s.changeInterval;
            s.range = 8.0f;
            s.cutoffCos = cos(glm::radians(20.0f));
        }
    }

    gMinionBirthTimes.clear();
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
    if (angle > glm::two_pi<float>()) angle -= glm::two_pi<float>();

    //if game is won then game is over
    if (gameWon) return;

    GLFWwindow* win = glfwGetCurrentContext();
    if (!win) return;

    //tab to toggle free or locked cam
    int tabState = glfwGetKey(win, GLFW_KEY_TAB);
    if (tabState == GLFW_PRESS && !prevTabPressed) {
        thirdPersonLocked = !thirdPersonLocked;
        if (!thirdPersonLocked) {
            glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            firstMouse = true;
        }
        else {
            firstMouse = true;
        }
    }
    prevTabPressed = (tabState == GLFW_PRESS);

    //mouse look
    {
        double xpos, ypos;
        glfwGetCursorPos(win, &xpos, &ypos);
        if (firstMouse) {
            lastX = xpos;
            lastY = ypos;
            firstMouse = false;
        }
        float xoffset = float(xpos - lastX);
        float yoffset = float(lastY - ypos); 
        lastX = xpos;
        lastY = ypos;

        const float sensitivity = thirdPersonLocked ? 0.10f : 0.12f;
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
    }

    //movement input
    if (thirdPersonLocked) {
        // movement relative to camera position
        float forwardInput = 0.0f;
        float rightInput = 0.0f;
        if (glfwGetKey(win, GLFW_KEY_W) == GLFW_PRESS) forwardInput += 1.0f;
        if (glfwGetKey(win, GLFW_KEY_S) == GLFW_PRESS) forwardInput -= 1.0f;
        if (glfwGetKey(win, GLFW_KEY_D) == GLFW_PRESS) rightInput += 1.0f;
        if (glfwGetKey(win, GLFW_KEY_A) == GLFW_PRESS) rightInput -= 1.0f;

        glm::vec3 camForwardXZ = glm::normalize(glm::vec3(cameraFront.x, 0.0f, cameraFront.z));
        glm::vec3 camRight = glm::normalize(glm::cross(camForwardXZ, cameraUp));

        glm::vec3 moveVec = glm::vec3(0.0f);
        moveVec += camForwardXZ * forwardInput;
        moveVec += camRight * rightInput;

        if (glm::length(moveVec) > 0.001f) {
            moveVec = glm::normalize(moveVec);
            const float speed =10.0f; //move speed
            hecarimPos += moveVec * speed * (deltaT > 0.0f ? deltaT : 0.016f);

            float targetYaw = glm::degrees(std::atan2(moveVec.x, moveVec.z)) + 180.0f;
            auto shortestAngleDiff = [](float fromDeg, float toDeg) -> float {
                float diff = std::fmodf((toDeg - fromDeg + 540.0f), 360.0f) - 180.0f;
                return diff;
                };
            float diff = shortestAngleDiff(hecarimYaw, targetYaw);
            const float rotationSpeed = 360.0f;
            float maxDelta = rotationSpeed * (deltaT > 0.0f ? deltaT : 0.016f);
            diff = glm::clamp(diff, -maxDelta, maxDelta);
            hecarimYaw += diff;
        }

        // camera orbit
        float followDist = 6.0f;
        float followHeight = 2.0f;
        glm::vec3 camDir = glm::normalize(cameraFront);
        cameraPos = hecarimPos - camDir * followDist + glm::vec3(0.0f, followHeight, 0.0f);
        cameraFront = glm::normalize((hecarimPos + glm::vec3(0.0f, 1.0f, 0.0f)) - cameraPos);
        view = glm::lookAt(cameraPos, hecarimPos + glm::vec3(0.0f, 1.0f, 0.0f), cameraUp);
    }
    else {
        // free cam movement
        float cameraSpeed = 3.5f * (deltaT > 0.0f ? deltaT : 0.016f);
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

        view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
    }

    //moving spotlight update
    for (int i = 1; i < NUM_LIGHTS; ++i) {
        Spotlight& s = gSpotlights[i];
        s.changeTimer -= deltaT;
       
        vec3 toTarget = vec3(s.target.x - s.pos.x, 0.0f, s.target.z - s.pos.z);
        float dist = glm::length(toTarget);
        if (dist > 0.01f) {
            vec3 step = glm::normalize(toTarget) * s.speed * (deltaT > 0.0f ? deltaT : 0.016f);
            if (glm::length(step) > dist) step *= (dist / glm::length(step));
            s.pos.x += step.x;
            s.pos.z += step.z;
        }
        s.pos.y = glm::clamp(s.pos.y + randomRange(-0.05f, 0.05f), 4.0f, 9.0f);

        if (s.changeTimer <= 0.0f || dist < 0.5f) {
            //new target around plane
            float half = 25.0f;
            s.target = vec3(randomRange(-half, half), 0.0f, randomRange(-half, half));
            s.speed = randomRange(8.0f, 20.0f);
            s.changeInterval = randomRange(0.2f, 1.0f);
            s.changeTimer = s.changeInterval;
        }

        s.dir = glm::normalize(vec3(s.target.x - s.pos.x, -s.pos.y, s.target.z - s.pos.z));
    }

    spawnTimer -= deltaT;
    if (spawnTimer <= 0.0f) {
        spawnTimer = 0.0f;
        if ((int)minions.size() < maxMinions) {
            nextSpawnInterval = randomRange(2.0f, 6.0f);
            spawnTimer = nextSpawnInterval;
            glm::vec3 spawn;
            for (int tries = 0; tries < 10; ++tries) {
                float half = 25.0f;
                float x = randomRange(-half, half);
                float z = randomRange(-half, half);
                spawn = glm::vec3(x, 1.0f, z);
                if (glm::length(spawn - hecarimPos) > 2.0f) break;
            }
            Minion m; m.pos = spawn; m.alive = true;
            minions.push_back(m);
            gMinionBirthTimes.push_back(t);
        }
        else {
            spawnTimer = randomRange(1.0f, 3.0f);
        }
    }

    int qState = glfwGetKey(win, GLFW_KEY_Q);
    if (qState == GLFW_PRESS && !prevQPressed) {
        float collectRadius = 1.5f;
        for (size_t i = 0; i < minions.size(); ++i) {
            auto& m = minions[i];
            if (!m.alive) continue;
            float d = glm::length(glm::vec3(m.pos.x, 0.0f, m.pos.z) - glm::vec3(hecarimPos.x, 0.0f, hecarimPos.z));
            if (d <= collectRadius) {
                m.alive = false;
                collectedCount++;
                std::cout << "Collected a minion! total=" << collectedCount << std::endl;

                if (!gameWon && collectedCount >= winTarget) {
                    gameWon = true;
                    std::cout << "You win! Collected " << collectedCount << " minions.\n";
                    glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                }

                break;
            }
        }
    }
    prevQPressed = (qState == GLFW_PRESS);

    //expire old minions
    const float lifespan = 10.0f;
    for (size_t i = 0; i < minions.size() && i < gMinionBirthTimes.size(); ++i) {
        if (!minions[i].alive) continue;
        if (t - gMinionBirthTimes[i] > lifespan) {
            minions[i].alive = false; //no score
        }
    }

    if (!minions.empty()) {
        std::vector<Minion> tmp;
        tmp.reserve(minions.size());
        std::vector<float> tmpTimes;
        tmpTimes.reserve(gMinionBirthTimes.size());
        for (size_t i = 0; i < minions.size(); ++i) {
            if (minions[i].alive) {
                tmp.push_back(minions[i]);
                tmpTimes.push_back(gMinionBirthTimes[i]);
            }
        }
        minions.swap(tmp);
        gMinionBirthTimes.swap(tmpTimes);
    }
    else {
        gMinionBirthTimes.clear();
    }
}

void SceneBasic_Uniform::render()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glDepthFunc(GL_LEQUAL);
    prog.use();
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

    //lights into shader
    for (int i = 0; i < NUM_LIGHTS; ++i) {
        const Spotlight& s = gSpotlights[i];
        vec4 posView = view * vec4(s.pos, 1.0f);
        vec3 dirView = vec3(view * vec4(s.dir, 0.0f));
        prog.setUniform((std::string("Lights[") + std::to_string(i) + "].Position").c_str(), posView);
        prog.setUniform((std::string("Lights[") + std::to_string(i) + "].L").c_str(), vec3(8.0f));
        prog.setUniform((std::string("Lights[") + std::to_string(i) + "].La").c_str(), vec3(0.05f));
        prog.setUniform((std::string("Lights[") + std::to_string(i) + "].SpotDirection").c_str(), dirView);
        prog.setUniform((std::string("Lights[") + std::to_string(i) + "].SpotCutoff").c_str(), s.cutoffCos);
        prog.setUniform((std::string("Lights[") + std::to_string(i) + "].SpotExponent").c_str(), 20.0f);
    }

    prog.setUniform("Material.Kd", vec3(0.8f));
    prog.setUniform("Material.Ks", vec3(0.3f));
    prog.setUniform("Material.Ka", vec3(0.1f));
    prog.setUniform("Material.Shininess", 128.0f);

    if (gDiffuseTex) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, gDiffuseTex);
        prog.setUniform("UseTexture", true);
        prog.setUniform("DiffuseTex", 0);
    }
    else {
        prog.setUniform("UseTexture", false);
    }

    //render hecarim
    model = mat4(1.0f);
    model = glm::translate(model, hecarimPos);
    model = glm::rotate(model, glm::radians(hecarimYaw + 180.0f), vec3(0, 1, 0));
    model = glm::scale(model, vec3(0.015f));
    setMatrices();
    if (mesh) mesh->render();

    // render minions
    if (minionMesh && minionTex) {
        for (auto& m : minions) {
            if (!m.alive) continue;
            prog.setUniform("Material.Kd", vec3(0.8f));
            prog.setUniform("Material.Ks", vec3(0.1f));
            prog.setUniform("Material.Ka", vec3(0.1f));
            prog.setUniform("Material.Shininess", 32.0f);

            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, minionTex);
            prog.setUniform("UseTexture", true);
            prog.setUniform("DiffuseTex", 2);

            model = mat4(1.0f);
            model = glm::translate(model, m.pos + vec3(0.0f, 0.2f, 0.0f));
            model = glm::scale(model, vec3(0.02f));
            setMatrices();
            minionMesh->render();
        }
        glActiveTexture(GL_TEXTURE0);
        prog.setUniform("DiffuseTex", 0);
    }

    // Render ground
    prog.setUniform("Material.Kd", vec3(0.7f));
    prog.setUniform("Material.Ks", vec3(0.0f));
    prog.setUniform("Material.Ka", vec3(0.2f));
    prog.setUniform("Material.Shininess", 180.0f);

    if (groundTex) {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, groundTex);
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