#include "entity_tracker.hpp"
#include <algorithm>
#include <shared_mutex>
#include <thread>
#include <iostream>

namespace lattice {
namespace entity {

// ===== SpatialPartition 实现 =====

SpatialPartition::SpatialPartition() {
    queryCache_.reserve(MAX_QUERY_CACHE);
    // MemoryArena只有默认构造函数，需要其他地方初始化
}

SpatialPartition::~SpatialPartition() = default;

void SpatialPartition::registerEntity(int entityId, const Position& pos, float boundingRadius) {
    std::unique_lock lock(rwMutex_);
    
    EntityData data;
    data.pos = pos;
    data.radius = boundingRadius;
    data.lastSeenTick = currentTick_;
    
    entities_[entityId] = data;
    addToRegion(entityId, pos);
    
    stats_.entitiesProcessed.fetch_add(1, std::memory_order_relaxed);
}

void SpatialPartition::unregisterEntity(int entityId) {
    std::unique_lock lock(rwMutex_);
    
    auto it = entities_.find(entityId);
    if (it != entities_.end()) {
        removeFromRegion(entityId, it->second.pos);
        entities_.erase(it);
    }
}

void SpatialPartition::updateEntityPosition(int entityId, const Position& newPos) {
    std::unique_lock lock(rwMutex_);
    
    auto it = entities_.find(entityId);
    if (it == entities_.end()) return;
    
    Position oldPos = it->second.pos;
    it->second.pos = newPos;
    it->second.lastSeenTick = currentTick_;
    it->second.accessCount.fetch_add(1, std::memory_order_relaxed);
    
    // 仅当跨越区域边界时更新分区
    if (oldPos.regionX() != newPos.regionX() || 
        oldPos.regionY() != newPos.regionY() || 
        oldPos.regionZ() != newPos.regionZ()) {
        removeFromRegion(entityId, oldPos);
        addToRegion(entityId, newPos);
    }
}

void SpatialPartition::addToRegion(int entityId, const Position& pos) {
    int rx = pos.regionX();
    int ry = pos.regionY();
    int rz = pos.regionZ();
    
    regions_[rx][ry][rz].push_back(entityId);
}

void SpatialPartition::removeFromRegion(int entityId, const Position& pos) {
    int rx = pos.regionX();
    int ry = pos.regionY();
    int rz = pos.regionZ();
    
    auto rxIt = regions_.find(rx);
    if (rxIt == regions_.end()) return;
    
    auto ryIt = rxIt->second.find(ry);
    if (ryIt == rxIt->second.end()) return;
    
    auto rzIt = ryIt->second.find(rz);
    if (rzIt == ryIt->second.end()) return;
    
    auto& entityList = rzIt->second;
    entityList.erase(std::remove(entityList.begin(), entityList.end(), entityId), entityList.end());
    
    // 清理空列表
    if (entityList.empty()) {
        ryIt->second.erase(rz);
        if (ryIt->second.empty()) {
            rxIt->second.erase(ry);
            if (rxIt->second.empty()) {
                regions_.erase(rx);
            }
        }
    }
}

std::vector<int> SpatialPartition::getVisibleEntities(const Position& viewerPos, float viewDistance) {
    auto startTime = std::chrono::steady_clock::now();
    
    std::shared_lock lock(rwMutex_);
    stats_.totalQueries.fetch_add(1, std::memory_order_relaxed);
    
    // 1. 检查缓存
    for (const auto& cache : queryCache_) {
        if (cache.lastTick == currentTick_ && 
            cache.viewerPos == viewerPos &&
            std::abs(cache.viewDistance - viewDistance) < 0.1f) {
            stats_.cacheHits.fetch_add(1, std::memory_order_relaxed);
            return cache.result;
        }
    }
    
    // 2. 计算区域查询范围
    auto [minX, maxX] = getRegionRange(viewerPos.x, viewDistance);
    auto [minY, maxY] = getRegionRange(viewerPos.y, viewDistance);
    auto [minZ, maxZ] = getRegionRange(viewerPos.z, viewDistance);
    
    // 3. 收集候选实体
    std::vector<int> candidates;
    candidates.reserve(200); // 预分配避免动态扩容
    
    float viewDistanceSq = viewDistance * viewDistance;
    
    for (int rx = minX; rx <= maxX; rx++) {
        auto rxIt = regions_.find(rx);
        if (rxIt == regions_.end()) continue;
        
        stats_.regionsChecked.fetch_add(1, std::memory_order_relaxed);
        
        for (int ry = minY; ry <= maxY; ry++) {
            auto ryIt = rxIt->second.find(ry);
            if (ryIt == rxIt->second.end()) continue;
            
            for (int rz = minZ; rz <= maxZ; rz++) {
                auto rzIt = ryIt->second.find(rz);
                if (rzIt == ryIt->second.end()) continue;
                
                // 检查该区域中的所有实体
                for (int entityId : rzIt->second) {
                    auto it = entities_.find(entityId);
                    if (it != entities_.end()) {
                        // 精确距离检查（使用平方距离避免sqrt开销）
                        float distSq = distanceSquared(viewerPos, it->second.pos);
                        if (distSq <= viewDistanceSq) {
                            candidates.push_back(entityId);
                        }
                    }
                }
            }
        }
    }
    
    // 4. 更新缓存
    lock.unlock(); // 释放读锁，获取写锁
    std::unique_lock writeLock(rwMutex_);
    
    if (queryCache_.size() < MAX_QUERY_CACHE) {
        queryCache_.push_back({
            viewerPos, 
            viewDistance, 
            candidates, 
            currentTick_,
            std::chrono::steady_clock::now()
        });
    } else {
        // 循环缓冲区策略
        size_t index = currentTick_ % MAX_QUERY_CACHE;
        if (index < queryCache_.size()) {
            queryCache_[index] = {
                viewerPos, 
                viewDistance, 
                candidates, 
                currentTick_,
                std::chrono::steady_clock::now()
            };
        }
    }
    
    // 更新性能统计
    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - startTime);
    
    // 原子更新平均时间
    uint64_t oldTotal = stats_.averageQueryTimeNs.load(std::memory_order_relaxed);
    uint64_t newCount = stats_.totalQueries.load(std::memory_order_relaxed);
    uint64_t newTotal = oldTotal + duration.count();
    
    stats_.averageQueryTimeNs.store(newTotal / newCount, std::memory_order_relaxed);
    
    return candidates;
}

std::pair<int, int> SpatialPartition::getRegionRange(float position, float distance) const {
    int centerRegion = static_cast<int>(std::floor(position / REGION_SIZE));
    int range = static_cast<int>(std::ceil(distance / REGION_SIZE)) + 1;
    
    return {centerRegion - range, centerRegion + range};
}

void SpatialPartition::tick() {
    std::unique_lock lock(rwMutex_);
    currentTick_++;
    
    // 每100 ticks清理一次缓存
    if (currentTick_ % TICK_CACHE_CLEANUP == 0) {
        cleanupCache();
    }
    
    // 清理长时间未更新的实体
    if (currentTick_ % 1000 == 0) { // 每1000 ticks检查一次
        cleanupOldEntities();
    }
}

void SpatialPartition::cleanupCache() {
    auto now = std::chrono::steady_clock::now();
    constexpr auto CACHE_TTL = std::chrono::seconds(5); // 5秒TTL
    
    queryCache_.erase(
        std::remove_if(queryCache_.begin(), queryCache_.end(),
            [&](const QueryCache& cache) {
                return (now - cache.createTime) > CACHE_TTL;
            }),
        queryCache_.end()
    );
}

void SpatialPartition::cleanupOldEntities() {
    for (auto it = entities_.begin(); it != entities_.end();) {
        if (currentTick_ - it->second.lastSeenTick > ENTITY_CLEANUP_TICKS) {
            removeFromRegion(it->first, it->second.pos);
            it = entities_.erase(it);
        } else {
            ++it;
        }
    }
}

size_t SpatialPartition::getActiveRegionCount() const {
    std::shared_lock lock(rwMutex_);
    size_t count = 0;
    for (const auto& [rx, yMap] : regions_) {
        for (const auto& [ry, zMap] : yMap) {
            count += zMap.size();
        }
    }
    return count;
}

void SpatialPartition::resetStats() {
    stats_.totalQueries = 0;
    stats_.cacheHits = 0;
    stats_.entitiesProcessed = 0;
    stats_.regionsChecked = 0;
    stats_.averageQueryTimeNs = 0;
}

// ===== EntityTracker 实现 =====

EntityTracker::EntityTracker() = default;

EntityTracker::~EntityTracker() {
    shutdown();
}

bool EntityTracker::initialize(size_t memoryPoolSize) {
    (void)memoryPoolSize; // Unused parameter
    try {
        spatialPartition_ = std::make_unique<SpatialPartition>();
        initialized_ = true;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to initialize EntityTracker: " << e.what() << std::endl;
        return false;
    }
}

void EntityTracker::shutdown() {
    if (initialized_) {
        spatialPartition_.reset();
        initialized_ = false;
    }
}

void EntityTracker::addEntity(int entityId, float x, float y, float z, float radius) {
    if (!initialized_) return;
    
    Position pos{x, y, z};
    spatialPartition_->registerEntity(entityId, pos, radius);
}

void EntityTracker::removeEntity(int entityId) {
    if (!initialized_) return;
    
    spatialPartition_->unregisterEntity(entityId);
}

void EntityTracker::updatePosition(int entityId, float x, float y, float z) {
    if (!initialized_) return;
    
    Position pos{x, y, z};
    spatialPartition_->updateEntityPosition(entityId, pos);
}

std::vector<int> EntityTracker::getVisibleEntitiesForViewer(float viewerX, float viewerY, float viewerZ, 
                                                           float maxDistance) {
    if (!initialized_) return {};
    
    Position viewerPos{viewerX, viewerY, viewerZ};
    return spatialPartition_->getVisibleEntities(viewerPos, maxDistance);
}

void EntityTracker::batchUpdatePositions(const std::vector<std::tuple<int, float, float, float>>& updates) {
    if (!initialized_) return;
    
    for (const auto& [entityId, x, y, z] : updates) {
        Position pos{x, y, z};
        spatialPartition_->updateEntityPosition(entityId, pos);
    }
}

std::vector<std::vector<int>> EntityTracker::batchGetVisibleEntities(
    const std::vector<std::tuple<float, float, float, float>>& viewers) {
    std::vector<std::vector<int>> results;
    results.reserve(viewers.size());
    
    for (const auto& [x, y, z, distance] : viewers) {
        Position viewerPos{x, y, z};
        auto visible = spatialPartition_->getVisibleEntities(viewerPos, distance);
        results.push_back(std::move(visible));
    }
    
    return results;
}

void EntityTracker::tick() {
    if (!initialized_) return;
    
    spatialPartition_->tick();
}

const SpatialPartition::PerformanceStats& EntityTracker::getStats() const {
    return spatialPartition_->getStats();
}

void EntityTracker::resetStats() {
    if (initialized_) {
        spatialPartition_->resetStats();
    }
}

void EntityTracker::setGlobalViewDistance(float distance) {
    if (!initialized_) return;
    
    spatialPartition_->setViewDistance(distance);
}

// ===== JNI桥接实现 =====

thread_local EntityTracker* JNIEntityTracker::instance = nullptr;

void JNIEntityTracker::initialize() {
    if (!instance) {
        instance = new EntityTracker();
        instance->initialize();
    }
}

void JNIEntityTracker::shutdown() {
    if (instance) {
        instance->shutdown();
        delete instance;
        instance = nullptr;
    }
}

void JNIEntityTracker::registerEntity(int entityId, float x, float y, float z, float radius) {
    if (instance) {
        instance->addEntity(entityId, x, y, z, radius);
    }
}

void JNIEntityTracker::updateEntityPosition(int entityId, float x, float y, float z) {
    if (instance) {
        instance->updatePosition(entityId, x, y, z);
    }
}

int* JNIEntityTracker::getVisibleEntities(float viewerX, float viewerY, float viewerZ, 
                                         float viewDistance, int* outCount) {
    if (!instance) {
        *outCount = 0;
        return nullptr;
    }
    
    auto visible = instance->getVisibleEntitiesForViewer(viewerX, viewerY, viewerZ, viewDistance);
    *outCount = static_cast<int>(visible.size());
    
    if (visible.empty()) {
        return nullptr;
    }
    
    // 使用内存池分配
    int* result = new int[visible.size()];
    std::copy(visible.begin(), visible.end(), result);
    
    return result;
}

void JNIEntityTracker::tick() {
    if (instance) {
        instance->tick();
    }
}

} // namespace entity
} // namespace lattice