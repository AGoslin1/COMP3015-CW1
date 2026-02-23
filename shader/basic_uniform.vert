#version 460

layout (location = 0) in vec3 VertexPosition;
layout (location = 1) in vec3 VertexNormal;
layout (location = 2) in vec2 VertexTexCoord;
layout (location = 3) in vec4 VertexTangent; // xyz = tangent, w = bitangent sign

out vec3 Position;
out vec3 Normal;
out vec2 TexCoord;
out vec3 SkyboxDir;
out vec3 Tangent;
out vec3 Bitangent;

uniform mat4 ModelViewMatrix;
uniform mat3 NormalMatrix;
uniform mat4 MVP;

// Skybox support
uniform bool RenderSkybox = false;
uniform mat4 ViewNoTrans; // view matrix with translation removed (for skybox)
uniform mat4 Projection;  // for skybox projection

void main()
{
    if (RenderSkybox) {
        // For skybox draw: pass the vertex position as sampling direction
        SkyboxDir = VertexPosition;
        vec4 pos = Projection * ViewNoTrans * vec4(VertexPosition, 1.0);
        // ensure depth is at far plane
        gl_Position = pos.xyww;
        // set other outputs to safe defaults
        Position = vec3(0.0);
        Normal = vec3(0.0, 0.0, 1.0);
        Tangent = vec3(1.0, 0.0, 0.0);
        Bitangent = vec3(0.0, 1.0, 0.0);
        TexCoord = vec2(0.0);
        return;
    }

    Normal = normalize(NormalMatrix * VertexNormal);
    // transform tangent to eye space; VertexTangent.w is handedness
    vec3 T = normalize(NormalMatrix * VertexTangent.xyz);
    vec3 B = normalize(cross(Normal, T) * VertexTangent.w);
    Tangent = T;
    Bitangent = B;

    Position = (ModelViewMatrix * vec4(VertexPosition, 1.0)).xyz;
    TexCoord = VertexTexCoord;
    gl_Position = MVP * vec4(VertexPosition, 1.0);
}