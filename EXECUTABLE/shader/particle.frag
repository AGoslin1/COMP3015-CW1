#version 460 core

in vec3 FragPosView;   // fragment position (in view space) on particle quad plane
flat in vec3 CenterView;    // particle center in view space
flat in float RadiusView;   // particle radius (world units)
flat in float Life;         // 0..1 (1 = new, 0 = dead)
flat in float TypeID;       // 0=crown,1=explosion,2=whisp trail

layout (location = 0) out vec4 FragColor;

// view-space lighting direction
const vec3 lightDirView = normalize(vec3(0.3, 0.8, 0.6));
const float ambientBase = 0.12;
const float specStrength = 0.5;
const float specPower = 20.0;

void main()
{
    vec2 s = FragPosView.xy - CenterView.xy;
    float dist2 = dot(s, s);
    float r2 = RadiusView * RadiusView;
    if (dist2 > r2) discard;

    float zSurf = sqrt(max(0.0, r2 - dist2));
    vec3 normal = normalize(vec3(s.x, s.y, zSurf));

    float l = clamp(Life, 0.0, 1.0);
    vec3 color;

    if (TypeID < 0.5) {
        // crown / fire-like (orange -> red -> grey -> black)
        vec3 colOrange = vec3(1.0, 0.60, 0.10);
        vec3 colRed    = vec3(1.0, 0.12, 0.03);
        vec3 colGrey   = vec3(0.48, 0.48, 0.48);
        vec3 colBlack  = vec3(0.02, 0.02, 0.02);
        if (l > 0.66) {
            float t = (l - 0.66) / (1.0 - 0.66);
            color = mix(colRed, colOrange, t);
        } else if (l > 0.33) {
            float t = (l - 0.33) / (0.66 - 0.33);
            color = mix(colGrey, colRed, t);
        } else {
            float t = l / 0.33;
            color = mix(colBlack, colGrey, t);
        }
    } else if (TypeID < 1.5) {
        // explosion (use brighter fire)
        vec3 colOrange = vec3(1.0, 0.70, 0.20);
        vec3 colRed    = vec3(1.0, 0.28, 0.06);
        vec3 colGrey   = vec3(0.6, 0.6, 0.6);
        vec3 colBlack  = vec3(0.05,0.05,0.05);
        if (l > 0.66) {
            float t = (l - 0.66) / (1.0 - 0.66);
            color = mix(colRed, colOrange, t);
        } else if (l > 0.33) {
            float t = (l - 0.33) / (0.66 - 0.33);
            color = mix(colGrey, colRed, t);
        } else {
            float t = l / 0.33;
            color = mix(colBlack, colGrey, t);
        }
    } else {
        // whisp trail: dark bluish -> grey
        vec3 colYoung = vec3(0.06, 0.10, 0.16); // very dark blue
        vec3 colMid   = vec3(0.12, 0.16, 0.22);
        vec3 colGrey  = vec3(0.45, 0.48, 0.52);
        if (l > 0.5) {
            float t = (l - 0.5) / 0.5;
            color = mix(colMid, colYoung, t);
        } else {
            float t = l / 0.5;
            color = mix(vec3(0.02), colGrey, t);
        }
    }

    // lighting
    vec3 viewDir = normalize(vec3(0.0, 0.0, 1.0));
    float diff = max(dot(normal, lightDirView), 0.0);
    vec3 halfVec = normalize(lightDirView + viewDir);
    float spec = pow(max(dot(normal, halfVec), 0.0), specPower) * specStrength;
    float lighting = ambientBase + diff * 0.85 + spec;

    // brightness envelope (a bit different per type)
    float age = 1.0 - l;
    float rampUp = smoothstep(0.0, 0.5, age);
    float rampDown = 1.0 - smoothstep(0.6, 1.0, age);
    float brightness = mix(0.25, 1.1, rampUp) * rampDown;
    if (TypeID >= 1.0 && TypeID < 1.5) brightness *= 1.15; // explosion brighter
    if (TypeID >= 2.0) brightness *= 0.6; // whisp darker

    vec3 finalColor = color * lighting * brightness;

    float edgeSoftness = 0.18;
    float circleAlpha = 1.0 - smoothstep(1.0 - edgeSoftness, 1.0, sqrt(dist2) / RadiusView);

    float alpha = circleAlpha * l;
    if (alpha < 0.01) discard;

    FragColor = vec4(finalColor, alpha);
}