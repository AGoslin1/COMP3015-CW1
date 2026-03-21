#version 460 core

in vec3 vDir;
out vec4 FragColor;

uniform float Time;

// ------------------- HASH -------------------
float hash(vec3 p) {
    return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453);
}

// ------------------- NOISE -------------------
float noise(vec3 p) {
    vec3 i = floor(p);
    vec3 f = fract(p);

    float a = hash(i);
    float b = hash(i + vec3(1,0,0));
    float c = hash(i + vec3(0,1,0));
    float d = hash(i + vec3(1,1,0));
    float e = hash(i + vec3(0,0,1));
    float f1 = hash(i + vec3(1,0,1));
    float g = hash(i + vec3(0,1,1));
    float h = hash(i + vec3(1,1,1));

    vec3 u = f * f * (3.0 - 2.0 * f);

    return mix(
        mix(mix(a, b, u.x), mix(c, d, u.x), u.y),
        mix(mix(e, f1, u.x), mix(g, h, u.x), u.y),
        u.z
    );
}

// ------------------- FBM -------------------
float fbm(vec3 p) {
    float value = 0.0;
    float amp = 0.5;

    for(int i = 0; i < 5; i++) {
        value += noise(p) * amp;
        p *= 2.0;
        amp *= 0.5;
    }

    return value;
}

void main()
{
    vec3 dir = normalize(vDir);

    // ------------------- SKY BASE -------------------
    float horizon = clamp(dir.y * 0.5 + 0.5, 0.0, 1.0);

    vec3 skyColor = mix(
        vec3(0.02, 0.02, 0.05),   // horizon tint
        vec3(0.0, 0.0, 0.0),      // deep space
        horizon
    );

    // ------------------- STARS -------------------
    float starField = noise(dir * 300.0);

    // MUCH more stars + slightly bigger
    float stars = smoothstep(0.985, 1.0, starField);

    // soften stars (makes them bigger than 1px)
    stars += smoothstep(0.97, 1.0, starField) * 0.5;

    // gentle twinkle (NOT aggressive)
    float twinkle = 0.8 + 0.2 * sin(Time * 1.5 + starField * 50.0);
    stars *= twinkle;

    // slight color variation (blue-ish stars)
    vec3 starColor = mix(vec3(1.0), vec3(0.7, 0.8, 1.0), noise(dir * 100.0));

    vec3 starsFinal = starColor * stars * 1.2;

// ------------------- NEBULA (RICH + BALANCED) -------------------
float neb1 = fbm(dir * 4.0 + Time * 0.015);
float neb2 = fbm(dir * 10.0 - Time * 0.01);

// Combine layers → more detail
float neb = neb1 * 0.7 + neb2 * 0.3;

// MUCH more presence
neb = smoothstep(0.3, 0.8, neb);

// Colour noise (controls palette distribution)
float colorNoise = fbm(dir * 3.5);

// Palette (balanced + vivid but still dark)
vec3 deepBlue = vec3(0.03, 0.08, 0.25);
vec3 purple   = vec3(0.25, 0.05, 0.4);
vec3 darkRed  = vec3(0.4, 0.05, 0.08);

//  Balanced colour blending (THIS is the key fix)
float rWeight = smoothstep(0.4, 0.9, colorNoise);
float bWeight = smoothstep(0.0, 0.6, colorNoise);
float pWeight = 1.0 - abs(colorNoise - 0.5) * 2.0;

// Normalize weights so they’re equal contributors
float total = rWeight + bWeight + pWeight + 0.001;
rWeight /= total;
bWeight /= total;
pWeight /= total;

// Final colour mix
vec3 nebulaColor =
    deepBlue * bWeight +
    purple   * pWeight +
    darkRed  * rWeight;

// Stronger visibility but still space-like
nebulaColor *= neb * 0.4;

    // ------------------- FINAL -------------------
    vec3 finalColor = skyColor + starsFinal + nebulaColor;

    FragColor = vec4(finalColor, 1.0);
}