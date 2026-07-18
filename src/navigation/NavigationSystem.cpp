#include "navigation/NavigationSystem.hpp"

#include <DetourNavMesh.h>
#include <DetourNavMeshBuilder.h>
#include <DetourNavMeshQuery.h>
#include <Recast.h>

#include <glm/geometric.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>

namespace reflex::navigation {
namespace {
constexpr unsigned char walkableArea = 1;
constexpr unsigned short walkableFlag = 1;

template<typename T, void (*Free)(T*)>
struct RecastDeleter { void operator()(T* value) const noexcept { if (value != nullptr) Free(value); } };

using Heightfield = std::unique_ptr<rcHeightfield, RecastDeleter<rcHeightfield, rcFreeHeightField>>;
using CompactHeightfield = std::unique_ptr<rcCompactHeightfield, RecastDeleter<rcCompactHeightfield, rcFreeCompactHeightfield>>;
using ContourSet = std::unique_ptr<rcContourSet, RecastDeleter<rcContourSet, rcFreeContourSet>>;
using PolyMesh = std::unique_ptr<rcPolyMesh, RecastDeleter<rcPolyMesh, rcFreePolyMesh>>;
using PolyMeshDetail = std::unique_ptr<rcPolyMeshDetail, RecastDeleter<rcPolyMeshDetail, rcFreePolyMeshDetail>>;

void mixHash(std::uint64_t& hash, const void* data, const std::size_t size) noexcept {
    const auto* bytes = static_cast<const unsigned char*>(data);
    for (std::size_t i = 0; i < size; ++i) { hash ^= bytes[i]; hash *= 1099511628211ULL; }
}
} // namespace

struct NavigationSystem::Impl {
    struct NavMeshDeleter { void operator()(dtNavMesh* value) const noexcept { dtFreeNavMesh(value); } };
    struct QueryDeleter { void operator()(dtNavMeshQuery* value) const noexcept { dtFreeNavMeshQuery(value); } };
    std::unique_ptr<dtNavMesh, NavMeshDeleter> mesh;
    std::unique_ptr<dtNavMeshQuery, QueryDeleter> query;
};

bool NavigationBuildSettings::validate(std::string& error) const {
    const auto positive = [](const float value) { return std::isfinite(value) && value > 0.0F; };
    if (!positive(cellSize) || !positive(cellHeight) || !positive(agentHeight) ||
        !positive(agentRadius) || !std::isfinite(agentMaximumClimb) || agentMaximumClimb < 0.0F ||
        !std::isfinite(agentMaximumSlopeDegrees) || agentMaximumSlopeDegrees <= 0.0F ||
        agentMaximumSlopeDegrees >= 90.0F || verticesPerPolygon < 3 || verticesPerPolygon > DT_VERTS_PER_POLYGON) {
        error = "navigation settings contain invalid cell, agent, slope, or polygon values";
        return false;
    }
    return true;
}

std::uint64_t NavigationBuildSettings::checksum() const noexcept {
    std::uint64_t hash = 14695981039346656037ULL;
    mixHash(hash, this, sizeof(*this));
    return hash;
}

bool NavigationAgentState::shouldRepath(const glm::vec3& position, const glm::vec3& target,
                                        const float deltaTime, const float targetThreshold) {
    pathAge += std::max(0.0F, deltaTime);
    const float displacement = glm::length(position - previousPosition);
    if (displacement < 0.01F && glm::length(target - position) > 0.5F) stuckTime += std::max(0.0F, deltaTime);
    else stuckTime = 0.0F;
    previousPosition = position;
    const bool targetMoved = glm::length(target - destination) > targetThreshold;
    return path.empty() || targetMoved || pathAge >= repathDelay || stuckTime >= 1.0F;
}

NavigationSystem::NavigationSystem() : impl_(std::make_unique<Impl>()) {}
NavigationSystem::~NavigationSystem() = default;
NavigationSystem::NavigationSystem(NavigationSystem&&) noexcept = default;
NavigationSystem& NavigationSystem::operator=(NavigationSystem&&) noexcept = default;

bool NavigationSystem::build(const std::vector<Triangle>& triangles,
                             const NavigationBuildSettings& settings, std::string& error,
                             const std::span<const OffMeshLink> links) {
    clear();
    if (!settings.validate(error)) return false;
    if (triangles.empty()) { error = "cannot build navigation from an empty collision mesh"; return false; }
    const auto started = std::chrono::steady_clock::now();
    std::vector<float> vertices;
    std::vector<int> indices;
    vertices.reserve(triangles.size() * 9);
    indices.reserve(triangles.size() * 3);
    for (const Triangle& triangle : triangles) {
        const std::array<glm::vec3, 3> points{triangle.a, triangle.b, triangle.c};
        for (const glm::vec3& point : points) {
            indices.push_back(static_cast<int>(vertices.size() / 3));
            vertices.insert(vertices.end(), {point.x, point.y, point.z});
        }
    }
    rcConfig config{};
    config.cs = settings.cellSize; config.ch = settings.cellHeight;
    config.walkableSlopeAngle = settings.agentMaximumSlopeDegrees;
    config.walkableHeight = static_cast<int>(std::ceil(settings.agentHeight / config.ch));
    config.walkableClimb = static_cast<int>(std::floor(settings.agentMaximumClimb / config.ch));
    config.walkableRadius = static_cast<int>(std::ceil(settings.agentRadius / config.cs));
    config.maxEdgeLen = static_cast<int>(settings.edgeMaximumLength / config.cs);
    config.maxSimplificationError = settings.edgeMaximumError;
    config.minRegionArea = static_cast<int>(settings.regionMinimumSize * settings.regionMinimumSize);
    config.mergeRegionArea = static_cast<int>(settings.regionMergeSize * settings.regionMergeSize);
    config.maxVertsPerPoly = settings.verticesPerPolygon;
    config.detailSampleDist = settings.detailSampleDistance < 0.9F ? 0.0F : config.cs * settings.detailSampleDistance;
    config.detailSampleMaxError = config.ch * settings.detailSampleMaximumError;
    rcCalcBounds(vertices.data(), static_cast<int>(vertices.size() / 3), config.bmin, config.bmax);
    rcCalcGridSize(config.bmin, config.bmax, config.cs, &config.width, &config.height);
    rcContext context;
    Heightfield solid{rcAllocHeightfield()};
    if (!solid || !rcCreateHeightfield(&context, *solid, config.width, config.height,
                                       config.bmin, config.bmax, config.cs, config.ch)) {
        error = "Recast failed to allocate the heightfield"; return false;
    }
    std::vector<unsigned char> areas(triangles.size(), RC_NULL_AREA);
    rcMarkWalkableTriangles(&context, config.walkableSlopeAngle, vertices.data(),
                            static_cast<int>(vertices.size() / 3), indices.data(),
                            static_cast<int>(triangles.size()), areas.data());
    if (!rcRasterizeTriangles(&context, vertices.data(), static_cast<int>(vertices.size() / 3),
                              indices.data(), areas.data(), static_cast<int>(triangles.size()),
                              *solid, config.walkableClimb)) {
        error = "Recast failed to rasterize triangles"; return false;
    }
    rcFilterLowHangingWalkableObstacles(&context, config.walkableClimb, *solid);
    rcFilterLedgeSpans(&context, config.walkableHeight, config.walkableClimb, *solid);
    rcFilterWalkableLowHeightSpans(&context, config.walkableHeight, *solid);
    CompactHeightfield compact{rcAllocCompactHeightfield()};
    if (!compact || !rcBuildCompactHeightfield(&context, config.walkableHeight,
                                                config.walkableClimb, *solid, *compact) ||
        !rcErodeWalkableArea(&context, config.walkableRadius, *compact) ||
        !rcBuildDistanceField(&context, *compact) ||
        !rcBuildRegions(&context, *compact, 0, config.minRegionArea, config.mergeRegionArea)) {
        error = "Recast failed while building walkable regions"; return false;
    }
    ContourSet contours{rcAllocContourSet()};
    if (!contours || !rcBuildContours(&context, *compact, config.maxSimplificationError,
                                      config.maxEdgeLen, *contours)) {
        error = "Recast failed to build contours"; return false;
    }
    PolyMesh poly{rcAllocPolyMesh()};
    PolyMeshDetail detail{rcAllocPolyMeshDetail()};
    if (!poly || !detail || !rcBuildPolyMesh(&context, *contours, config.maxVertsPerPoly, *poly) ||
        !rcBuildPolyMeshDetail(&context, *poly, *compact, config.detailSampleDist,
                               config.detailSampleMaxError, *detail)) {
        error = "Recast failed to build polygon/detail meshes"; return false;
    }
    for (int i = 0; i < poly->npolys; ++i) {
        if (poly->areas[i] != RC_NULL_AREA) { poly->areas[i] = walkableArea; poly->flags[i] = walkableFlag; }
    }
    dtNavMeshCreateParams parameters{};
    parameters.verts = poly->verts; parameters.vertCount = poly->nverts;
    parameters.polys = poly->polys; parameters.polyAreas = poly->areas;
    parameters.polyFlags = poly->flags; parameters.polyCount = poly->npolys;
    parameters.nvp = poly->nvp; parameters.detailMeshes = detail->meshes;
    parameters.detailVerts = detail->verts; parameters.detailVertsCount = detail->nverts;
    parameters.detailTris = detail->tris; parameters.detailTriCount = detail->ntris;
    parameters.walkableHeight = settings.agentHeight; parameters.walkableRadius = settings.agentRadius;
    parameters.walkableClimb = settings.agentMaximumClimb; parameters.cs = config.cs; parameters.ch = config.ch;
    parameters.buildBvTree = true;
    std::vector<float> linkVertices;
    std::vector<float> linkRadii;
    std::vector<unsigned char> linkDirections;
    std::vector<unsigned char> linkAreas;
    std::vector<unsigned short> linkFlags;
    std::vector<unsigned int> linkIds;
    for (const OffMeshLink& link : links) {
        if (!link.enabled || link.radius <= 0.0F || !std::isfinite(link.radius)) continue;
        linkVertices.insert(linkVertices.end(), {link.start.x, link.start.y, link.start.z,
                                                  link.end.x, link.end.y, link.end.z});
        linkRadii.push_back(link.radius);
        linkDirections.push_back(link.bidirectional ? DT_OFFMESH_CON_BIDIR : 0);
        linkAreas.push_back(walkableArea); linkFlags.push_back(walkableFlag); linkIds.push_back(link.id);
    }
    parameters.offMeshConVerts = linkVertices.data(); parameters.offMeshConRad = linkRadii.data();
    parameters.offMeshConDir = linkDirections.data(); parameters.offMeshConAreas = linkAreas.data();
    parameters.offMeshConFlags = linkFlags.data(); parameters.offMeshConUserID = linkIds.data();
    parameters.offMeshConCount = static_cast<int>(linkRadii.size());
    std::memcpy(parameters.bmin, poly->bmin, sizeof(parameters.bmin));
    std::memcpy(parameters.bmax, poly->bmax, sizeof(parameters.bmax));
    unsigned char* navigationData = nullptr;
    int navigationDataSize = 0;
    if (!dtCreateNavMeshData(&parameters, &navigationData, &navigationDataSize)) {
        error = "Detour rejected generated navigation data"; return false;
    }
    impl_->mesh.reset(dtAllocNavMesh());
    if (!impl_->mesh || dtStatusFailed(impl_->mesh->init(navigationData, navigationDataSize, DT_TILE_FREE_DATA))) {
        dtFree(navigationData); error = "Detour failed to initialize the navigation mesh"; return false;
    }
    impl_->query.reset(dtAllocNavMeshQuery());
    if (!impl_->query || dtStatusFailed(impl_->query->init(impl_->mesh.get(), 2048))) {
        error = "Detour failed to initialize navigation queries"; clear(); return false;
    }
    settings_ = settings;
    statistics_.polygonCount = static_cast<std::size_t>(poly->npolys);
    statistics_.buildMilliseconds = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started).count();
    return true;
}

NavigationPath NavigationSystem::findPath(const glm::vec3& start, const glm::vec3& destination) {
    NavigationPath result;
    ++statistics_.queryCount;
    const auto started = std::chrono::steady_clock::now();
    if (!ready()) { result.status = PathStatus::Invalid; ++statistics_.failedQueries; return result; }
    dtQueryFilter filter;
    filter.setIncludeFlags(walkableFlag);
    const float extents[3]{settings_.agentRadius * 2.0F, settings_.agentHeight, settings_.agentRadius * 2.0F};
    const float startPoint[3]{start.x, start.y, start.z};
    const float targetPoint[3]{destination.x, destination.y, destination.z};
    float nearestStart[3]{}; float nearestTarget[3]{};
    dtPolyRef startRef = 0; dtPolyRef targetRef = 0;
    impl_->query->findNearestPoly(startPoint, extents, &filter, &startRef, nearestStart);
    impl_->query->findNearestPoly(targetPoint, extents, &filter, &targetRef, nearestTarget);
    if (startRef == 0) result.status = PathStatus::StartOutsideMesh;
    else if (targetRef == 0) result.status = PathStatus::TargetOutsideMesh;
    else {
        std::array<dtPolyRef, 256> polygons{}; int polygonCount = 0;
        const dtStatus pathStatus = impl_->query->findPath(startRef, targetRef, nearestStart,
            nearestTarget, &filter, polygons.data(), &polygonCount, static_cast<int>(polygons.size()));
        if (dtStatusFailed(pathStatus) || polygonCount == 0) result.status = PathStatus::NoPath;
        else {
            std::array<float, 256 * 3> straight{}; std::array<unsigned char, 256> flags{};
            std::array<dtPolyRef, 256> refs{}; int straightCount = 0;
            impl_->query->findStraightPath(nearestStart, nearestTarget, polygons.data(), polygonCount,
                straight.data(), flags.data(), refs.data(), &straightCount, 256);
            result.points.reserve(static_cast<std::size_t>(straightCount));
            for (int i = 0; i < straightCount; ++i) result.points.emplace_back(
                straight[i * 3], straight[i * 3 + 1], straight[i * 3 + 2]);
            const bool partial = polygons[static_cast<std::size_t>(polygonCount - 1)] != targetRef;
            result.status = partial ? PathStatus::Partial : PathStatus::Complete;
            if (partial) ++statistics_.partialQueries;
        }
    }
    if (!result.succeeded() && result.status != PathStatus::Partial) ++statistics_.failedQueries;
    statistics_.queryMilliseconds += std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started).count();
    return result;
}

void NavigationSystem::clear() noexcept {
    if (!impl_) impl_ = std::make_unique<Impl>();
    impl_->query.reset(); impl_->mesh.reset(); statistics_ = {};
}
bool NavigationSystem::ready() const noexcept { return impl_ && impl_->mesh && impl_->query; }

} // namespace reflex::navigation
