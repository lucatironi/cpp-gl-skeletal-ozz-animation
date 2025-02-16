#pragma once

#include "animated_model.hpp"

#include "ozz/animation/offline/raw_animation.h"
#include "ozz/animation/offline/raw_skeleton.h"
#include "ozz/animation/offline/animation_builder.h"
#include "ozz/animation/offline/skeleton_builder.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/matrix4x4.h>

#include <iostream>
#include <functional>
#include <map>
#include <string>
#include <vector>

class ModelLoader
{
public:
    // Get the instance of the singleton
    static ModelLoader& GetInstance()
    {
        static ModelLoader instance;  // Guaranteed to be destroyed, and instantiated on first use
        return instance;
    }

    static inline glm::mat4 AiToGlmMat4(const aiMatrix4x4& from)
    {
        glm::mat4 to;
        memcpy(glm::value_ptr(to), &from, sizeof(glm::mat4));
        return glm::transpose(to);
    }
    static inline glm::vec3 AiToGlmVec3(const aiVector3D& vec) { return glm::vec3(vec.x, vec.y, vec.z); }
    static inline glm::quat AiToGlmQuat(const aiQuaternion& qat) { return glm::quat(qat.w, qat.x, qat.y, qat.z); }

    static inline ozz::math::Transform AiToOzzTransform(const aiMatrix4x4& aiTransform)
    {
        aiVector3D scale, translation;
        aiQuaternion rotation;
        aiTransform.Decompose(scale, rotation, translation);

        ozz::math::Transform ozzTransform = {
            ozz::math::Float3(translation.x, translation.y, translation.z),
            ozz::math::Quaternion(rotation.x, rotation.y, rotation.z, rotation.w),
            ozz::math::Float3(scale.x, scale.y, scale.z)
        };
        return ozzTransform;
    }

    bool LoadFromFile(const std::string& path, AnimatedModel& model)
    {
        Assimp::Importer importer;
        const aiScene* pScene = importer.ReadFile(path,
            aiProcessPreset_TargetRealtime_Fast | aiProcess_LimitBoneWeights | aiProcess_FlipUVs);

        if (!pScene || (pScene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !pScene->mRootNode)
            throw std::runtime_error("ERROR::ASSIMP: " + std::string(importer.GetErrorString()));

        directory = path.substr(0, path.find_last_of("/"));

        std::vector<Joint> joints;
        std::map<std::string, int> boneMap;

        // Extract skeleton from gltfModel and set model.skeleton
        if(!ExtractSkeleton(pScene, joints, boneMap, model))
        {
            std::cerr << "Error extracting skeleton from model \"" << path << "\"" << std::endl;
            return false;
        }
        // Extract animations from gltfModel and populate model.animations
        if(!ExtractAnimations(pScene, joints, boneMap, model))
        {
            std::cerr << "Error extracting animations from model \"" << path << "\"" << std::endl;
            return false;
        }
        // Extract meshes from gltfModel and populate model.meshes
        if(!ExtractMeshes(pScene, joints, boneMap, model))
        {
            std::cerr << "Error extracting meshes from model \"" << path << "\"" << std::endl;
            return false;
        }

        importer.FreeScene();

        return true;
    }

private:
    unsigned int MAX_BONE_INFLUENCE = 4;
    std::string directory;
    std::vector<Texture> cachedTextures;

    // Private constructor to prevent external instantiation
    ModelLoader() {}
    // Delete copy constructor and assignment operator to prevent copying
    ModelLoader(const ModelLoader&) = delete;
    ModelLoader& operator=(const ModelLoader&) = delete;

    bool ExtractSkeleton(const aiScene* pScene, std::vector<Joint>& joints, std::map<std::string, int>& boneMap, AnimatedModel& model)
    {
        // Extract joints from gltfModel and populate model.joints
        ExtractJoints(pScene->mRootNode, -1, joints, boneMap);

        if(joints.empty())
        {
            std::cerr << "Failed to extract joints" << std::endl;
            return false;
        }

        ozz::animation::offline::RawSkeleton rawSkeleton;
        rawSkeleton.roots.resize(1); // Root joint

        std::function<void(int, ozz::animation::offline::RawSkeleton::Joint&)> buildHierarchy =
            [&](int jointIndex, ozz::animation::offline::RawSkeleton::Joint& outJoint)
            {
                const Joint& joint = joints[jointIndex];
                outJoint.name = joint.name;
                outJoint.transform = joint.localTransform;

                // Add children
                for (size_t i = 0; i < joints.size(); ++i)
                {
                    if (joints[i].parentIndex == jointIndex)
                    {
                        outJoint.children.emplace_back();
                        buildHierarchy(i, outJoint.children.back());
                    }
                }
            };

        // Build hierarchy starting from root joints
        for (size_t i = 0; i < joints.size(); ++i)
            if (joints[i].parentIndex == -1) // Root joints
                buildHierarchy(i, rawSkeleton.roots.back());

        if (!rawSkeleton.Validate())
        {
            std::cerr <<  "Failed to validate Ozz Skeleton" << std::endl;
            return false;
        }

        ozz::animation::offline::SkeletonBuilder skelBuilder;
        model.SetSkeleton(std::move(skelBuilder(rawSkeleton)));

        return true;
    }

    bool ExtractAnimations(const aiScene* scene, std::vector<Joint>& joints, const std::map<std::string, int>& boneMap, AnimatedModel& model)
    {
        if (!scene->HasAnimations())
        {
            std::cerr << "No animations found in this model" << std::endl;
            return false;
        }

        for (unsigned int animIndex = 0; animIndex < scene->mNumAnimations; ++animIndex)
        {
            aiAnimation* aiAnim = scene->mAnimations[animIndex];
            ozz::animation::offline::RawAnimation rawAnimation;
            rawAnimation.duration = static_cast<float>(aiAnim->mDuration / aiAnim->mTicksPerSecond);
            rawAnimation.tracks.resize(boneMap.size());
            rawAnimation.name = aiAnim->mName.C_Str();

            for (unsigned int channelIndex = 0; channelIndex < aiAnim->mNumChannels; ++channelIndex)
            {
                aiNodeAnim* channel = aiAnim->mChannels[channelIndex];
                auto it = boneMap.find(channel->mNodeName.C_Str());

                if (it == boneMap.end())
                    continue; // Skip non-skeletal nodes

                int jointIndex = it->second;
                auto& track = rawAnimation.tracks[jointIndex];

                track.translations.reserve(channel->mNumPositionKeys);
                track.rotations.reserve(channel->mNumRotationKeys);
                track.scales.reserve(channel->mNumScalingKeys);

                // Translation Keys
                for (unsigned int i = 0; i < channel->mNumPositionKeys; ++i)
                {
                    const aiVectorKey& key = channel->mPositionKeys[i];
                    track.translations.push_back({
                        static_cast<float>(key.mTime / aiAnim->mTicksPerSecond),
                        ozz::math::Float3(key.mValue.x, key.mValue.y, key.mValue.z)
                    });
                }

                // Rotation Keys
                for (unsigned int i = 0; i < channel->mNumRotationKeys; ++i)
                {
                    const aiQuatKey& key = channel->mRotationKeys[i];
                    track.rotations.push_back({
                        static_cast<float>(key.mTime / aiAnim->mTicksPerSecond),
                        ozz::math::Quaternion(key.mValue.x, key.mValue.y, key.mValue.z, key.mValue.w)
                    });
                }

                // Scale Keys
                for (unsigned int i = 0; i < channel->mNumScalingKeys; ++i)
                {
                    const aiVectorKey& key = channel->mScalingKeys[i];
                    track.scales.push_back({
                        static_cast<float>(key.mTime / aiAnim->mTicksPerSecond),
                        ozz::math::Float3(key.mValue.x, key.mValue.y, key.mValue.z)
                    });
                }
            }

            if (!rawAnimation.Validate())
            {
                std::cerr << "Failed to validate Ozz Animation: " << aiAnim->mName.C_Str() << std::endl;
                continue;
            }

            ozz::animation::offline::AnimationBuilder animBuilder;
            model.AddAnimation(std::move(animBuilder(rawAnimation)));
        }

        return true;
    }

    bool ExtractMeshes(const aiScene* scene, std::vector<Joint>& joints, std::map<std::string, int>& boneMap, AnimatedModel& model)
    {
        for (unsigned int i = 0; i < scene->mNumMeshes; ++i)
        {
            const aiMesh* mesh = scene->mMeshes[i];

            std::vector<Vertex> vertices;
            std::vector<GLuint> indices;
            std::vector<Texture> textures;

            // Process vertices
            for (unsigned int i = 0; i < mesh->mNumVertices; ++i)
            {
                vertices.emplace_back(Vertex{
                    .Position    = AiToGlmVec3(mesh->mVertices[i]),
                    .Normal      = AiToGlmVec3(mesh->mNormals[i]),
                    .TexCoords   = mesh->mTextureCoords[0]
                                    ? glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y)
                                    : glm::vec2(0.0f, 0.0f),
                    .BoneIDs     = glm::ivec4(-1),
                    .BoneWeights = glm::vec4(0.0f) // Bone Weights are initialised in another loop
                });
            }

            // Process bones
            for (unsigned int i = 0; i < mesh->mNumBones; ++i)
            {
                aiBone* bone = mesh->mBones[i];

                int boneID = 0;
                std::string boneName(bone->mName.data);
                if (boneMap.find(boneName) != boneMap.end())
                    boneID = boneMap[boneName];
                else
                {
                    std::cerr << "Found missing joint \"" << boneName << std::endl;
                    return false;
                }

                joints[boneID].invBindPose = AiToGlmMat4(bone->mOffsetMatrix);

                for (unsigned int j = 0; j < bone->mNumWeights; ++j)
                {
                    unsigned int vertexID = bone->mWeights[j].mVertexId;
                    float weight = bone->mWeights[j].mWeight;

                    for (unsigned int g = 0; g < MAX_BONE_INFLUENCE; ++g)
                    {
                        if (vertices[vertexID].BoneWeights[g] == 0.0f)
                        {
                            vertices[vertexID].BoneIDs[g] = boneID;
                            vertices[vertexID].BoneWeights[g] = weight;
                            break;
                        }
                    }
                }
            }

            // Process indices
            for (unsigned int i = 0; i < mesh->mNumFaces; ++i)
            {
                const aiFace& face = mesh->mFaces[i];
                for (unsigned int j = 0; j < face.mNumIndices; ++j)
                    indices.emplace_back(face.mIndices[j]);
            }

            // Process textures
            if (mesh->mMaterialIndex >= 0)
            {
                aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];

                auto diffuseTextures = ExtractTextures(scene, material, aiTextureType_DIFFUSE, "texture_diffuse");
                textures.insert(textures.end(), diffuseTextures.begin(), diffuseTextures.end());
                auto specularTextures = ExtractTextures(scene, material, aiTextureType_SPECULAR, "texture_specular");
                textures.insert(textures.end(), specularTextures.begin(), specularTextures.end());
                auto normalTextures = ExtractTextures(scene, material, aiTextureType_HEIGHT, "texture_normal");
                textures.insert(textures.end(), normalTextures.begin(), normalTextures.end());
            }

            model.AddMesh({ std::move(vertices), std::move(indices), std::move(textures) });
            model.SetJoints(joints);
        }

        return true;
    }

    void ExtractJoints(const aiNode* node, int parentIndex, std::vector<Joint>& joints, std::map<std::string, int>& boneMap)
    {
        int jointIndex = 0;
        std::string boneName(node->mName.data);
        if (boneMap.find(boneName) == boneMap.end())
        {
            jointIndex = static_cast<int>(boneMap.size());
            boneMap[boneName] = jointIndex;
        }
        else
            jointIndex = boneMap[boneName];

        // Store joint
        joints.push_back({
            .name           = boneName,
            .parentIndex    = parentIndex,
            .localTransform = AiToOzzTransform(node->mTransformation),
            .invBindPose    = glm::mat4(1.0f)
        });

        // Recursively process child nodes
        for (unsigned int i = 0; i < node->mNumChildren; ++i)
            ExtractJoints(node->mChildren[i], jointIndex, joints, boneMap);
    }

    std::vector<Texture> ExtractTextures(const aiScene* scene, aiMaterial* material, aiTextureType textureType, const std::string& typeName)
    {
        std::vector<Texture> textures;
        for (unsigned int i = 0; i < material->GetTextureCount(textureType); ++i)
        {
            aiString textureFilename;
            material->GetTexture(textureType, i, &textureFilename);

            // check if texture was loaded before and if so, continue to next iteration: skip loading a new texture
            bool skip = false;
            for (unsigned int j = 0; j < cachedTextures.size(); ++j)
            {
                if (std::strcmp(cachedTextures[j].path.data(), textureFilename.C_Str()) == 0)
                {
                    textures.emplace_back(cachedTextures[j]);
                    skip = true; // a texture with the same filepath has already been loaded, continue to next one. (optimization)
                    break;
                }
            }

            if (!skip) // if texture hasn't been loaded already, load it
            {
                Texture2D texture2D;
                if (const auto& texture = scene->GetEmbeddedTexture(textureFilename.C_Str()))
                    texture2D = Texture2D(reinterpret_cast<unsigned char*>(texture->pcData), texture->mWidth, texture->mHeight);
                else
                    texture2D = Texture2D(std::string(directory + "/" + std::string(textureFilename.C_Str())));

                Texture tex = { texture2D, typeName, std::string(textureFilename.C_Str()) };
                textures.push_back(tex);
                cachedTextures.push_back(tex);
            }
        }
        return textures;
    }
};