#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoords;
layout(location = 3) in ivec4 aBoneIds;
layout(location = 4) in vec4 aWeights;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoords;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

uniform bool animated;
const int MAX_BONES = 100;
const int MAX_BONE_INFLUENCE = 4;
uniform mat4 finalBonesMatrices[MAX_BONES];

vec4 applyBoneTransform(vec4 pos)
{
    vec4 result = vec4(0.0f);
    for (int i = 0; i < MAX_BONE_INFLUENCE; ++i)
    {
        if (aBoneIds[i] == -1)
            continue;
        if (aBoneIds[i] >= MAX_BONES)
        {
            result = pos;
            break;
        }
        result += aWeights[i] * (finalBonesMatrices[aBoneIds[i]] * pos);
    }
    return result;
}

void main()
{
    vec4 totalPosition = vec4(0.0);
    if (animated)
    {
        totalPosition = applyBoneTransform(vec4(aPos, 1.0f));
        Normal = normalize(applyBoneTransform(vec4(aNormal, 0.0))).xyz;
    }
    else
    {
        totalPosition = vec4(aPos, 1.0f);
        Normal = mat3(transpose(inverse(model))) * aNormal; // Transform normals to world space
    }

    vec4 worldPos = model * totalPosition;
    FragPos = worldPos.xyz;
    TexCoords = aTexCoords;

    gl_Position = projection * view * worldPos;
}