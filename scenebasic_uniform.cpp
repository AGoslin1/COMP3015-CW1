#include "scenebasic_uniform.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <sstream>
#include <iostream>
#include <ctime>
#include <cmath>
#include <vector>
#include <memory>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>

#include "helper/glutils.h"
#include "helper/texture.h"
#include "helper/particleutils.h"
#include "helper/random.h"
#include "helper/frustum.h"

#include <GLFW/glfw3.h>

using std::string;
using std::cerr;
using std::endl;

using glm::vec3;
using glm::vec4;
using glm::mat4;
using glm::mat3;

static GLuint gDiffuseTex = 0;
static Random gRng;
static GLuint gRandomTex1D = 0;

// camera
static glm::vec3 cameraPos = glm::vec3(0.0f, 2.0f, 4.0f);
static glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
static glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
static float yaw = -90.0f;
static float pitch = 0.0f;
static double lastX = 0.0, lastY = 0.0;
static bool firstMouse = true;

// shadows
static std::vector<GLuint> shadowFBOs;
static std::vector<GLuint> shadowTexs;
static std::vector<glm::mat4> lightSpaces;

static std::unique_ptr<Frustum> lightFrustum;
static const int SHADOW_SIZE = 2048;
static GLSLProgram depthProg;

// debug
static GLSLProgram debugProg;
static GLuint debugVAO = 0;
static GLuint debugVBO = 0;
static bool debugShowShadow = true;

// sky particle ring center
static glm::vec3 particleRingCenter = glm::vec3(0.0f, 12.0f, 0.0f);

static GLSLProgram skyboxProg;


// skybox geometry
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

static std::vector<float> gMinionBirthTimes;

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

static const int NUM_LIGHTS = 8;
static std::vector<Spotlight> gSpotlights;

static float randomRange(float a, float b) {
    float r = (float)std::rand() / (float)RAND_MAX;
    return a + r * (b - a);
}

// particles
static GLuint particleVAO = 0;
static GLuint particleVBO = 0;
static GLuint particleInstanceVBO = 0;
static GLuint fireTex = 0;
static GLSLProgram particleProg;
static const int MAX_PARTICLES = 12000;

static const float PARTICLE_BASE_SIZE = 0.05f;
static const float PARTICLE_SIZE_VARIANCE = 0.04f;
static const float PARTICLE_MIN_SIZE = 0.005f;

static bool crownEnabled = true;
static bool gameLost = false;

struct Whisp {
    glm::vec3 pos;
    glm::vec3 vel;
    bool alive;
    float speed;
    float radius;
};
static Whisp theWhisp;

struct Particle {
    vec3 pos;
    vec3 vel;
    float life;
    float size;
    float type;
    bool alive;
};

static std::vector<Particle> particles;
static float particleSpawnAccumulator = 0.0f;



static void initParticleSystem()
{
    float quadVerts[] = {
        -0.5f, -0.5f,
         0.5f, -0.5f,
         0.5f,  0.5f,
        -0.5f, -0.5f,
         0.5f,  0.5f,
        -0.5f,  0.5f
    };

    glGenVertexArrays(1, &particleVAO);
    glGenBuffers(1, &particleVBO);
    glGenBuffers(1, &particleInstanceVBO);

    glBindVertexArray(particleVAO);

    glBindBuffer(GL_ARRAY_BUFFER, particleVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

    glBindBuffer(GL_ARRAY_BUFFER, particleInstanceVBO);
    glBufferData(GL_ARRAY_BUFFER, MAX_PARTICLES * 8 * sizeof(float), nullptr, GL_STREAM_DRAW);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glVertexAttribDivisor(1, 1);

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(4 * sizeof(float)));
    glVertexAttribDivisor(2, 1);

    glBindVertexArray(0);

    particles.clear();
    particles.resize(MAX_PARTICLES);
    for (int i = 0; i < MAX_PARTICLES; ++i) {
        particles[i].alive = false;
        particles[i].life = 0.0f;
        particles[i].type = 0.0f;
    }

    fireTex = Texture::loadTexture("media/texture/fire.png");

    if (gRandomTex1D == 0) {
        gRandomTex1D = ParticleUtils::createRandomTex1D(1024);
    }
}

static void spawnParticleRing(const vec3& center, float radius)
{
    for (int i = 0; i < (int)particles.size(); ++i) {
        if (!particles[i].alive) {
            glm::vec3 c = gRng.uniformCircle();
            float jitter = gRng.nextFloat() * 0.06f - 0.03f;
            float r = radius + jitter;
            float x = center.x + r * c.x;
            float z = center.z + r * c.y;
            float y = center.y + gRng.nextFloat() * 0.12f - 0.06f;

            particles[i].pos = vec3(x, y, z);

            vec3 outward = glm::normalize(vec3(
                c.x + gRng.nextFloat() * 0.04f - 0.02f,
                0.0f,
                c.y + gRng.nextFloat() * 0.04f - 0.02f
            ));

            particles[i].vel = outward * (0.2f + gRng.nextFloat() * 0.6f)
                + vec3(0.0f, 0.45f + gRng.nextFloat() * 0.8f, 0.0f);

            particles[i].life = 1.0f;
            particles[i].size = PARTICLE_BASE_SIZE + gRng.nextFloat() * PARTICLE_SIZE_VARIANCE;
            particles[i].type = 0.0f;
            particles[i].alive = true;
            break;
        }
    }
}

static void spawnParticleExplosion(const vec3& center, int count)
{
    int spawned = 0;

    for (int i = 0; i < (int)particles.size() && spawned < count; ++i) {
        if (!particles[i].alive) {
            glm::vec3 dir = glm::normalize(glm::vec3(
                gRng.nextFloat() * 2.0f - 1.0f,
                gRng.nextFloat() * 2.0f - 1.0f,
                gRng.nextFloat() * 2.0f - 1.0f
            ));
            float speed = 2.0f + gRng.nextFloat() * 4.0f;
            particles[i].pos = center + dir * (0.02f + gRng.nextFloat() * 0.08f);
            particles[i].vel = dir * speed + vec3(0.0f, 0.6f + gRng.nextFloat() * 1.6f, 0.0f);
            particles[i].life = 0.6f + gRng.nextFloat() * 0.9f;
            particles[i].size = 0.03f + gRng.nextFloat() * 0.08f;
            particles[i].type = 1.0f;
            particles[i].alive = true;
            spawned++;
        }
    }

    for (int i = 0; i < (int)particles.size() && spawned < count; ++i) {
        if (!particles[i].alive) continue;
        if (particles[i].type != 1.0f || particles[i].life < 0.3f) {
            glm::vec3 dir = glm::normalize(glm::vec3(
                gRng.nextFloat() * 2.0f - 1.0f,
                gRng.nextFloat() * 2.0f - 1.0f,
                gRng.nextFloat() * 2.0f - 1.0f
            ));
            float speed = 2.0f + gRng.nextFloat() * 4.0f;
            particles[i].pos = center + dir * (0.02f + gRng.nextFloat() * 0.08f);
            particles[i].vel = dir * speed + vec3(0.0f, 0.6f + gRng.nextFloat() * 1.6f, 0.0f);
            particles[i].life = 0.6f + gRng.nextFloat() * 0.9f;
            particles[i].size = 0.03f + gRng.nextFloat() * 0.08f;
            particles[i].type = 1.0f;
            particles[i].alive = true;
            spawned++;
        }
    }

    for (int i = 0; i < (int)particles.size() && spawned < count; ++i) {
        glm::vec3 dir = glm::normalize(glm::vec3(
            gRng.nextFloat() * 2.0f - 1.0f,
            gRng.nextFloat() * 2.0f - 1.0f,
            gRng.nextFloat() * 2.0f - 1.0f
        ));
        float speed = 2.0f + gRng.nextFloat() * 4.0f;
        particles[i].pos = center + dir * (0.02f + gRng.nextFloat() * 0.08f);
        particles[i].vel = dir * speed + vec3(0.0f, 0.6f + gRng.nextFloat() * 1.6f, 0.0f);
        particles[i].life = 0.6f + gRng.nextFloat() * 0.9f;
        particles[i].size = 0.03f + gRng.nextFloat() * 0.08f;
        particles[i].type = 1.0f;
        particles[i].alive = true;
        spawned++;
    }
}

static void updateWhisp(float dt, const glm::vec3& headPos)
{
    if (!theWhisp.alive) return;

    glm::vec3 target = headPos;
    glm::vec3 toTarget = target - theWhisp.pos;
    float dist = glm::length(toTarget);

    if (dist > 1e-6f) {
        glm::vec3 desired = glm::normalize(toTarget);
        float accel = 6.0f;
        theWhisp.vel = glm::mix(theWhisp.vel, desired * theWhisp.speed, glm::clamp(accel * dt, 0.0f, 1.0f));
    }
    theWhisp.pos += theWhisp.vel * dt;

    const int trailSpawnCount = 20;
    for (int s = 0; s < trailSpawnCount; ++s) {
        for (int i = 0; i < (int)particles.size(); ++i) {
            if (!particles[i].alive) {
                float spread = 0.50f + gRng.nextFloat() * 0.9f;
                glm::vec3 offset = glm::vec3(
                    (gRng.nextFloat() * 2.0f - 1.0f) * spread,
                    (gRng.nextFloat() * 2.0f - 1.0f) * (spread * 0.6f),
                    (gRng.nextFloat() * 2.0f - 1.0f) * spread
                );
                particles[i].pos = theWhisp.pos + offset;
                particles[i].vel = glm::vec3(
                    gRng.nextFloat() * 0.12f - 0.06f,
                    gRng.nextFloat() * 0.08f,
                    gRng.nextFloat() * 0.12f - 0.06f
                );
                particles[i].life = 0.8f + gRng.nextFloat() * 0.9f;
                particles[i].size = 0.15f + gRng.nextFloat() * 0.06f;
                particles[i].type = 2.0f;
                particles[i].alive = true;
                break;
            }
        }
    }

    if (dist <= theWhisp.radius + 0.4f) {
        spawnParticleExplosion(theWhisp.pos, 150);
        crownEnabled = false;
        gameLost = true;
        theWhisp.alive = false;
        GLFWwindow* win = glfwGetCurrentContext();
        if (win) glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        std::cout << "Whisp touched Hecarim: crown disabled - you lose\n";
    }
}

static void updateParticles(float dt, const vec3& ringCenter)
{
    const float spawnRatePerSecond = 300.0f;
    const float ringRadius = 0.35f;

    if (crownEnabled) {
        particleSpawnAccumulator += spawnRatePerSecond * dt;
        while (particleSpawnAccumulator >= 1.0f) {
            spawnParticleRing(ringCenter, ringRadius);
            particleSpawnAccumulator -= 1.0f;
        }
    }

    for (auto& p : particles) {
        if (!p.alive) continue;

        p.pos += p.vel * dt;

        if (p.type == 2.0f) {
            p.vel += vec3(0.0f, -0.15f, 0.0f) * dt;
        }
        else {
            p.vel += vec3(0.0f, -0.4f, 0.0f) * dt;
        }

        float lifeDecay = 0.45f;
        if (p.type == 1.0f) lifeDecay = 0.9f;
        if (p.type == 2.0f) lifeDecay = 0.35f;

        p.life -= lifeDecay * dt;
        if (p.life <= 0.0f) {
            p.alive = false;
            p.life = 0.0f;
        }

        p.size *= (1.0f - 0.10f * dt);
        if (p.size < PARTICLE_MIN_SIZE) p.size = PARTICLE_MIN_SIZE;
    }
}

static void renderParticles(const mat4& view, const mat4& projection)
{
    if (!particleProg.isLinked()) return;
    if (fireTex == 0) return;

    std::vector<float> instanceData;
    instanceData.reserve(particles.size() * 8);

    int aliveCount = 0;
    for (const auto& p : particles) {
        if (!p.alive) continue;
        instanceData.push_back(p.pos.x);
        instanceData.push_back(p.pos.y);
        instanceData.push_back(p.pos.z);
        instanceData.push_back(p.size);
        instanceData.push_back(p.life);
        instanceData.push_back(p.type);
        instanceData.push_back(0.0f);
        instanceData.push_back(0.0f);
        ++aliveCount;
    }
    if (aliveCount == 0) return;

    glBindBuffer(GL_ARRAY_BUFFER, particleInstanceVBO);
    glBufferData(GL_ARRAY_BUFFER, instanceData.size() * sizeof(float), instanceData.data(), GL_STREAM_DRAW);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(GL_FALSE);

    particleProg.use();
    particleProg.setUniform("Projection", projection);
    particleProg.setUniform("View", view);

    glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
    glm::vec3 camFront = glm::normalize(cameraFront);
    glm::vec3 camRight = glm::cross(camFront, worldUp);

    if (glm::length(camRight) < 1e-4f) {
        glm::vec3 altUp(0.0f, 0.0f, 1.0f);
        camRight = glm::cross(camFront, altUp);
        if (glm::length(camRight) < 1e-4f) {
            camRight = glm::vec3(1.0f, 0.0f, 0.0f);
        }
    }

    camRight = glm::normalize(camRight);
    glm::vec3 camUp = glm::normalize(glm::cross(camRight, camFront));

    particleProg.setUniform("CameraRight", camRight);
    particleProg.setUniform("CameraUp", camUp);

    particleProg.setUniform("FireTex", 6);
    glActiveTexture(GL_TEXTURE0 + 6);
    glBindTexture(GL_TEXTURE_2D, fireTex);

    if (gRandomTex1D != 0) {
        particleProg.setUniform("RandomTex", 7);
        glActiveTexture(GL_TEXTURE0 + 7);
        glBindTexture(GL_TEXTURE_1D, gRandomTex1D);
    }

    glBindVertexArray(particleVAO);
    glDrawArraysInstanced(GL_TRIANGLES, 0, 6, aliveCount);
    glBindVertexArray(0);

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glActiveTexture(GL_TEXTURE0);
}

SceneBasic_Uniform::SceneBasic_Uniform() :
    tPrev(0),
    plane(50.0f, 50.0f, 1, 1),
    teapot(14, glm::mat4(1.0f))
{
    mesh = ObjMesh::load("media/hecarim1.obj", true, true);
    minionMesh = ObjMesh::load("media/RedMinion.obj", true);

    collectedCount = 0;
    spawnTimer = 0.0f;
    nextSpawnInterval = 3.0f;
    maxMinions = 8;
    hecarimPos = glm::vec3(0.0f, 1.5f, 0.0f);
    hecarimYaw = 0.0f;
    thirdPersonLocked = true;
    prevTabPressed = false;
    prevQPressed = false;
    gameWon = false;

    std::srand((unsigned int)std::time(nullptr));
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

void SceneBasic_Uniform::initScene()
{
    compile();
    glEnable(GL_DEPTH_TEST);

    view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
    projection = mat4(1.0f);
    angle = 0.0f;

    prog.setUniform("Light.L", vec3(10.0f));
    prog.setUniform("Light.La", vec3(0.5f));
    prog.setUniform("Fog.MaxDist", 100.0f);
    prog.setUniform("Fog.MinDist", 5.0f);
    prog.setUniform("Fog.Color", vec3(0.5f, 0.5f, 0.5f));

    prog.setUniform("UseFog", false);
    prog.setUniform("UseShadows", true);
    prog.setUniform("UseNormalMap", false);

    GLFWwindow* win = glfwGetCurrentContext();
    if (win) {
        glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        double x, y;
        glfwGetCursorPos(win, &x, &y);
        lastX = x;
        lastY = y;
        firstMouse = true;
    }

    if (glDebugMessageControl != nullptr) {
        glEnable(GL_DEBUG_OUTPUT);
        glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);
        glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_LOW, 0, nullptr, GL_FALSE);
    }

    // main textures
    gDiffuseTex = Texture::loadTexture("media/lambert4.png");
    groundTex = Texture::loadTexture("media/ground.jpg");
    minionTex = Texture::loadTexture("media/RedMinion.png");

    prog.setUniform("UseTexture", gDiffuseTex != 0);
    prog.setUniform("DiffuseTex", 0);
    prog.setUniform("NormalMap", 1);

    // skybox
    initSkyboxVAO(skyboxVAO, skyboxVBO);

    gSpotlights.clear();
    gSpotlights.resize(NUM_LIGHTS);
    for (int i = 0; i < NUM_LIGHTS; ++i) {
        Spotlight& s = gSpotlights[i];
        float half = 25.0f;
        if (i == 0) {
            s.pos = vec3(0.0f, 14.0f, 0.0f);
            s.target = vec3(0.0f, 0.0f, 0.0f);
            s.dir = glm::normalize(vec3(s.target.x - s.pos.x, -s.pos.y, s.target.z - s.pos.z));
            s.speed = 0.0f;
            s.changeInterval = 99999.0f;
            s.changeTimer = s.changeInterval;
            s.range = 18.0f;
            s.cutoffCos = cos(glm::radians(89.0f));
        }
        else {
            s.pos = vec3(randomRange(-half, half), randomRange(5.0f, 9.0f), randomRange(-half, half));
            s.target = vec3(randomRange(-half, half), 0.0f, randomRange(-half, half));
            s.dir = glm::normalize(vec3(s.target.x - s.pos.x, -s.pos.y, s.target.z - s.pos.z));
            s.speed = randomRange(8.0f, 20.0f);
            s.changeInterval = randomRange(0.2f, 0.9f);
            s.changeTimer = s.changeInterval;
            s.range = 8.0f;
            s.cutoffCos = cos(glm::radians(89.0f));
        }
    }

    gMinionBirthTimes.clear();

    initParticleSystem();

    lightFrustum = std::make_unique<Frustum>();

    crownEnabled = true;
    gameLost = false;
    particleSpawnAccumulator = 0.0f;

    theWhisp.alive = true;
    theWhisp.pos = hecarimPos + glm::vec3(6.0f, 2.0f, 6.0f);
    theWhisp.vel = glm::vec3(0.0f);
    theWhisp.speed = 3.2f;
    theWhisp.radius = 1.2f;

    try {
        particleProg.compileShader("shader/particle.vert");
        particleProg.compileShader("shader/particle.frag");
        particleProg.link();
    }
    catch (GLSLProgramException& e) {
        cerr << "Particle shader compile/link failed: " << e.what() << endl;
    }

    try {
        depthProg.compileShader("shader/depth.vert");
        depthProg.compileShader("shader/depth.frag");
        depthProg.link();
    }
    catch (GLSLProgramException& e) {
        cerr << "Depth shader compile/link failed: " << e.what() << endl;
    }

    try {
        skyboxProg.compileShader("shader/sky_noise.vert");
        skyboxProg.compileShader("shader/sky_noise.frag");
        skyboxProg.link();
    }
    catch (GLSLProgramException& e) {
        cerr << "Skybox shader compile/link failed: " << e.what() << endl;
    }

    shadowTexs.assign(NUM_LIGHTS, 0u);
    shadowFBOs.assign(NUM_LIGHTS, 0u);
    lightSpaces.assign(NUM_LIGHTS, mat4(1.0f));

    for (int i = 0; i < NUM_LIGHTS; ++i) {
        glGenTextures(1, &shadowTexs[i]);
        glBindTexture(GL_TEXTURE_2D, shadowTexs[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, SHADOW_SIZE, SHADOW_SIZE, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        GLfloat borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

        glGenFramebuffers(1, &shadowFBOs[i]);
        glBindFramebuffer(GL_FRAMEBUFFER, shadowFBOs[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowTexs[i], 0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            cerr << "Shadow FBO incomplete for light " << i << "\n";
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    glm::vec3 lightPos = hecarimPos + glm::vec3(8.0f, 12.0f, 8.0f);
    lightFrustum->orient(lightPos, hecarimPos, glm::vec3(0.0f, 1.0f, 0.0f));
    lightFrustum->setPerspective(60.0f, 1.0f, 1.0f, 60.0f);
}

void SceneBasic_Uniform::update(float t)
{
    float deltaT = t - tPrev;
    if (tPrev == 0.0f) deltaT = 0.0f;
    tPrev = t;

    angle += 0.1f * deltaT;
    if (angle > glm::two_pi<float>()) angle -= glm::two_pi<float>();

    if (gameWon) return;

    GLFWwindow* win = glfwGetCurrentContext();
    if (!win) return;

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

    if (thirdPersonLocked) {
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
            if (!gameLost) {
                moveVec = glm::normalize(moveVec);
                const float speed = 10.0f;
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
        }

        float followDist = 6.0f;
        float followHeight = 2.0f;
        glm::vec3 camDir = glm::normalize(cameraFront);
        cameraPos = hecarimPos - camDir * followDist + glm::vec3(0.0f, followHeight, 0.0f);
        cameraFront = glm::normalize((hecarimPos + glm::vec3(0.0f, 1.0f, 0.0f)) - cameraPos);
        view = glm::lookAt(cameraPos, hecarimPos + glm::vec3(0.0f, 1.0f, 0.0f), cameraUp);
    }
    else {
        float cameraSpeed = 3.5f * (deltaT > 0.0f ? deltaT : 0.016f);
        if (glfwGetKey(win, GLFW_KEY_W) == GLFW_PRESS) cameraPos += cameraSpeed * cameraFront;
        if (glfwGetKey(win, GLFW_KEY_S) == GLFW_PRESS) cameraPos -= cameraSpeed * cameraFront;
        if (glfwGetKey(win, GLFW_KEY_A) == GLFW_PRESS) cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
        if (glfwGetKey(win, GLFW_KEY_D) == GLFW_PRESS) cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
        if (glfwGetKey(win, GLFW_KEY_Q) == GLFW_PRESS) cameraPos -= cameraUp * cameraSpeed;
        if (glfwGetKey(win, GLFW_KEY_E) == GLFW_PRESS) cameraPos += cameraUp * cameraSpeed;

        view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
    }

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

            Minion m;
            m.pos = spawn;
            m.alive = true;
            minions.push_back(m);
            gMinionBirthTimes.push_back(t);
        }
        else {
            spawnTimer = randomRange(1.0f, 3.0f);
        }
    }

    int qState = glfwGetKey(win, GLFW_KEY_Q);
    if (qState == GLFW_PRESS && !prevQPressed && !gameLost) {
        float collectRadius = 1.5f;
        for (size_t i = 0; i < minions.size(); ++i) {
            auto& m = minions[i];
            if (!m.alive) continue;

            float d = glm::length(
                glm::vec3(m.pos.x, 0.0f, m.pos.z) -
                glm::vec3(hecarimPos.x, 0.0f, hecarimPos.z)
            );

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

    const float lifespan = 10.0f;
    for (size_t i = 0; i < minions.size() && i < gMinionBirthTimes.size(); ++i) {
        if (!minions[i].alive) continue;
        if (t - gMinionBirthTimes[i] > lifespan) {
            minions[i].alive = false;
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

    particleRingCenter = hecarimPos + glm::vec3(0.0f, 1.6f, 0.0f);

    updateWhisp(deltaT, hecarimPos + glm::vec3(0.0f, 1.6f, 0.0f));
    updateParticles(deltaT, particleRingCenter);
}

void SceneBasic_Uniform::render()
{
    if (!gSpotlights.empty()) {
        gSpotlights[0].pos = vec3(0.0f, 14.0f, 0.0f);
        gSpotlights[0].target = vec3(0.0f, 0.0f, 0.0f);
        gSpotlights[0].dir = glm::normalize(vec3(
            gSpotlights[0].target.x - gSpotlights[0].pos.x,
            -gSpotlights[0].pos.y,
            gSpotlights[0].target.z - gSpotlights[0].pos.z
        ));
        gSpotlights[0].speed = 0.0f;
        gSpotlights[0].changeInterval = 99999.0f;
        gSpotlights[0].changeTimer = gSpotlights[0].changeInterval;
    }

    // shadow pass
    if (depthProg.isLinked() && !shadowFBOs.empty() && !shadowTexs.empty() && lightFrustum) {
        GLint prevViewport[4];
        glGetIntegerv(GL_VIEWPORT, prevViewport);
        glCullFace(GL_FRONT);

        // Skip center light (index 0) when rendering shadow maps
        for (int i = 1; i < NUM_LIGHTS; ++i) {
            glm::vec3 lightPos = gSpotlights[i].pos;
            glm::vec3 lightTarget = gSpotlights[i].target;
            lightFrustum->orient(lightPos, lightTarget, glm::vec3(0.0f, 1.0f, 0.0f));
            lightFrustum->setPerspective(60.0f, 1.0f, 1.0f, 60.0f);

            mat4 lightView = lightFrustum->getViewMatrix();
            mat4 lightProj = lightFrustum->getProjectionMatrix();
            mat4 lightSpace = lightProj * lightView;
            lightSpaces[i] = lightSpace;

            glViewport(0, 0, SHADOW_SIZE, SHADOW_SIZE);
            glBindFramebuffer(GL_FRAMEBUFFER, shadowFBOs[i]);
            glClear(GL_DEPTH_BUFFER_BIT);

            depthProg.use();

            mat4 savedModel = model;

            model = mat4(1.0f);
            depthProg.setUniform("LightMVP", lightSpace * model);
            plane.render();

            model = mat4(1.0f);
            model = glm::translate(model, hecarimPos);
            model = glm::rotate(model, glm::radians(hecarimYaw + 180.0f), vec3(0, 1, 0));
            model = glm::scale(model, vec3(0.015f));
            depthProg.setUniform("LightMVP", lightSpace * model);
            if (mesh) mesh->render();

            if (minionMesh) {
                for (auto& m : minions) {
                    if (!m.alive) continue;
                    model = mat4(1.0f);
                    model = glm::translate(model, m.pos + vec3(0.0f, 0.2f, 0.0f));
                    model = glm::scale(model, vec3(0.02f));
                    depthProg.setUniform("LightMVP", lightSpace * model);
                    minionMesh->render();
                }
            }

            model = savedModel;
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }

        glCullFace(GL_BACK);
        glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);

        prog.use();

        GLint maxTexUnits = 0;
        glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &maxTexUnits);
        int reservedUnits = 8;
        int available = maxTexUnits - reservedUnits;
        if (available < 1) available = 1;

        int bindCount = std::min(NUM_LIGHTS - 1, available); // prefer binding non-center lights
        int baseUnit = reservedUnits;

        // bind order: put non-center lights first, center (0) last so it's effectively ignored
        std::vector<int> bindOrder;
        bindOrder.reserve(NUM_LIGHTS);
        for (int i = 1; i < NUM_LIGHTS; ++i) bindOrder.push_back(i);
        bindOrder.push_back(0);

        for (int k = 0; k < bindCount; ++k) {
            int i = bindOrder[k];
            int unit = baseUnit + k;
            prog.setUniform((std::string("ShadowMap[") + std::to_string(i) + "]").c_str(), unit);
            prog.setUniform((std::string("LightSpace[") + std::to_string(i) + "]").c_str(), lightSpaces[i]);
            glActiveTexture(GL_TEXTURE0 + unit);
            glBindTexture(GL_TEXTURE_2D, shadowTexs[i]);
        }

        for (int k = bindCount; k < NUM_LIGHTS; ++k) {
            int i = bindOrder[k];
            // ensure leftover entries don't reference a bound texture
            prog.setUniform((std::string("ShadowMap[") + std::to_string(i) + "]").c_str(), 0);
            prog.setUniform((std::string("LightSpace[") + std::to_string(i) + "]").c_str(), glm::mat4(1.0f));
        }

        glActiveTexture(GL_TEXTURE0);
    }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glDepthFunc(GL_LEQUAL);

    skyboxProg.use();

    mat4 viewNoTrans = glm::mat4(glm::mat3(view));
    skyboxProg.setUniform("ViewNoTrans", viewNoTrans);
    skyboxProg.setUniform("Projection", projection);
    skyboxProg.setUniform("Time", tPrev);

    glBindVertexArray(skyboxVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);

    glDepthFunc(GL_LESS);

    prog.use();

    for (int i = 0; i < NUM_LIGHTS; ++i) {
        const Spotlight& s = gSpotlights[i];
        vec4 posView = view * vec4(s.pos, 1.0f);
        vec3 dirView = vec3(view * vec4(s.dir, 0.0f));
        prog.setUniform((std::string("Lights[") + std::to_string(i) + "].Position").c_str(), posView);

        // Disable the center light (index 0)
        if (i == 0) {
            prog.setUniform((std::string("Lights[") + std::to_string(i) + "].L").c_str(), vec3(0.0f));
            prog.setUniform((std::string("Lights[") + std::to_string(i) + "].La").c_str(), vec3(0.0f));
        }
        else {
            prog.setUniform((std::string("Lights[") + std::to_string(i) + "].L").c_str(), vec3(30.0f));
            prog.setUniform((std::string("Lights[") + std::to_string(i) + "].La").c_str(), vec3(0.05f));
        }

        prog.setUniform((std::string("Lights[") + std::to_string(i) + "].SpotDirection").c_str(), dirView);
        prog.setUniform((std::string("Lights[") + std::to_string(i) + "].SpotCutoff").c_str(), s.cutoffCos);
        prog.setUniform((std::string("Lights[") + std::to_string(i) + "].SpotExponent").c_str(), 30.0f);
    }

    // Hecarim - legacy Blinn-Phong path
    prog.setUniform("UseNormalMap", false);
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

    model = mat4(1.0f);
    model = glm::translate(model, hecarimPos);
    model = glm::rotate(model, glm::radians(hecarimYaw + 180.0f), vec3(0, 1, 0));
    model = glm::scale(model, vec3(0.015f));
    setMatrices();
    if (mesh) mesh->render();

    // Minions - legacy path
    prog.setUniform("UseNormalMap", false);

    if (minionMesh && minionTex) {
        for (auto& m : minions) {
            if (!m.alive) continue;

            prog.setUniform("Material.Kd", vec3(0.8f));
            prog.setUniform("Material.Ks", vec3(0.1f));
            prog.setUniform("Material.Ka", vec3(0.1f));
            prog.setUniform("Material.Shininess", 32.0f);

            glActiveTexture(GL_TEXTURE7);
            glBindTexture(GL_TEXTURE_2D, minionTex);
            prog.setUniform("UseTexture", true);
            prog.setUniform("DiffuseTex", 7);

            model = mat4(1.0f);
            model = glm::translate(model, m.pos + vec3(0.0f, 0.2f, 0.0f));
            model = glm::scale(model, vec3(0.02f));
            setMatrices();
            minionMesh->render();
        }
    }

    // Ground - legacy path
    prog.setUniform("UseNormalMap", false);
    prog.setUniform("Material.Kd", vec3(0.7f));
    prog.setUniform("Material.Ks", vec3(0.0f));
    prog.setUniform("Material.Ka", vec3(0.2f));
    prog.setUniform("Material.Shininess", 180.0f);

    if (groundTex) {
        glActiveTexture(GL_TEXTURE7);
        glBindTexture(GL_TEXTURE_2D, groundTex);
        prog.setUniform("UseTexture", true);
        prog.setUniform("DiffuseTex", 7);
    }
    else {
        prog.setUniform("UseTexture", false);
    }

    model = mat4(1.0f);
    setMatrices();
    plane.render();

    renderParticles(view, projection);

    glActiveTexture(GL_TEXTURE0);
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
    prog.setUniform("Model", model);
}