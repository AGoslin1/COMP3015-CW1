
#version 460 core

layout(location = 0) in vec2 VertexPos;            // quad vertex coords [-0.5,0.5]
layout(location = 1) in vec4 InstancePosSize;     // xyz = world pos, w = world-space radius
layout(location = 2) in vec4 InstanceLifePad;     // x = life (0..1), y = type id

uniform mat4 Projection;
uniform mat4 View;
uniform vec3 CameraRight; // world-space camera right
uniform vec3 CameraUp;    // world-space camera up

out vec3 FragPosView;     // per-vertex view-space position (of quad vertex)
flat out vec3 CenterView;      // particle center in view space (flat)
flat out float RadiusView;     // radius (world units) (flat)
flat out float Life;           // life (flat)
flat out float TypeID;         // type id (flat)

void main()
{
    vec3 center = InstancePosSize.xyz;
    float radius = InstancePosSize.w;

    // Transform center and camera basis into view space
    vec4 centerView4 = View * vec4(center, 1.0);
    CenterView = centerView4.xyz;
    vec3 rightView = (View * vec4(CameraRight, 0.0)).xyz;
    vec3 upView    = (View * vec4(CameraUp, 0.0)).xyz;

    // scale VertexPos so quad covers the full projected circle (half-extents = radius)
    vec3 offsetView = rightView * (VertexPos.x * radius * 2.0) + upView * (VertexPos.y * radius * 2.0);
    vec4 posView = vec4(CenterView + offsetView, 1.0);

    FragPosView = posView.xyz;
    RadiusView = radius;
    Life = clamp(InstanceLifePad.x, 0.0, 1.0);
    TypeID = InstanceLifePad.y;

    gl_Position = Projection * posView;
}