#include "animation/Animation.hpp"

#include <glm/common.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>

namespace reflex::animation {
namespace {
template<typename T>
bool validTrack(const Track<T>& track) {
    if (track.times.empty() || track.times.size() != track.values.size()) return false;
    float previous = -std::numeric_limits<float>::infinity();
    for (const float time : track.times) {
        if (!std::isfinite(time) || time < previous) return false;
        previous = time;
    }
    return true;
}

template<typename T, typename Blend>
T sampleTrack(const Track<T>& track, const float time, const T& fallback, Blend blend) {
    if (!validTrack(track)) return fallback;
    const auto upper = std::upper_bound(track.times.begin(), track.times.end(), time);
    if (upper == track.times.begin()) return track.values.front();
    if (upper == track.times.end()) return track.values.back();
    const std::size_t right = static_cast<std::size_t>(upper - track.times.begin());
    const std::size_t left = right - 1;
    if (track.interpolation == Interpolation::Step) return track.values[left];
    const float span = track.times[right] - track.times[left];
    const float factor = span > 1.0e-6F ? glm::clamp((time - track.times[left]) / span, 0.0F, 1.0F) : 0.0F;
    return blend(track.values[left], track.values[right], factor);
}
} // namespace

glm::mat4 Transform::matrix() const noexcept {
    return glm::translate(glm::mat4{1.0F}, translation) * glm::mat4_cast(rotation) *
           glm::scale(glm::mat4{1.0F}, scale);
}

bool AnimationClip::validate(std::string& error) const {
    if (name.empty()) { error = "animation clip has no name"; return false; }
    if (!std::isfinite(duration) || duration <= 0.0F) {
        error = "animation clip '" + name + "' has invalid duration"; return false;
    }
    for (const JointTracks& joint : joints) {
        if (joint.translation && !validTrack(*joint.translation)) { error = "invalid translation track"; return false; }
        if (joint.rotation && !validTrack(*joint.rotation)) { error = "invalid rotation track"; return false; }
        if (joint.scale && !validTrack(*joint.scale)) { error = "invalid scale track"; return false; }
    }
    float previous = -1.0F;
    for (const AnimationEvent& event : events) {
        if (event.name.empty() || event.time < 0.0F || event.time > duration || event.time < previous) {
            error = "animation clip '" + name + "' has invalid or unsorted events"; return false;
        }
        previous = event.time;
    }
    return true;
}

bool Skeleton::validate(std::string& error, const std::size_t maximumJoints) const {
    if (joints.empty()) { error = "skeleton has no joints"; return false; }
    if (joints.size() > maximumJoints) { error = "skeleton exceeds joint limit"; return false; }
    std::unordered_set<int> nodes;
    for (std::size_t i = 0; i < joints.size(); ++i) {
        const SkeletonJoint& joint = joints[i];
        if (joint.parent >= static_cast<int>(i) || joint.parent < -1) {
            error = "skeleton parents must precede their children"; return false;
        }
        if (joint.gltfNode < 0 || !nodes.insert(joint.gltfNode).second) {
            error = "skeleton contains an invalid or duplicate glTF node mapping"; return false;
        }
    }
    return true;
}

void Skeleton::evaluate(const std::vector<Transform>& localPose, const glm::mat4& meshWorld,
                        std::vector<glm::mat4>& jointWorld,
                        std::vector<glm::mat4>& skinMatrices) const {
    jointWorld.resize(joints.size());
    skinMatrices.resize(joints.size());
    const glm::mat4 inverseMeshWorld = glm::inverse(meshWorld);
    for (std::size_t i = 0; i < joints.size(); ++i) {
        const glm::mat4 local = (i < localPose.size() ? localPose[i] : joints[i].bindLocal).matrix();
        jointWorld[i] = joints[i].parent >= 0
            ? jointWorld[static_cast<std::size_t>(joints[i].parent)] * local
            : meshWorld * local;
        skinMatrices[i] = inverseMeshWorld * jointWorld[i] * joints[i].inverseBind;
    }
}

float normalizeTime(const float time, const float duration, const bool looping) noexcept {
    if (!(duration > 0.0F) || !std::isfinite(time)) return 0.0F;
    if (!looping) return glm::clamp(time, 0.0F, duration);
    const float wrapped = std::fmod(time, duration);
    return wrapped < 0.0F ? wrapped + duration : wrapped;
}

glm::vec3 sample(const Track<glm::vec3>& track, const float time,
                 const glm::vec3& fallback) noexcept {
    return sampleTrack(track, time, fallback,
        [](const glm::vec3& a, const glm::vec3& b, const float t) { return glm::mix(a, b, t); });
}

glm::quat sample(const Track<glm::quat>& track, const float time,
                 const glm::quat& fallback) noexcept {
    return glm::normalize(sampleTrack(track, time, fallback,
        [](const glm::quat& a, const glm::quat& b, const float t) { return glm::slerp(a, b, t); }));
}

void samplePose(const Skeleton& skeleton, const AnimationClip& clip, const float time,
                std::vector<Transform>& pose) {
    pose.resize(skeleton.joints.size());
    for (std::size_t i = 0; i < skeleton.joints.size(); ++i) {
        pose[i] = skeleton.joints[i].bindLocal;
        if (i >= clip.joints.size()) continue;
        const JointTracks& tracks = clip.joints[i];
        if (tracks.translation) pose[i].translation = sample(*tracks.translation, time, pose[i].translation);
        if (tracks.rotation) pose[i].rotation = sample(*tracks.rotation, time, pose[i].rotation);
        if (tracks.scale) pose[i].scale = sample(*tracks.scale, time, pose[i].scale);
    }
}

void blendPoses(const std::vector<Transform>& from, const std::vector<Transform>& to,
                const float factor, std::vector<Transform>& output) {
    const std::size_t count = std::min(from.size(), to.size());
    output.resize(count);
    const float t = glm::clamp(factor, 0.0F, 1.0F);
    for (std::size_t i = 0; i < count; ++i) {
        output[i].translation = glm::mix(from[i].translation, to[i].translation, t);
        output[i].rotation = glm::normalize(glm::slerp(from[i].rotation, to[i].rotation, t));
        output[i].scale = glm::mix(from[i].scale, to[i].scale, t);
    }
}

void Animator::play(const AnimationClip* clip, const bool looping, const float playbackSpeed) {
    clip_ = clip; looping_ = looping; playbackSpeed_ = playbackSpeed;
    time_ = 0.0F; completed_ = clip == nullptr;
}

void Animator::update(const float deltaTime, std::vector<std::string>& deliveredEvents) {
    if (clip_ == nullptr || completed_ || deltaTime <= 0.0F || playbackSpeed_ <= 0.0F) return;
    const float previous = time_;
    const float unwrapped = time_ + deltaTime * playbackSpeed_;
    const bool crossedLoop = looping_ && unwrapped >= clip_->duration;
    time_ = normalizeTime(unwrapped, clip_->duration, looping_);
    for (const AnimationEvent& event : clip_->events) {
        const bool crossed = crossedLoop ? (event.time > previous || event.time <= time_)
                                         : (event.time > previous && event.time <= time_);
        if (crossed) deliveredEvents.push_back(event.name);
    }
    if (!looping_ && unwrapped >= clip_->duration) { time_ = clip_->duration; completed_ = true; }
}

float Animator::normalizedTime() const noexcept {
    return clip_ != nullptr && clip_->duration > 0.0F ? time_ / clip_->duration : 0.0F;
}

bool AnimationStateMachine::configure(std::vector<AnimationState> states,
                                      std::vector<AnimationTransition> transitions,
                                      const std::string_view initial, std::string& error) {
    if (states.empty()) { error = "animation state machine has no states"; return false; }
    std::unordered_map<std::string, std::size_t> lookup;
    for (std::size_t i = 0; i < states.size(); ++i) {
        if (states[i].name.empty() || states[i].clip.empty() || !lookup.emplace(states[i].name, i).second) {
            error = "animation states require unique names and clip names"; return false;
        }
    }
    const auto initialIt = lookup.find(std::string{initial});
    if (initialIt == lookup.end()) { error = "initial animation state does not exist"; return false; }
    for (const AnimationTransition& transition : transitions) {
        if (!lookup.contains(transition.source) || !lookup.contains(transition.destination) ||
            !std::isfinite(transition.blendDuration) || transition.blendDuration < 0.0F) {
            error = "animation transition references an unknown state or invalid duration"; return false;
        }
    }
    states_ = std::move(states); transitions_ = std::move(transitions); lookup_ = std::move(lookup);
    current_ = previous_ = initialIt->second; blendTime_ = blendDuration_ = 0.0F;
    return true;
}

bool AnimationStateMachine::transition(const std::string_view destination) {
    const auto target = lookup_.find(std::string{destination});
    if (target == lookup_.end() || target->second == current_) return false;
    const std::string& sourceName = states_[current_].name;
    const auto rule = std::find_if(transitions_.begin(), transitions_.end(), [&](const AnimationTransition& entry) {
        return entry.source == sourceName && entry.destination == destination;
    });
    if (rule == transitions_.end()) return false;
    previous_ = current_; current_ = target->second; blendTime_ = 0.0F; blendDuration_ = rule->blendDuration;
    return true;
}

void AnimationStateMachine::update(const float deltaTime) noexcept { blendTime_ += std::max(0.0F, deltaTime); }
std::string_view AnimationStateMachine::state() const noexcept { return states_.empty() ? std::string_view{} : states_[current_].name; }
std::string_view AnimationStateMachine::previousState() const noexcept { return states_.empty() ? std::string_view{} : states_[previous_].name; }
float AnimationStateMachine::blendFactor() const noexcept { return blendDuration_ <= 0.0F ? 1.0F : glm::clamp(blendTime_ / blendDuration_, 0.0F, 1.0F); }

} // namespace reflex::animation
