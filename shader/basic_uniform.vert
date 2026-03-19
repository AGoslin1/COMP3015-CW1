#version 460

layout (location = 0) in vec3 VertexPosition;
layout (location = 1) in vec3 VertexNormal;
layout (location = 2) in vec2 VertexTexCoord;
layout (location = 3) in vec4 VertexTangent;

out vec3 Position;
out vec3 Normal;
out vec2 TexCoord;
out vec3 SkyboxDir;
out vec3 Tangent;
out vec3 Bitangent;
out vec4 WorldPos;

uniform mat4 ModelViewMatrix;
uniform mat3 NormalMatrix;
uniform mat4 MVP;
uniform mat4 Model;

uniform bool RenderSkybox = false;
uniform mat4 ViewNoTrans;
uniform mat4 Projection;

void main()
{
    if (RenderSkybox) {
        SkyboxDir = VertexPosition;
        vec4 pos = Projection * ViewNoTrans * vec4(VertexPosition, 1.0);
        gl_Position = pos.xyww;

        Position = vec3(0.0);
        Normal = vec3(0.0, 0.0, 1.0);
        Tangent = vec3(1.0, 0.0, 0.0);
        Bitangent = vec3(0.0, 1.0, 0.0);
        TexCoord = vec2(0.0);
        WorldPos = vec4(0.0);
        return;
    }

    Position = (ModelViewMatrix * vec4(VertexPosition, 1.0)).xyz;
    Normal = normalize(NormalMatrix * VertexNormal);

    vec3 T = normalize(NormalMatrix * VertexTangent.xyz);
    vec3 B = normalize(cross(Normal, T)) * VertexTangent.w;

    Tangent = T;
    Bitangent = B;

    TexCoord = VertexTexCoord;

    WorldPos = Model * vec4(VertexPosition, 1.0);

    gl_Position = MVP * vec4(VertexPosition, 1.0);
}