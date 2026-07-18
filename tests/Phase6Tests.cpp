#include "animation/Animation.hpp"
#include "assets/AssetManifest.hpp"
#include "audio/AudioSystem.hpp"
#include "campaign/Campaign.hpp"
#include "navigation/NavigationSystem.hpp"
#include "profiling/Profiler.hpp"

#include <glm/geometric.hpp>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_set>

namespace {
int failures = 0;
void expect(const bool condition, const char* message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}

Triangle triangle(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c) {
    Triangle value{a, b, c};
    value.normal = glm::normalize(glm::cross(b - a, c - a));
    value.bounds.expand(a); value.bounds.expand(b); value.bounds.expand(c);
    return value;
}

void animationTests() {
    using namespace reflex::animation;
    Track<glm::vec3> position{{0.0F, 1.0F}, {{0,0,0}, {10,0,0}}, Interpolation::Linear};
    expect(std::abs(sample(position, 0.25F, {}).x - 2.5F) < 0.001F, "linear keyframe sampling");
    Track<glm::quat> rotation{{0.0F, 1.0F},
        {glm::quat{1,0,0,0}, glm::angleAxis(glm::radians(90.0F), glm::vec3{0,1,0})}, Interpolation::Linear};
    expect(std::abs(glm::length(sample(rotation, 0.5F, {})) - 1.0F) < 0.001F,
           "quaternion interpolation remains normalized");
    expect(std::abs(normalizeTime(2.25F, 1.0F, true) - 0.25F) < 0.001F, "loop time normalization");

    AnimationClip clip{"test", 1.0F, {}, {{0.25F, "fire"}, {0.9F, "footstep"}}};
    std::string error;
    expect(clip.validate(error), "valid animation clip");
    Animator animator;
    animator.play(&clip, true);
    std::vector<std::string> events;
    animator.update(0.3F, events);
    expect(events.size() == 1 && events[0] == "fire", "animation event delivery");
    events.clear(); animator.update(0.8F, events);
    expect(events.size() == 1 && events[0] == "footstep", "event delivery across loop boundary");
    animator.play(&clip, false); events.clear(); animator.update(2.0F, events);
    expect(animator.completed() && std::abs(animator.time() - 1.0F) < 0.001F, "one-shot completion");

    AnimationStateMachine machine;
    expect(machine.configure({{"Idle","idle",true,1}, {"Walk","walk",true,1}},
        {{"Idle","Walk",0.2F}, {"Walk","Idle",0.1F}}, "Idle", error), "state machine configuration");
    expect(machine.transition("Walk"), "state transition");
    machine.update(0.1F);
    expect(std::abs(machine.blendFactor() - 0.5F) < 0.001F, "blend factor");
    machine.update(1.0F); expect(machine.blendFactor() == 1.0F, "blend factor clamping");

    Skeleton skeleton;
    skeleton.joints = {{"root", -1, 3, {}, glm::mat4{1}},
                       {"child", 0, 7, Transform{{0,1,0}}, glm::mat4{1}}};
    expect(skeleton.validate(error), "skeleton hierarchy validation");
    std::vector<glm::mat4> worlds, skins;
    skeleton.evaluate({}, glm::mat4{1}, worlds, skins);
    expect(worlds.size() == 2 && std::abs(worlds[1][3].y - 1.0F) < 0.001F,
           "skeleton hierarchy evaluation");
    skeleton.joints[1].parent = 1;
    expect(!skeleton.validate(error), "malformed skin rejection");
}

void navigationTests() {
    using namespace reflex::navigation;
    NavigationBuildSettings settings;
    std::string error;
    expect(settings.validate(error), "default navigation settings");
    settings.agentHeight = 0.0F;
    expect(!settings.validate(error), "invalid navigation settings rejection");
    settings = {};
    std::vector<Triangle> floor{
        triangle({-10,0,-10}, {-10,0,10}, {10,0,10}),
        triangle({-10,0,-10}, {10,0,10}, {10,0,-10})};
    NavigationSystem navigation;
    expect(navigation.build(floor, settings, error), "Recast navigation build");
    const NavigationPath path = navigation.findPath({-5,0,-5}, {5,0,5});
    expect(path.succeeded() && path.points.size() >= 2, "Detour complete path");
    NavigationAgentState agent;
    agent.destination = {0,0,0}; agent.path = {{0,0,0}}; agent.previousPosition = {0,0,0};
    expect(!agent.shouldRepath({0,0,0}, {0,0,0}, 0.1F), "repath timer not premature");
    expect(agent.shouldRepath({0,0,0}, {2,0,0}, 0.1F), "target movement causes repath");
}

void campaignTests() {
    using namespace reflex::campaign;
    std::string error;
    ObjectiveSystem objectives;
    expect(objectives.initialize({{"reach", ObjectiveType::ReachLocation, "Reach the room", "room", 1, true, "clear"},
        {"clear", ObjectiveType::ClearEncounter, "Clear enemies", "fight", 1, false, {}}}, error),
        "objective initialization");
    expect(objectives.complete("reach"), "objective completes once");
    expect(!objectives.complete("reach"), "objective completion idempotency");
    expect(objectives.state("clear")->state == ObjectiveState::Active, "next objective activation");

    EncounterSystem encounters;
    EncounterDefinition encounter;
    encounter.id = "fight"; encounter.waves = {{0.0F, {"wave1"}}, {0.25F, {"wave2"}}};
    expect(encounters.initialize({encounter}, error) && encounters.start("fight"), "encounter start");
    std::vector<std::string> activated, completed;
    encounters.update(0.01F, {}, activated, completed);
    expect(activated == std::vector<std::string>{"wave1"}, "first encounter wave activation");
    encounters.update(0.01F, {}, activated, completed);
    expect(encounters.state("fight")->state == EncounterState::WaitingForNextWave,
           "encounter waits between waves");
    encounters.update(0.25F, {}, activated, completed);
    expect(activated.back() == "wave2", "second encounter wave activation");
    encounters.update(0.01F, {}, activated, completed);
    expect(completed.size() == 1 && completed[0] == "fight", "encounter completes exactly once");
}

void dataAndProfilerTests() {
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "reflex_phase6_tests";
    std::filesystem::create_directories(root);
    const auto audioPath = root / "audio.json";
    { std::ofstream out{audioPath}; out << R"({"format_version":1,"sounds":[{"id":"test","file":"test.wav","maximum_instances":2}]})"; }
    std::vector<reflex::audio::SoundDefinition> sounds;
    std::string error;
    expect(reflex::audio::loadAudioManifest(audioPath, sounds, error), "audio definition parsing");
    expect(reflex::audio::canPlay(sounds[0], 1) && !reflex::audio::canPlay(sounds[0], 2),
           "audio concurrency policy");
    const auto manifestPath = root / "manifest.json";
    { std::ofstream out{manifestPath}; out << R"({"format_version":1,"assets":[{"id":"level","type":"level","path":"level.json"}]})"; }
    reflex::assets::AssetManifest manifest;
    expect(manifest.load(manifestPath, error) && manifest.find("level") != nullptr,
           "asset manifest parsing");
    const auto duplicatePath = root / "duplicate.json";
    { std::ofstream out{duplicatePath}; out << R"({"format_version":1,"assets":[{"id":"x","type":"model","path":"a"},{"id":"x","type":"model","path":"b"}]})"; }
    expect(!manifest.load(duplicatePath, error), "duplicate asset ID rejection");
    reflex::profiling::Profiler profiler;
    profiler.beginFrame(); profiler.record(reflex::profiling::Category::Frame, 16.0); profiler.endFrame();
    expect(std::abs(profiler.framesPerSecond() - 62.5) < 0.01, "profiler rolling frame statistics");
    std::filesystem::remove_all(root);
}
} // namespace

int main() {
    animationTests(); navigationTests(); campaignTests(); dataAndProfilerTests();
    if (failures == 0) std::cout << "All Phase 6 system tests passed.\n";
    return failures == 0 ? 0 : 1;
}
