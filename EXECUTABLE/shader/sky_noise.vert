#version 460 core

layout(location = 0) in vec3 aPosition;

out vec3 vDir;

uniform mat4 Projection;
uniform mat4 ViewNoTrans;

void main()
{
    vDir = aPosition;

    vec4 pos = Projection * ViewNoTrans * vec4(aPosition, 1.0);
    gl_Position = pos.xyww; // keeps skybox at infinity
}