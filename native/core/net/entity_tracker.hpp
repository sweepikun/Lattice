#pragma once

#include <vector>
#include <unordered_map>
#include <memory>
#include <cmath>
#include <atomic>
#include <chrono>
#include <optional>
#include <shared_mutex>
#include "memory_arena.hpp"

namespace lattice {
namespace entity {

// 常量定义
constexpr int REGION_SIZE = 32;
constexpr float VIEW_DISTANCE = 64.0f;
constexpr size_t MAX_QUERY_CACHE = 32;
constexpr int TICK_CACHE_CLEANUP = 100;
constexpr int ENTITY_CLEANUP_TICKS = 6000; // 5分钟

// 3D位置结构
struct Position {
    float x, y, z;
    
    Position() : x(0), y(0), z(0) {}
    Position(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    
    int regionX() const { return static_cast<int>(std::floor(x / REGION_SIZE)); }
    int regionY() const { return static_cast<int>(std::floor(y / REGION_SIZE)); }
    int regionZ() const { return static_cast<int>(std::floor(z / REGION_SIZE)); }
    
    struct ChunkPos {
        int x, z;
    };
    
    ChunkPos chunkPos() const { 
        return {static_cast<int>(x) >> 4, static_cast<int>(z) >> 4}; 
    }
    
    bool operator==(const Position& other) const {
        return std::abs(x - other.x) < 0.1f && 
               std::abs(y - other.y) < 0.1f && 
               std::abs(z - other.z) < 0.1f;
    }
};

// 实体数据
struct EntityData {
    Position pos{};
    float radius = 0.0f;
    int lastSeenTick = 0;
    std::atomic<int> accessCount{0};
    
    // 默认构造函数
    EntityData() = default;
    
    // 自定义复制构造函数，因为包含atomic成员
    EntityData(const EntityData& other) 
        : pos(other.pos), radius(other.radius), lastSeenTick(other.lastSeenTick),
          accessCount(other.accessCount.load()) {}
    
    EntityData& operator=(const EntityData& other) {
        if (this != &other) {
            pos = other.pos;
            radius = other.radius;
            lastSeenTick = other.lastSeenTick;
            accessCount.store(other.accessCount.load());
        }
        return *this;
    }
};

// 查询缓存
struct QueryCache {
    Position viewerPos;
    float viewDistance;
    std::vector<int> result;
    int lastTick;
    std::chrono::steady_clock::time_point createTime;
};

// 空间分区类 - 核心实现
class SpatialPartition {
public:
    SpatialPartition();
    ~SpatialPartition();
    
    // 实体注册与管理
    void registerEntity(int entityId, const Position& pos, float boundingRadius);
    void unregisterEntity(int entityId);
    void updateEntityPosition(int entityId, const Position& newPos);
    
    // 可见性查询
    std::vector<int> getVisibleEntities(const Position& viewerPos, float viewDistance = VIEW_DISTANCE);
    
    // 批量更新（每tick调用一次）
    void tick();
    
    // 性能统计
    struct PerformanceStats {
        std::atomic<uint64_t> totalQueries{0};
        std::atomic<uint64_t> cacheHits{0};
        std::atomic<uint64_t> entitiesProcessed{0};
        std::atomic<uint64_t> regionsChecked{0};
        std::atomic<double> averageQueryTimeNs{0};
    };
    
    const PerformanceStats& getStats() const { return stats_; }
    void resetStats();
    
    // 配置
    void setViewDistance(float distance) { viewDistance_ = distance; }
    void setRegionSize(int size) { 
        (void)size; // Runtime size adjustment requires special handling
    }
    
    // 调试信息
    size_t getEntityCount() const { return entities_.size(); }
    size_t getActiveRegionCount() const;
    size_t getCacheSize() const { return queryCache_.size(); }

private:
    // 3D网格分区存储：regions_[regionX][regionY][regionZ] = vector<entityId>
    std::unordered_map<int, std::unordered_map<int, std::unordered_map<int, std::vector<int>>>> regions_;
    
    // 实体数据映射：entityId -> EntityData
    std::unordered_map<int, EntityData> entities_;
    
    // 查询结果缓存
    std::vector<QueryCache> queryCache_;
    
    // 内存池（使用Arena Allocator）
    std::unique_ptr<net::MemoryArena> memoryPool_;
    
    // 配置参数
    float viewDistance_ = VIEW_DISTANCE;
    int currentTick_ = 0;
    
    // 性能统计
    mutable PerformanceStats stats_;
    
    // 线程安全控制
    mutable std::shared_mutex rwMutex_;
    
    // 辅助方法
    void addToRegion(int entityId, const Position& pos);
    void removeFromRegion(int entityId, const Position& pos);
    std::pair<int, int> getRegionRange(float position, float distance) const;
    
    // 缓存管理
    void cleanupCache();
    void cleanupOldEntities();
    
    // 数学工具
    static inline float distanceSquared(const Position& a, const Position& b) {
        float dx = a.x - b.x;
        float dy = a.y - b.y;
        float dz = a.z - b.z;
        return dx*dx + dy*dy + dz*dz;
    }
    
    // 缓存键生成
    struct CacheKey {
        Position viewerPos;
        float viewDistance;
        
        bool operator==(const CacheKey& other) const {
            return viewerPos == other.viewerPos && 
                   std::abs(viewDistance - other.viewDistance) < 0.1f;
        }
    };
    
    struct CacheKeyHash {
        size_t operator()(const CacheKey& key) const noexcept {
            size_t h1 = std::hash<float>{}(key.viewerPos.x);
            size_t h2 = std::hash<float>{}(key.viewerPos.y);
            size_t h3 = std::hash<float>{}(key.viewerPos.z);
            size_t h4 = std::hash<float>{}(key.viewDistance);
            return ((h1 * 31 + h2) * 31 + h3) * 31 + h4;
        }
    };
};

// 实体追踪器主类 - 提供高级接口
class EntityTracker {
public:
    EntityTracker();
    ~EntityTracker();
    
    // 初始化和清理
    bool initialize(size_t memoryPoolSize = 64 * 1024);
    void shutdown();
    
    // 实体管理
    void addEntity(int entityId, float x, float y, float z, float radius = 0.5f);
    void removeEntity(int entityId);
    void updatePosition(int entityId, float x, float y, float z);
    
    // 可见性查询
    std::vector<int> getVisibleEntitiesForViewer(float viewerX, float viewerY, float viewerZ, 
                                                   float maxDistance = VIEW_DISTANCE);
    
    // 批量操作
    void batchUpdatePositions(const std::vector<std::tuple<int, float, float, float>>& updates);
    std::vector<std::vector<int>> batchGetVisibleEntities(
        const std::vector<std::tuple<float, float, float, float>>& viewers);
    
    // 生命周期管理
    void tick();
    
    // 统计和监控
    const SpatialPartition::PerformanceStats& getStats() const;
    void resetStats();
    
    // 配置
    void setGlobalViewDistance(float distance);
    
private:
    std::unique_ptr<SpatialPartition> spatialPartition_;
    bool initialized_ = false;
};

// JNI桥接用的简化接口
extern "C" {
    // 这些函数将由JNI层调用
    struct JNIEntityTracker {
        static thread_local EntityTracker* instance;
        
        static void initialize();
        static void shutdown();
        static void registerEntity(int entityId, float x, float y, float z, float radius);
        static void updateEntityPosition(int entityId, float x, float y, float z);
        static int* getVisibleEntities(float viewerX, float viewerY, float viewerZ, 
                                      float viewDistance, int* outCount);
        static void tick();
    };
}

} // namespace entity
} // namespace lattice

// Hash函数特化
namespace std {
    template<>
    struct hash<lattice::entity::Position> {
        size_t operator()(const lattice::entity::Position& pos) const noexcept {
            size_t h1 = std::hash<float>{}(pos.x);
            size_t h2 = std::hash<float>{}(pos.y);
            size_t h3 = std::hash<float>{}(pos.z);
            return ((h1 * 31 + h2) * 31 + h3);
        }
    };
}