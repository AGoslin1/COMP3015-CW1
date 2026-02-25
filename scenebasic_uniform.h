#include "helper/scene.h"
#ifndef SCENEBASIC_UNIFORM_H
#define SCENEBASIC_UNIFORM_H

#include "helper/scene.h"

#include <glad/glad.h>
#include "helper/glslprogram.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>
#include "helper/plane.h"
#include "helper/teapot.h"
#include "helper/objmesh.h"

#include <vector>

class SceneBasic_Uniform : public Scene
{
private:
    Plane plane;
    Teapot teapot;

    std::unique_ptr<ObjMesh> mesh;
    std::unique_ptr<ObjMesh> minionMesh; 

    float tPrev;
    float angle;
    GLSLProgram prog;
    void setMatrices();
    void compile();

    GLSLProgram skyProg;
    GLuint skyboxTex = 0;
    GLuint skyboxVAO = 0;
    GLuint skyboxVBO = 0;

    GLuint minionTex = 0;
    GLuint groundTex = 0;

    struct Minion {
        glm::vec3 pos;
        bool alive;
    };
    std::vector<Minion> minions;
    int collectedCount;
    float spawnTimer;
    float nextSpawnInterval;
    int maxMinions;

    bool gameWon;
    int winTarget = 15; 


    glm::vec3 hecarimPos;
    float hecarimYaw;
    bool thirdPersonLocked;
    bool prevTabPressed;
    bool prevQPressed;

public:
    SceneBasic_Uniform();

    void initScene();
    void update(float t);
    void render();
    void resize(int, int);
};

#endif // SCENEBASIC_UNIFORM_H