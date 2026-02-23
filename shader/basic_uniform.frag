
#version 460

in vec3 Position;
in vec3 Normal;
in vec2 TexCoord;
in vec3 SkyboxDir;
in vec3 Tangent;
in vec3 Bitangent;

layout (location = 0) out vec4 FragColor;

// Skybox sampler + toggle
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
} Light;

uniform struct MaterialInfo { vec3 Kd; vec3 Ka; vec3 Ks; float Shininess; } Material;
uniform struct FogInfo { float MaxDist; float MinDist; vec3 Color; } Fog;

vec3 blinnPhong(vec3 position, vec3 n, vec3 baseColor) {
    vec3 ambient = Light.La * (Material.Ka * baseColor);

    vec3 s = normalize(Light.Position.xyz - position);
    float sDotN = max(dot(s, n), 0.0);
    vec3 diffuse = baseColor * sDotN;

    vec3 spec = vec3(0.0);
    if (sDotN > 0.0) {
        vec3 v = normalize(-position.xyz);
        vec3 h = normalize(v + s);
        spec = Material.Ks * pow(max(dot(h, n), 0.0), Material.Shininess);
    }

    // Spotlight factor
    float spotFactor = 1.0;
    if (Light.SpotCutoff > 0.0) {
        float cosAngle = dot(normalize(Light.SpotDirection), normalize(-s));
        if (cosAngle <= Light.SpotCutoff) {
            spotFactor = 0.0;
        } else {
            spotFactor = pow(cosAngle, Light.SpotExponent);
        }
    }

    // Distance attenuation
    float dist = length(Light.Position.xyz - position);
    float attenuation = 1.0 / (1.0 + 0.09 * dist + 0.032 * dist * dist);

    return ambient + (diffuse + spec) * Light.L * spotFactor * attenuation;
}

void main() {
    if (RenderSkybox) {
        // sample cubemap using direction stored in SkyboxDir
        vec3 col = texture(Skybox, SkyboxDir).rgb;
        FragColor = vec4(col, 1.0);
        return;
    }

    // base color (texture or material)
    vec3 baseColor = UseTexture ? texture(DiffuseTex, TexCoord).rgb : Material.Kd;

    // start with interpolated normal (eye space)
    vec3 n = normalize(Normal);

    if (UseNormalMap) {
        vec3 nm = texture(NormalMap, TexCoord).rgb * 2.0 - 1.0;
        vec3 T = normalize(Tangent);
        vec3 B = normalize(Bitangent);
        mat3 TBN = mat3(T, B, normalize(Normal));
        n = normalize(TBN * nm);
    }

    vec3 shadeColor = blinnPhong(Position, n, baseColor);

    float dist = abs(Position.z);
    float fogFactor = clamp((Fog.MaxDist - dist) / (Fog.MaxDist - Fog.MinDist), 0.0, 1.0);
    vec3 color = mix(Fog.Color, shadeColor, fogFactor);

    FragColor = vec4(color, 1.0);
}