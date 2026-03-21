#version 460 core

layout(location = 0) in vec3 VertexPosition;

uniform mat4 LightMVP; // light projection * view * model

void main()
{
    gl_Position = LightMVP * vec4(VertexPosition, 1.0);
}