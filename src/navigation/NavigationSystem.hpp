#pragma once

#include "collision/Triangle.hpp"

#include <glm/vec3.hpp>

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

class dtNavMesh;
class dtNavMeshQuery;

namespace reflex::navigation {

struct NavigationBuildSettings {
    float cellSize{0.25F};
    float cellHeight{0.15F};
    float agentHeight{1.8F};
    float agentRadius{0.4F};
    float agentMaximumClimb{0.35F};
    float agentMaximumSlopeDegrees{45.0F};
    float regionMinimumSize{8.0F};
    float regionMergeSize{20.0F};
    float edgeMaximumLength{12.0F};
    float edgeMaximumError{1.3F};
    int verticesPerPolygon{6};
    float detailSampleDistance{6.0F};
    float detailSampleMaximumError{1.0F};
    [[nodiscard]] bool validate(std::string& error) const;
    [[nodiscard]] std::uint64_t checksum() const noexcept;
};

enum class PathStatus { Complete, Partial, StartOutsideMesh, TargetOutsideMesh, NoPath, Invalid };

struct NavigationPath {
    PathStatus status{PathStatus::Invalid};
    std::vector<glm::vec3> points;
    [[nodiscard]] bool succeeded() const noexcept { return status == PathStatus::Complete; }
};

struct NavigationStatistics {
    std::size_t polygonCount{0};
    std::size_t queryCount{0};
    std::size_t failedQueries{0};
    std::size_t partialQueries{0};
    double buildMilliseconds{0.0};
    double queryMilliseconds{0.0};
};

enum class OffMeshLinkType { Door, Drop };
struct OffMeshLink {
    std::uint32_t id{0};
    glm::vec3 start{};
    glm::vec3 end{};
    float radius{0.5F};
    OffMeshLinkType type{OffMeshLinkType::Drop};
    bool bidirectional{false};
    bool enabled{true};
};

struct NavigationAgentState {
    glm::vec3 destination{};
    std::vector<glm::vec3> path;
    std::size_t nextPoint{0};
    float pathAge{0.0F};
    float repathDelay{0.5F};
    float stuckTime{0.0F};
    glm::vec3 previousPosition{};
    [[nodiscard]] bool shouldRepath(const glm::vec3& position, const glm::vec3& target,
                                    float deltaTime, float targetThreshold = 0.75F);
};

class NavigationSystem {
public:
    NavigationSystem();
    ~NavigationSystem();
    NavigationSystem(const NavigationSystem&) = delete;
    NavigationSystem& operator=(const NavigationSystem&) = delete;
    NavigationSystem(NavigationSystem&&) noexcept;
    NavigationSystem& operator=(NavigationSystem&&) noexcept;

    [[nodiscard]] bool build(const std::vector<Triangle>& triangles,
                             const NavigationBuildSettings& settings,
                             std::string& error,
                             std::span<const OffMeshLink> links = {});
    [[nodiscard]] NavigationPath findPath(const glm::vec3& start,
                                          const glm::vec3& destination);
    void clear() noexcept;
    [[nodiscard]] bool ready() const noexcept;
    [[nodiscard]] const NavigationStatistics& statistics() const noexcept { return statistics_; }
    [[nodiscard]] const NavigationBuildSettings& settings() const noexcept { return settings_; }
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    NavigationBuildSettings settings_;
    NavigationStatistics statistics_;
};

} // namespace reflex::navigation
