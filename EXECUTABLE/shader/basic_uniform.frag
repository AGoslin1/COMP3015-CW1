#version 460

#define NUM_LIGHTS 8

in vec3 Position;
in vec3 Normal;
in vec2 TexCoord;
in vec3 SkyboxDir;
in vec3 Tangent;
in vec3 Bitangent;
in vec4 WorldPos;

layout (location = 0) out vec4 FragColor;

uniform bool RenderSkybox = false;
uniform samplerCube Skybox;

uniform sampler2D DiffuseTex;
uniform bool UseTexture;
uniform sampler2D NormalMap;
uniform bool UseNormalMap;

uniform bool UseFog = false;
uniform bool UseShadows = true;

uniform sampler2D ShadowMap[NUM_LIGHTS];
uniform mat4 LightSpace[NUM_LIGHTS];

uniform struct LightInfo {
    vec4 Position;
    vec3 La;
    vec3 L;
    vec3 SpotDirection;
    float SpotCutoff;
    float SpotExponent;
} Lights[NUM_LIGHTS];

uniform struct MaterialInfo {
    vec3 Kd;
    vec3 Ka;
    vec3 Ks;
    float Shininess;
} Material;

uniform struct FogInfo {
    float MaxDist;
    float MinDist;
    vec3 Color;
} Fog;

vec3 blinnPhongSingle(in LightInfo L, vec3 position, vec3 n, vec3 baseColor)
{
    vec3 ambient = L.La * (Material.Ka * baseColor);

    vec3 s = normalize(L.Position.xyz - position);
    float sDotN = max(dot(s, n), 0.0);
    vec3 diffuse = Material.Kd * baseColor * sDotN;

    vec3 spec = vec3(0.0);
    if (sDotN > 0.0) {
        vec3 v = normalize(-position);
        vec3 h = normalize(v + s);
        spec = Material.Ks * pow(max(dot(h, n), 0.0), Material.Shininess);
    }

    float spotFactor = 1.0;
    if (L.SpotCutoff > 0.0) {
        float cosAngle = dot(normalize(L.SpotDirection), normalize(-s));
        if (cosAngle <= L.SpotCutoff)
            spotFactor = 0.0;
        else
            spotFactor = pow(cosAngle, L.SpotExponent);
    }

    float dist = length(L.Position.xyz - position);
    float attenuation = 1.0 / (1.0 + 0.09 * dist + 0.032 * dist * dist);

    return ambient + (diffuse + spec) * L.L * spotFactor * attenuation;
}

float computeShadowForLight(int idx, vec4 worldPos, vec3 n, vec3 viewPos)
{
    if (!UseShadows) return 1.0;

    vec4 lightCoord = LightSpace[idx] * worldPos;
    if (lightCoord.w <= 0.0) return 1.0;

    vec3 projCoords = lightCoord.xyz / lightCoord.w;
    vec2 uv = projCoords.xy * 0.5 + 0.5;
    float currentDepth = projCoords.z * 0.5 + 0.5;

    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
        return 1.0;

    vec3 lightDirVS = normalize(Lights[idx].Position.xyz - viewPos);
    float bias = max(0.0002 * (1.0 - dot(normalize(n), lightDirVS)), 0.0002);

    float shadow = 0.0;
    float samples = 0.0;
    float texelSize = 1.0 / 2048.0;

    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            float closestDepth = texture(ShadowMap[idx], uv + offset).r;
            shadow += (currentDepth - bias > closestDepth) ? 0.0 : 1.0;
            samples += 1.0;
        }
    }

    return shadow / samples;
}

void main()
{
    if (RenderSkybox) {
        FragColor = vec4(texture(Skybox, SkyboxDir).rgb, 1.0);
        return;
    }

    vec3 baseColor = UseTexture ? texture(DiffuseTex, TexCoord).rgb : Material.Kd;

    vec3 n = normalize(Normal);

    if (UseNormalMap) {
        vec3 nm = texture(NormalMap, TexCoord).rgb * 2.0 - 1.0;
        mat3 TBN = mat3(normalize(Tangent), normalize(Bitangent), normalize(Normal));
        n = normalize(TBN * nm);
    }

    vec3 accum = vec3(0.0);

    for (int i = 0; i < NUM_LIGHTS; ++i) {
        vec3 contrib = blinnPhongSingle(Lights[i], Position, n, baseColor);
        vec3 ambient = Lights[i].La * (Material.Ka * baseColor);
        vec3 direct = contrib - ambient;

        float shadow = computeShadowForLight(i, WorldPos, n, Position);

        accum += ambient + direct * shadow;
    }

    vec3 color = accum;

    if (UseFog) {
        float dist = abs(Position.z);
        float fogFactor = clamp((Fog.MaxDist - dist) / (Fog.MaxDist - Fog.MinDist), 0.0, 1.0);
        color = mix(Fog.Color, color, fogFactor);
    }

    FragColor = vec4(color, 1.0);
}