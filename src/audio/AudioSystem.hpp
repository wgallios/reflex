#pragma once

#include <glm/vec3.hpp>

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace reflex::audio {

struct SoundDefinition {
    std::string id;
    std::filesystem::path file;
    bool spatial{true};
    bool looping{false};
    bool music{false};
    float volume{1.0F};
    float minimumDistance{1.0F};
    float maximumDistance{25.0F};
    int maximumInstances{4};
};

[[nodiscard]] bool loadAudioManifest(const std::filesystem::path& path,
                                     std::vector<SoundDefinition>& definitions,
                                     std::string& error);
[[nodiscard]] bool canPlay(const SoundDefinition& definition, int activeInstances) noexcept;

struct AudioStatistics {
    std::size_t activeVoices{0};
    std::size_t loopingVoices{0};
    std::size_t missingAssets{0};
    std::size_t concurrencyRejections{0};
};

class AudioSystem {
public:
    AudioSystem();
    ~AudioSystem();
    AudioSystem(const AudioSystem&) = delete;
    AudioSystem& operator=(const AudioSystem&) = delete;
    AudioSystem(AudioSystem&&) noexcept;
    AudioSystem& operator=(AudioSystem&&) noexcept;

    [[nodiscard]] bool initialize(const std::filesystem::path& manifest, std::string& error);
    [[nodiscard]] bool play(std::string_view id, const glm::vec3& position = {});
    [[nodiscard]] bool playMusic(std::string_view id);
    void setListener(const glm::vec3& position, const glm::vec3& forward,
                     const glm::vec3& up) noexcept;
    void setMasterVolume(float volume) noexcept;
    void update();
    void stopAll() noexcept;
    void shutdown() noexcept;
    [[nodiscard]] bool available() const noexcept;
    [[nodiscard]] const AudioStatistics& statistics() const noexcept { return statistics_; }
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::unordered_map<std::string, SoundDefinition> definitions_;
    AudioStatistics statistics_;
};

} // namespace reflex::audio
