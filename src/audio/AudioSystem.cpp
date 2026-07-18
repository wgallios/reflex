#include "audio/AudioSystem.hpp"

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <unordered_set>
#include <utility>

namespace reflex::audio {
namespace {
using json = nlohmann::json;
}

struct AudioSystem::Impl {
    struct Voice {
        std::unique_ptr<ma_sound> sound;
        std::string definition;
        bool looping{false};
    };
    ma_engine engine{};
    bool initialized{false};
    std::vector<Voice> voices;
};

bool loadAudioManifest(const std::filesystem::path& path,
                       std::vector<SoundDefinition>& definitions, std::string& error) {
    std::ifstream stream{path};
    if (!stream) { error = "audio manifest not found: " + path.string(); return false; }
    json root;
    try { stream >> root; } catch (const json::exception& exception) {
        error = std::string{"invalid audio JSON: "} + exception.what(); return false;
    }
    if (!root.is_object() || root.value("format_version", 0) != 1 ||
        !root.contains("sounds") || !root["sounds"].is_array()) {
        error = "audio manifest requires format_version 1 and a sounds array"; return false;
    }
    std::vector<SoundDefinition> parsed;
    std::unordered_set<std::string> ids;
    try {
        for (const json& value : root["sounds"]) {
            SoundDefinition definition;
            definition.id = value.value("id", std::string{});
            definition.file = value.value("file", std::string{});
            definition.spatial = value.value("spatial", true);
            definition.looping = value.value("looping", false);
            definition.music = value.value("music", false);
            definition.volume = value.value("volume", 1.0F);
            definition.minimumDistance = value.value("minimum_distance", 1.0F);
            definition.maximumDistance = value.value("maximum_distance", 25.0F);
            definition.maximumInstances = value.value("maximum_instances", 4);
            if (definition.id.empty() || definition.file.empty() || !ids.insert(definition.id).second ||
                !std::isfinite(definition.volume) || definition.volume < 0.0F ||
                !std::isfinite(definition.minimumDistance) || definition.minimumDistance < 0.0F ||
                !std::isfinite(definition.maximumDistance) ||
                definition.maximumDistance <= definition.minimumDistance || definition.maximumInstances <= 0) {
                error = "audio definitions require unique IDs, files, valid distances, and positive instance limits";
                return false;
            }
            parsed.push_back(std::move(definition));
        }
    } catch (const json::exception& exception) {
        error = std::string{"invalid audio definition: "} + exception.what(); return false;
    }
    definitions = std::move(parsed); return true;
}

bool canPlay(const SoundDefinition& definition, const int activeInstances) noexcept {
    return activeInstances >= 0 && activeInstances < definition.maximumInstances;
}

AudioSystem::AudioSystem() : impl_(std::make_unique<Impl>()) {}
AudioSystem::~AudioSystem() { shutdown(); }
AudioSystem::AudioSystem(AudioSystem&&) noexcept = default;
AudioSystem& AudioSystem::operator=(AudioSystem&&) noexcept = default;

bool AudioSystem::initialize(const std::filesystem::path& manifest, std::string& error) {
    shutdown();
    std::vector<SoundDefinition> definitions;
    if (!loadAudioManifest(manifest, definitions, error)) return false;
    if (ma_engine_init(nullptr, &impl_->engine) != MA_SUCCESS) {
        error = "audio device unavailable; continuing without sound"; return false;
    }
    impl_->initialized = true;
    for (SoundDefinition& definition : definitions) definitions_.emplace(definition.id, std::move(definition));
    return true;
}

bool AudioSystem::play(const std::string_view id, const glm::vec3& position) {
    if (!available()) return false;
    const auto found = definitions_.find(std::string{id});
    if (found == definitions_.end()) return false;
    const int active = static_cast<int>(std::count_if(impl_->voices.begin(), impl_->voices.end(),
        [&](const Impl::Voice& voice) { return voice.definition == id; }));
    if (!canPlay(found->second, active)) { ++statistics_.concurrencyRejections; return false; }
    if (!std::filesystem::exists(found->second.file)) { ++statistics_.missingAssets; return false; }
    auto sound = std::make_unique<ma_sound>();
    const ma_uint32 flags = found->second.spatial
        ? ma_uint32{0} : static_cast<ma_uint32>(MA_SOUND_FLAG_NO_SPATIALIZATION);
    if (ma_sound_init_from_file(&impl_->engine, found->second.file.string().c_str(), flags,
                                nullptr, nullptr, sound.get()) != MA_SUCCESS) {
        ++statistics_.missingAssets; return false;
    }
    ma_sound_set_volume(sound.get(), found->second.volume);
    ma_sound_set_looping(sound.get(), found->second.looping ? MA_TRUE : MA_FALSE);
    ma_sound_set_position(sound.get(), position.x, position.y, position.z);
    ma_sound_set_min_distance(sound.get(), found->second.minimumDistance);
    ma_sound_set_max_distance(sound.get(), found->second.maximumDistance);
    if (ma_sound_start(sound.get()) != MA_SUCCESS) { ma_sound_uninit(sound.get()); return false; }
    impl_->voices.push_back({std::move(sound), found->first, found->second.looping});
    return true;
}

bool AudioSystem::playMusic(const std::string_view id) {
    const auto found = definitions_.find(std::string{id});
    return found != definitions_.end() && found->second.music && play(id);
}

void AudioSystem::setListener(const glm::vec3& position, const glm::vec3& forward,
                              const glm::vec3& up) noexcept {
    if (!available()) return;
    ma_engine_listener_set_position(&impl_->engine, 0, position.x, position.y, position.z);
    ma_engine_listener_set_direction(&impl_->engine, 0, forward.x, forward.y, forward.z);
    ma_engine_listener_set_world_up(&impl_->engine, 0, up.x, up.y, up.z);
}

void AudioSystem::setMasterVolume(const float volume) noexcept {
    if (available()) ma_engine_set_volume(&impl_->engine, std::clamp(volume, 0.0F, 1.0F));
}

void AudioSystem::update() {
    if (!available()) return;
    std::erase_if(impl_->voices, [](Impl::Voice& voice) {
        if (voice.looping || ma_sound_is_playing(voice.sound.get())) return false;
        ma_sound_uninit(voice.sound.get()); return true;
    });
    statistics_.activeVoices = impl_->voices.size();
    statistics_.loopingVoices = static_cast<std::size_t>(std::count_if(impl_->voices.begin(), impl_->voices.end(),
        [](const Impl::Voice& voice) { return voice.looping; }));
}

void AudioSystem::stopAll() noexcept {
    if (!impl_) return;
    for (Impl::Voice& voice : impl_->voices) { ma_sound_stop(voice.sound.get()); ma_sound_uninit(voice.sound.get()); }
    impl_->voices.clear(); statistics_.activeVoices = statistics_.loopingVoices = 0;
}

void AudioSystem::shutdown() noexcept {
    if (!impl_) impl_ = std::make_unique<Impl>();
    stopAll();
    if (impl_->initialized) { ma_engine_uninit(&impl_->engine); impl_->initialized = false; }
    definitions_.clear();
}
bool AudioSystem::available() const noexcept { return impl_ && impl_->initialized; }

} // namespace reflex::audio
