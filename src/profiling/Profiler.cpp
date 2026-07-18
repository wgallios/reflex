#include "profiling/Profiler.hpp"

#include <algorithm>

namespace reflex::profiling {

void Profiler::beginFrame() noexcept { current_.fill(0.0); counters_ = {}; }
void Profiler::record(const Category category, const double milliseconds) noexcept {
    current_[static_cast<std::size_t>(category)] += std::max(0.0, milliseconds);
}
void Profiler::endFrame() noexcept {
    constexpr double smoothing = 0.1;
    for (std::size_t i = 0; i < current_.size(); ++i) {
        average_[i] = average_[i] == 0.0 ? current_[i]
                                         : average_[i] + (current_[i] - average_[i]) * smoothing;
    }
}
double Profiler::current(const Category category) const noexcept { return current_[static_cast<std::size_t>(category)]; }
double Profiler::average(const Category category) const noexcept { return average_[static_cast<std::size_t>(category)]; }
double Profiler::framesPerSecond() const noexcept {
    const double frame = average(Category::Frame); return frame > 0.0 ? 1000.0 / frame : 0.0;
}
std::string_view Profiler::name(const Category category) noexcept {
    constexpr std::array names{"frame", "simulation", "rendering", "animation", "skinning",
        "navigation", "enemy AI", "collision", "audio", "particles"};
    return names[static_cast<std::size_t>(category)];
}

ScopedTimer::ScopedTimer(Profiler& profiler, const Category category) noexcept
    : profiler_(profiler), category_(category), start_(std::chrono::steady_clock::now()) {}
ScopedTimer::~ScopedTimer() {
    profiler_.record(category_, std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start_).count());
}

} // namespace reflex::profiling
