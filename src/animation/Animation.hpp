#pragma once

#include <glm/mat4x4.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace reflex::animation {

enum class Interpolation { Step, Linear, CubicSpline };

struct Transform {
    glm::vec3 translation{0.0F};
    glm::quat rotation{1.0F, 0.0F, 0.0F, 0.0F};
    glm::vec3 scale{1.0F};
    [[nodiscard]] glm::mat4 matrix() const noexcept;
};

template<typename T>
struct Track {
    std::vector<float> times;
    std::vector<T> values;
    Interpolation interpolation{Interpolation::Linear};
};

struct JointTracks {
    std::optional<Track<glm::vec3>> translation;
    std::optional<Track<glm::quat>> rotation;
    std::optional<Track<glm::vec3>> scale;
};

struct AnimationEvent {
    float time{0.0F};
    std::string name;
};

struct AnimationClip {
    std::string name;
    float duration{0.0F};
    std::vector<JointTracks> joints;
    std::vector<AnimationEvent> events;
    [[nodiscard]] bool validate(std::string& error) const;
};

struct SkeletonJoint {
    std::string name;
    int parent{-1};
    int gltfNode{-1};
    Transform bindLocal;
    glm::mat4 inverseBind{1.0F};
};

class Skeleton {
public:
    std::vector<SkeletonJoint> joints;
    [[nodiscard]] bool validate(std::string& error, std::size_t maximumJoints = 128) const;
    void evaluate(const std::vector<Transform>& localPose, const glm::mat4& meshWorld,
                  std::vector<glm::mat4>& jointWorld,
                  std::vector<glm::mat4>& skinMatrices) const;
};

[[nodiscard]] float normalizeTime(float time, float duration, bool looping) noexcept;
[[nodiscard]] glm::vec3 sample(const Track<glm::vec3>& track, float time,
                               const glm::vec3& fallback) noexcept;
[[nodiscard]] glm::quat sample(const Track<glm::quat>& track, float time,
                               const glm::quat& fallback) noexcept;
void samplePose(const Skeleton& skeleton, const AnimationClip& clip, float time,
                std::vector<Transform>& pose);
void blendPoses(const std::vector<Transform>& from, const std::vector<Transform>& to,
                float factor, std::vector<Transform>& output);

class Animator {
public:
    void play(const AnimationClip* clip, bool looping, float playbackSpeed = 1.0F);
    void update(float deltaTime, std::vector<std::string>& deliveredEvents);
    [[nodiscard]] float time() const noexcept { return time_; }
    [[nodiscard]] float normalizedTime() const noexcept;
    [[nodiscard]] bool completed() const noexcept { return completed_; }
    [[nodiscard]] const AnimationClip* clip() const noexcept { return clip_; }
private:
    const AnimationClip* clip_{nullptr};
    float time_{0.0F};
    float playbackSpeed_{1.0F};
    bool looping_{true};
    bool completed_{false};
};

struct AnimationState {
    std::string name;
    std::string clip;
    bool looping{true};
    float playbackSpeed{1.0F};
};

struct AnimationTransition {
    std::string source;
    std::string destination;
    float blendDuration{0.1F};
};

class AnimationStateMachine {
public:
    [[nodiscard]] bool configure(std::vector<AnimationState> states,
                                 std::vector<AnimationTransition> transitions,
                                 std::string_view initial, std::string& error);
    [[nodiscard]] bool transition(std::string_view destination);
    void update(float deltaTime) noexcept;
    [[nodiscard]] std::string_view state() const noexcept;
    [[nodiscard]] std::string_view previousState() const noexcept;
    [[nodiscard]] float blendFactor() const noexcept;
private:
    std::vector<AnimationState> states_;
    std::vector<AnimationTransition> transitions_;
    std::unordered_map<std::string, std::size_t> lookup_;
    std::size_t current_{0};
    std::size_t previous_{0};
    float blendTime_{0.0F};
    float blendDuration_{0.0F};
};

} // namespace reflex::animation
