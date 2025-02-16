#version 330 core

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

layout(location = 0) out vec4 FragColor;

uniform vec3 cameraPos;
uniform vec3 lightDir;
uniform vec3 lightColor;
uniform vec3 ambientColor;
uniform float ambientIntensity;
uniform float specularShininess;
uniform float specularIntensity;

uniform sampler2D texture_diffuse0;
uniform sampler2D texture_specular0;
uniform sampler2D texture_normal0;

vec3 CalcBlinnPhong(vec3 viewDir, vec3 normal, vec3 lightDir, vec3 lightColor)
{
    // Diffuse
    float diff = max(dot(normal, lightDir), 0.0);
    // Specular
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(viewDir, halfwayDir), 0.0), specularShininess);

    return (ambientColor * ambientIntensity + (diff * lightColor + spec * lightColor * specularIntensity));
}

void main()
{
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(cameraPos - FragPos);
    vec3 Albedo = texture(texture_diffuse0, TexCoords).rgb;

    vec3 result = CalcBlinnPhong(viewDir, norm, lightDir, lightColor) * Albedo;

    FragColor = vec4(result, 1.0);
}