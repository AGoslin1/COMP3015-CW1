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

class SceneBasic_Uniform : public Scene
{
private:
    Plane plane;
    Teapot teapot;

    std::unique_ptr<ObjMesh> mesh;

    float tPrev;
    float angle;
    GLSLProgram prog;
    void setMatrices();
    void compile();

    // Skybox
    GLSLProgram skyProg;
    GLuint skyboxTex = 0;
    GLuint skyboxVAO = 0;
    GLuint skyboxVBO = 0;

public:
    SceneBasic_Uniform();

    void initScene();
    void update(float t);
    void render();
    void resize(int, int);
};

#endif // SCENEBASIC_UNIFORM_H