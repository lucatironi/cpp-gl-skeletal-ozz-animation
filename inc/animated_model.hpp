#pragma once

#include "mesh.hpp"
#include "shader.hpp"

#include "ozz/animation/runtime/animation.h"
#include "ozz/animation/runtime/skeleton.h"
#include "ozz/animation/runtime/sampling_job.h"
#include "ozz/animation/runtime/local_to_model_job.h"
#include "ozz/base/maths/soa_transform.h"
#include "ozz/base/memory/unique_ptr.h"

#include <iostream>
#include <map>
#include <string>
#include <vector>

struct Joint
{
    std::string name;
    int parentIndex;
    ozz::math::Transform localTransform;
    glm::mat4 invBindPose;
};

static inline glm::mat4 OzzToGlmMat4(const ozz::math::Float4x4& from) {
    glm::mat4 to;
    memcpy(glm::value_ptr(to), &from.cols[0], sizeof(glm::mat4));
    return to;
}

class AnimatedModel
{
public:
    AnimatedModel() : numJoints(0), currentAnimation(0), animationTime(0.0f) {}
    ~AnimatedModel() = default;

    void Draw(const Shader& shader) const
    {
        for (const auto& mesh : meshes)
            mesh.Draw(shader);
    }

    void SetJoints(std::vector<Joint>& j) { joints = j; }

    void SetSkeleton(ozz::unique_ptr<ozz::animation::Skeleton> skel)
    {
        skeleton = std::move(skel);
        numJoints = skeleton->num_joints();
        jointMatrices.resize(numJoints);
    }

    void AddAnimation(ozz::unique_ptr<ozz::animation::Animation> animation)
    {
        animationsMap[std::string(animation->name())] = animations.size();
        animations.emplace_back(std::move(animation));
    }

    void AddMesh(const Mesh& mesh)
    {
        meshes.emplace_back(mesh);
    }

    void SampleAnimation(float deltaTime, const ozz::animation::Animation& animation, ozz::animation::Skeleton& skeleton)
    {
        animationTime += deltaTime; // Advance animation time
        // Wrap around animation time if it exceeds duration
        if (animationTime > animation.duration())
            animationTime = fmod(animationTime, animation.duration());

        // Step 1: Sample animation
        std::vector<ozz::math::SoaTransform> localTransforms(numJoints);
        ozz::animation::SamplingJob samplingJob;
        samplingJob.animation = &animation;
        samplingJob.context = &context;
        samplingJob.ratio = animationTime / animation.duration();
        samplingJob.output = ozz::make_span(localTransforms);

        if (!samplingJob.Run())
        {
            std::cerr << "Failed to sample animation" << std::endl;
            return;
        }

        // Step 2: Convert to model space (world transform)
        std::vector<ozz::math::Float4x4> modelSpaceTransforms(numJoints);

        ozz::animation::LocalToModelJob localToModelJob;
        localToModelJob.skeleton = &skeleton;
        localToModelJob.input = ozz::make_span(localTransforms);
        localToModelJob.output = ozz::make_span(modelSpaceTransforms);

        if (!localToModelJob.Run())
        {
            std::cerr << "Failed to convert local to model transforms" << std::endl;
            std::fill(jointMatrices.begin(), jointMatrices.end(), glm::mat4(1.0f)); // Reset to identity
            return;
        }

        // Step 3: Convert to glm::mat4 for GPU
        for (size_t i = 0; i < modelSpaceTransforms.size(); ++i)
            jointMatrices[i] = OzzToGlmMat4(modelSpaceTransforms[i]) * joints[i].invBindPose;
    }

    void UpdateAnimation(float deltaTime)
    {
        if (animations.empty() || !skeleton) return;
        if (currentAnimation >= animations.size()) return; // Prevent out-of-bounds access
        SampleAnimation(deltaTime, *animations[currentAnimation], *skeleton);
    }

    void SetBoneTransformations(const Shader& shader)
    {
        shader.Use();
        shader.SetBool("animated", HasAnimations());
        if (HasAnimations())
            shader.SetMat4v("finalBonesMatrices", jointMatrices);
    }

    void SetCurrentAnimation(const std::string& animName)
    {
        auto it = animationsMap.find(animName);
        if (it != animationsMap.end())
        {
            currentAnimation = it->second;
            context.Resize(animations[currentAnimation]->num_tracks());
        }
    }

    void SetCurrentAnimation(const unsigned int index)
    {
        if (index < animations.size())
        {
            currentAnimation = index;
            context.Resize(animations[currentAnimation]->num_tracks());
        }
    }

    const bool HasAnimations() { return !animations.empty(); }
    const unsigned int GetNumAnimations() { return animations.size(); }
    std::map<std::string, unsigned int>& GetAnimationList() { return animationsMap; }

    void Debug()
    {
        std::cout << "Animated Model: "
            << ", hasAnimations: " << (HasAnimations() ? "yes" : "no")
            << ", numAnimations: " << GetNumAnimations()
            << ", bonesCount: " << numJoints
            << ", meshes: " << meshes.size()
            << std::endl;

        std::cout << "Meshes:" << std::endl;
        for (const auto& mesh : meshes)
            mesh.Debug();

        for (const auto& [name, index] : animationsMap)
            std::cout << "Animation: " << name
                << ", Index: " << index
                << ", Duration: " << animations[index]->duration()
                << std::endl;
    }

private:
    std::vector<Mesh> meshes;
    ozz::unique_ptr<ozz::animation::Skeleton> skeleton;
    std::vector<Joint> joints;
    unsigned int numJoints;
    std::vector<ozz::unique_ptr<ozz::animation::Animation>> animations;
    std::map<std::string, unsigned int> animationsMap;
    ozz::animation::SamplingJob::Context context;
    unsigned int currentAnimation;
    float animationTime;
    std::vector<glm::mat4> jointMatrices;
};