#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <string_view>

namespace reflex::profiling {

enum class Category : std::size_t {
    Frame, Simulation, Rendering, Animation, Skinning, Navigation,
    EnemyAi, Collision, Audio, Particles, Count
};

struct FrameCounters {
    std::size_t drawCalls{0};
    std::size_t renderedTriangles{0};
    std::size_t activeEnemies{0};
    std::size_t navigationQueries{0};
    std::size_t animationCount{0};
    std::size_t audioVoices{0};
    std::size_t projectiles{0};
    std::size_t particles{0};
};

class Profiler {
public:
    void beginFrame() noexcept;
    void record(Category category, double milliseconds) noexcept;
    void endFrame() noexcept;
    [[nodiscard]] double current(Category category) const noexcept;
    [[nodiscard]] double average(Category category) const noexcept;
    [[nodiscard]] double framesPerSecond() const noexcept;
    [[nodiscard]] FrameCounters& counters() noexcept { return counters_; }
    [[nodiscard]] const FrameCounters& counters() const noexcept { return counters_; }
    [[nodiscard]] static std::string_view name(Category category) noexcept;
private:
    std::array<double, static_cast<std::size_t>(Category::Count)> current_{};
    std::array<double, static_cast<std::size_t>(Category::Count)> average_{};
    FrameCounters counters_{};
};

class ScopedTimer {
public:
    ScopedTimer(Profiler& profiler, Category category) noexcept;
    ~ScopedTimer();
    ScopedTimer(const ScopedTimer&) = delete;
    ScopedTimer& operator=(const ScopedTimer&) = delete;
private:
    Profiler& profiler_;
    Category category_;
    std::chrono::steady_clock::time_point start_;
};

} // namespace reflex::profiling
