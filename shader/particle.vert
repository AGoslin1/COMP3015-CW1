
#version 460 core

layout(location = 0) in vec2 VertexPos;     
layout(location = 1) in vec4 InstancePosSize;    
layout(location = 2) in vec4 InstanceLifePad;     

uniform mat4 Projection;
uniform mat4 View;
uniform vec3 CameraRight;
uniform vec3 CameraUp;   

out vec3 FragPosView;     
flat out vec3 CenterView;     
flat out float RadiusView;     
flat out float Life;          
flat out float TypeID;         

void main()
{
    vec3 center = InstancePosSize.xyz;
    float radius = InstancePosSize.w;

    // center and camera basis into view space
    vec4 centerView4 = View * vec4(center, 1.0);
    CenterView = centerView4.xyz;
    vec3 rightView = (View * vec4(CameraRight, 0.0)).xyz;
    vec3 upView    = (View * vec4(CameraUp, 0.0)).xyz;

    // scale VertexPos so quad covers the circle
    vec3 offsetView = rightView * (VertexPos.x * radius * 2.0) + upView * (VertexPos.y * radius * 2.0);
    vec4 posView = vec4(CenterView + offsetView, 1.0);

    FragPosView = posView.xyz;
    RadiusView = radius;
    Life = clamp(InstanceLifePad.x, 0.0, 1.0);
    TypeID = InstanceLifePad.y;

    gl_Position = Projection * posView;
}