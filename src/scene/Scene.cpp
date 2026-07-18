#include "scene/Scene.hpp"

#include <algorithm>

void Scene::updateAnimations(const float deltaTime) {
    for (SkinnedScenePrimitive& primitive : skinnedPrimitives) {
        if (primitive.skeleton >= skeletalAssets.size()) continue;
        const SkeletalAsset& asset = skeletalAssets[primitive.skeleton];
        if (asset.clips.empty()) {
            primitive.pose.clear();
        } else {
            primitive.clip = primitive.clip < asset.clips.size() ? primitive.clip : 0;
            const reflex::animation::AnimationClip& clip = asset.clips[primitive.clip];
            primitive.time = reflex::animation::normalizeTime(primitive.time + deltaTime, clip.duration, primitive.looping);
            reflex::animation::samplePose(asset.skeleton, clip, primitive.time, primitive.pose);
            // Gameplay/collision owns world motion; locomotion root translation stays in bind pose.
            if (!primitive.pose.empty()) primitive.pose[0].translation = asset.skeleton.joints[0].bindLocal.translation;
        }
        asset.skeleton.evaluate(primitive.pose, primitive.worldTransform,
                                primitive.jointWorld, primitive.skinMatrices);
    }
}

void Scene::setAnimation(const std::span<const std::size_t> primitives,
                         const std::string_view clipName, const bool looping,
                         const bool restart) {
    for (const std::size_t index : primitives) {
        if (index >= skinnedPrimitives.size()) continue;
        SkinnedScenePrimitive& primitive = skinnedPrimitives[index];
        if (primitive.skeleton >= skeletalAssets.size()) continue;
        const auto& clips = skeletalAssets[primitive.skeleton].clips;
        const auto found = std::find_if(clips.begin(), clips.end(), [&](const auto& clip) {
            return clip.name == clipName;
        });
        if (found == clips.end()) continue;
        const std::size_t selected = static_cast<std::size_t>(found - clips.begin());
        if (restart || primitive.clip != selected || primitive.animationState != clipName) primitive.time = 0.0F;
        primitive.clip = selected; primitive.looping = looping; primitive.animationState = clipName;
    }
}
