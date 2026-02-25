#version 460

#define NUM_LIGHTS 80

in vec3 Position;
in vec3 Normal;
in vec2 TexCoord;
in vec3 SkyboxDir;
in vec3 Tangent;
in vec3 Bitangent;

layout (location = 0) out vec4 FragColor;

uniform bool RenderSkybox = false;
uniform samplerCube Skybox;

uniform sampler2D DiffuseTex;
uniform bool UseTexture;
uniform sampler2D NormalMap;
uniform bool UseNormalMap;

uniform struct LightInfo {
    vec4 Position;
    vec3 La;
    vec3 L;
    vec3 SpotDirection;
    float SpotCutoff;
    float SpotExponent;
} Lights[NUM_LIGHTS];

uniform struct MaterialInfo { vec3 Kd; vec3 Ka; vec3 Ks; float Shininess; } Material;
uniform struct FogInfo { float MaxDist; float MinDist; vec3 Color; } Fog;

vec3 blinnPhongSingle(in LightInfo L, vec3 position, vec3 n, vec3 baseColor) {
    vec3 ambient = L.La * (Material.Ka * baseColor);

    vec3 s = normalize(L.Position.xyz - position);
    float sDotN = max(dot(s, n), 0.0);
    vec3 diffuse = Material.Kd * baseColor * sDotN;

    vec3 spec = vec3(0.0);
    if (sDotN > 0.0) {
        vec3 v = normalize(-position.xyz);
        vec3 h = normalize(v + s);
        spec = Material.Ks * pow(max(dot(h, n), 0.0), Material.Shininess);
    }

    //spotlight
    float spotFactor = 1.0;
    if (L.SpotCutoff > 0.0) {
        float cosAngle = dot(normalize(L.SpotDirection), normalize(-s));
        if (cosAngle <= L.SpotCutoff) {
            spotFactor = 0.0;
        } else {
            spotFactor = pow(cosAngle, L.SpotExponent);
        }
    }

    // Distance attenuation
    float dist = length(L.Position.xyz - position);
    float attenuation = 1.0 / (1.0 + 0.09 * dist + 0.032 * dist * dist);

    return ambient + (diffuse + spec) * L.L * spotFactor * attenuation;
}

void main() {
    if (RenderSkybox) {
        vec3 col = texture(Skybox, SkyboxDir).rgb;
        FragColor = vec4(col, 1.0);
        return;
    }

    vec3 baseColor = UseTexture ? texture(DiffuseTex, TexCoord).rgb : Material.Kd;

    vec3 n = normalize(Normal);
    if (UseNormalMap) {
        vec3 nm = texture(NormalMap, TexCoord).rgb * 2.0 - 1.0;
        vec3 T = normalize(Tangent);
        vec3 B = normalize(Bitangent);
        mat3 TBN = mat3(T, B, normalize(Normal));
        n = normalize(TBN * nm);
    }

    //get all contributions of the lights
    vec3 accum = vec3(0.0);
    for (int i = 0; i < NUM_LIGHTS; ++i) {
        accum += blinnPhongSingle(Lights[i], Position, n, baseColor);
    }

    float dist = abs(Position.z);
    float fogFactor = clamp((Fog.MaxDist - dist) / (Fog.MaxDist - Fog.MinDist), 0.0, 1.0);
    vec3 color = mix(Fog.Color, accum, fogFactor);

    FragColor = vec4(color, 1.0);
}