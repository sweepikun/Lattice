#pragma once

#include <vector>
#include <unordered_map>
#include <memory>
#include <cmath>
#include <atomic>
#include <chrono>
#include <optional>
#include <array>
#include <immintrin.h> // SIMD support
#include <shared_mutex>
#include "memory_arena.hpp"
#include "native_compressor.hpp"

using lattice::net::MemoryArena;

// 简化的无锁队列实现（避免外部依赖）
namespace moodycamel {
template<typename T>
class ConcurrentQueue {
private:
    struct Node {
        T data;
        std::atomic<Node*> next;
        
        Node() : data(T()), next(nullptr) {}
        explicit Node(const T& item) : data(item), next(nullptr) {}
    };
    
    std::atomic<Node*> head_;
    std::atomic<Node*> tail_;
    
public:
    ConcurrentQueue() {
        Node* dummy = new Node();
        head_.store(dummy, std::memory_order_relaxed);
        tail_.store(dummy, std::memory_order_relaxed);
    }
    
    ~ConcurrentQueue() {
        while (try_dequeue(nullptr)) {} // 清空队列
        Node* dummy = head_.load();
        delete dummy;
    }
    
    void enqueue(const T& item) {
        Node* newNode = new Node(item);
        Node* currentTail = tail_.load(std::memory_order_relaxed);
        
        while (true) {
            Node* next = currentTail->next.load(std::memory_order_acquire);
            if (next == nullptr) {
                if (currentTail->next.compare_exchange_weak(next, newNode,
                    std::memory_order_release, std::memory_order_relaxed)) {
                    break;
                }
            } else {
                if (tail_.compare_exchange_weak(currentTail, next,
                    std::memory_order_relaxed, std::memory_order_relaxed)) {
                    currentTail = next;
                }
            }
        }
        
        tail_.compare_exchange_weak(currentTail, newNode,
            std::memory_order_relaxed, std::memory_order_relaxed);
    }
    
    bool try_enqueue(const T& item) {
        // try_enqueue is essentially the same as enqueue but may fail if queue is full
        // For our simplified implementation, we always succeed
        enqueue(item);
        return true;
    }
    
    bool try_dequeue(T* result) {
        Node* currentHead = head_.load(std::memory_order_relaxed);
        Node* next = currentHead->next.load(std::memory_order_acquire);
        
        if (next == nullptr) {
            return false;
        }
        
        if (result) {
            *result = next->data;
        }
        
        if (head_.compare_exchange_weak(currentHead, next,
            std::memory_order_relaxed, std::memory_order_relaxed)) {
            delete currentHead;
            return true;
        }
        
        return false;
    }
    
    size_t size_approx() const {
        // 简化实现，不提供精确大小
        return 0;
    }
};
}

namespace lattice {
namespace entity {

// ===== 常量定义 =====
constexpr int CHUNK_SIZE = 32;
constexpr float MAX_VIEW_DISTANCE = 64.0f;
constexpr size_t MAX_QUERY_CACHE = 32;
constexpr int TICK_CACHE_CLEANUP = 100;
constexpr int PREDICTION_LOOKAHEAD_TICKS = 20; // 1秒预测
constexpr int ENTITY_CLEANUP_TICKS = 6000; // 5分钟
constexpr int SIMD_BATCH_SIZE = 8;

// ===== 实体类型枚举 =====
enum class EntityType : uint8_t {
    PLAYER = 0,
    MONSTER = 1,
    ANIMAL = 2,
    VILLAGER = 3,
    ITEM = 4,
    VEHICLE = 5,
    PROJECTILE = 6,
    OTHER = 7
};

// ===== 优先级调度 =====
enum class EntityPriority : uint8_t {
    CRITICAL = 0,    // 玩家、Boss
    HIGH = 1,        // 攻击性怪物
    MEDIUM = 2,      // 动物、村民
    LOW = 3          // 被动生物、物品
};

// ===== LOD级别定义 =====
enum class EntityLOD : uint8_t {
    LOD_FULL = 0,      // < 32格：完整数据（位置、旋转、装备、效果）
    LOD_MEDIUM = 1,    // 32-64格：位置+旋转
    LOD_LOW = 2        // > 64格：仅位置
};

// ===== 3D位置结构 =====
struct Position {
    float x, y, z;
    
    Position() : x(0), y(0), z(0) {}
    Position(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    
    int chunkX() const { return static_cast<int>(std::floor(x / CHUNK_SIZE)); }
    int chunkZ() const { return static_cast<int>(std::floor(z / CHUNK_SIZE)); }
    
    float distanceSquaredTo(const Position& other) const {
        float dx = x - other.x;
        float dy = y - other.y;
        float dz = z - other.z;
        return dx*dx + dy*dy + dz*dz;
    }
    
    bool operator==(const Position& other) const {
        return std::abs(x - other.x) < 0.1f && 
               std::abs(y - other.y) < 0.1f && 
               std::abs(z - other.z) < 0.1f;
    }
};

// ===== 区域定义（粗粒度） =====
struct Region {
    std::vector<int> entityIds;
    std::unique_ptr<class QuadTree> quadTree; // 细粒度四叉树
    Position center;
    
    Region() = default;
    Region(const Position& centerPos) : center(centerPos) {}
};

// ===== 实体预测器（预测性加载） =====
struct PlayerPredictor {
    Position lastPos;
    Position predictedPos;
    float velocityX, velocityZ;
    uint32_t lastUpdateTick;
    
    void update(const Position& currentPos, uint32_t currentTick) {
        if (currentTick > lastUpdateTick) {
            float dt = (currentTick - lastUpdateTick) / 20.0f; // 20 TPS
            velocityX = (currentPos.x - lastPos.x) / dt;
            velocityZ = (currentPos.z - lastPos.z) / dt;
            
            // 预测1秒后的位置
            predictedPos.x = currentPos.x + velocityX * 1.0f;
            predictedPos.z = currentPos.z + velocityZ * 1.0f;
            predictedPos.y = currentPos.y; // 假设高度不变
            
            lastPos = currentPos;
            lastUpdateTick = currentTick;
        }
    }
    
    const Position& getPredictedPosition() const { return predictedPos; }
};

// ===== 增强实体数据（包含优先级和LOD信息） =====
struct EnhancedEntity {
    int id;
    Position pos;
    float radius;
    EntityType type;
    EntityPriority priority;
    EntityLOD lod;
    uint32_t lastUpdateTick;
    uint32_t accessCount{0};
    
    // 移动检测
    bool isMoving() const {
        // 这里可以添加更复杂的移动检测逻辑
        return lastUpdateTick > 0;
    }
    
    // 动态LOD计算
    EntityLOD calculateLOD(float distance) const {
        if (distance < 32.0f) return EntityLOD::LOD_FULL;
        if (distance < 64.0f) return EntityLOD::LOD_MEDIUM;
        return EntityLOD::LOD_LOW;
    }
};

// ===== 热点查询缓存（复用thread_local思想） =====
struct QueryCache {
    Position viewerPos;
    float viewDistance;
    uint32_t cacheTick;
    std::vector<int> result;
    
    bool isValid(uint32_t currentTick) const {
        return (currentTick - cacheTick) < 5; // 5 tick缓存
    }
};

// ===== 四叉树节点（细粒度空间索引） =====
class QuadTree {
public:
    struct QuadNode {
        Position center;
        float size;
        std::vector<int> entityIds;
        std::array<std::unique_ptr<QuadNode>, 4> children; // 四个子节点
        
        QuadNode(const Position& centerPos, float nodeSize) 
            : center(centerPos), size(nodeSize) {}
        
        bool contains(const Position& pos) const {
            return pos.x >= center.x - size/2 && pos.x < center.x + size/2 &&
                   pos.z >= center.z - size/2 && pos.z < center.z + size/2;
        }
        
        bool isLeaf() const {
            return !children[0];
        }
    };
    
    QuadTree(const Position& center, float size);
    
    void insert(int entityId, const Position& pos);
    void remove(int entityId, const Position& pos);
    std::vector<int> queryRange(const Position& center, float range) const;
    
private:
    std::unique_ptr<QuadNode> root_;
    float treeSize_;
    
    void subdivide(QuadNode* node);
    void insertRecursive(QuadNode* node, int entityId, const Position& pos);
    void removeRecursive(QuadNode* node, int entityId, const Position& pos);
    void queryRecursive(const QuadNode* node, const Position& center, 
                       float range, std::vector<int>& results) const;
};

// ===== 异步实体同步（无锁队列） =====
class AsyncEntitySync {
private:
    struct SyncTask {
        int entityId;
        Position pos;
        float yaw, pitch;
        uint8_t flags;
        uint32_t tick;
        
        SyncTask() = default;
        SyncTask(int id, const Position& p, float y, float pi, uint8_t f, uint32_t t)
            : entityId(id), pos(p), yaw(y), pitch(pi), flags(f), tick(t) {}
    };
    
    // 无锁队列（使用内存池分配）
    moodycamel::ConcurrentQueue<SyncTask> taskQueue_;
    std::thread workerThread_;
    std::atomic<bool> running_ = true;
    
    // 内存池
    std::unique_ptr<MemoryArena> taskPool_;
    
public:
    AsyncEntitySync();
    ~AsyncEntitySync();
    
    void enqueueUpdate(int entityId, const Position& pos, 
                      float yaw, float pitch, uint8_t flags);
    
    size_t getQueueSize() const { return taskQueue_.size_approx(); }
    
private:
    void workerLoop();
    void processTask(const SyncTask& task);
};

// ===== SIMD批量距离计算器 =====
class SIMDDistanceCalculator {
public:
    // AVX2优化的批量距离计算
    static void calculateDistancesAVX2(const std::vector<int>& entityIds,
                                     const std::vector<EnhancedEntity*>& entities,
                                     const Position& viewerPos,
                                     float maxDistSq,
                                     std::vector<int>& result);
    
    // 降级到标量计算
    static void calculateDistancesScalar(const std::vector<EnhancedEntity*>& candidates,
                                       const Position& viewerPos,
                                       float maxDistSq,
                                       std::vector<int>& result);
    
    // 检查CPU支持
    static bool isAVX2Supported();
    
private:
    static bool checkAVX2Support();
    static bool avx2Supported_;
};

// ===== 分层空间索引（核心优化类） =====
class HierarchicalTracker {
public:
    HierarchicalTracker(int worldHeight = 256);
    ~HierarchicalTracker();
    
    // 实体注册与管理
    void registerEntity(int id, float x, float y, float z, 
                       float radius, EntityType type);
    void unregisterEntity(int id);
    void updateEntityPosition(int id, float x, float y, float z);
    
    // 高效查询：获取玩家可见的实体（包含预测性加载）
    std::vector<int> getVisibleEntities(int viewerId, float viewerX, float viewerY, 
                                       float viewerZ, float viewDistance = MAX_VIEW_DISTANCE);
    
    // 每tick调用
    void tick();
    
    // 性能统计
    struct PerformanceStats {
        std::atomic<uint64_t> totalQueries{0};
        std::atomic<uint64_t> cacheHits{0};
        std::atomic<uint64_t> simdOperations{0};
        std::atomic<uint64_t> asyncTasks{0};
        std::atomic<uint64_t> entitiesProcessed{0};
        std::atomic<double> averageQueryTimeNs{0};
    };
    
    const PerformanceStats& getStats() const { return stats_; }
    void resetStats();
    
    // 配置
    void setViewDistance(float distance) { viewDistance_ = distance; }
    void setSimdEnabled(bool enabled) { simdEnabled_ = enabled; }
    void setPredictiveLoading(bool enabled) { predictiveLoading_ = enabled; }
    void setPriorityScheduling(bool enabled) { priorityScheduling_ = enabled; }
    
    // 统计信息
    size_t getEntityCount() const { return entities_.size(); }
    size_t getActiveRegionCount() const;
    
private:
    // 1. 粗粒度：32x32区域网格
    std::unordered_map<int, std::unordered_map<int, Region>> regions_;
    
    // 2. 实体数据（内存池分配）
    std::unordered_map<int, EnhancedEntity> entities_;
    
    // 3. 玩家预测器缓存
    std::unordered_map<int, PlayerPredictor> playerPredictors_;
    
    // 4. 热点缓存
    std::vector<QueryCache> queryCache_;
    
    // 5. 内存池（复用设计）
    std::unique_ptr<MemoryArena> memoryPool_;
    
    // 6. 异步同步
    std::unique_ptr<AsyncEntitySync> asyncSync_;
    
    // 配置参数
    float viewDistance_ = MAX_VIEW_DISTANCE;
    int worldHeight_;
    int currentTick_ = 0;
    
    // 功能开关
    bool simdEnabled_ = true;
    bool predictiveLoading_ = true;
    bool priorityScheduling_ = true;
    
    // 性能统计
    PerformanceStats stats_;
    
    // 线程安全
    mutable std::shared_mutex rwMutex_;
    
    // 辅助方法
    void addToRegion(int entityId, const Position& pos);
    void removeFromRegion(int entityId, const Position& pos);
    Region* getOrCreateRegion(int chunkX, int chunkZ);
    
    // 查询方法
    std::vector<int> queryCurrentView(const Position& viewerPos, float viewDistance);
    std::vector<int> queryPredictedView(const Position& predictedPos, float viewDistance);
    void mergeQueryResults(const std::vector<int>& currentView, 
                          const std::vector<int>& predictedView,
                          std::vector<int>& result);
    
    // SIMD加速
    void calculateDistancesSIMD(const std::vector<EnhancedEntity*>& candidates,
                              const Position& viewerPos,
                              float maxDistSq,
                              std::vector<int>& result);
    
    // 优先级处理
    void processEntitiesByPriority();
    
    // 缓存管理
    void cleanupCache();
    void addToCache(const Position& viewerPos, float viewDistance, const std::vector<int>& result);
    const std::vector<int>* getFromCache(const Position& viewerPos, float viewDistance);
    
    // 清理
    void cleanupOldEntities();
    
    // 数学工具
    static inline float distanceSquared(const Position& a, const Position& b) {
        return a.distanceSquaredTo(b);
    }
};

// ===== JNI桥接接口 =====
extern "C" {
    struct JNIHierarchicalTracker {
        static thread_local HierarchicalTracker* instance;
        
        static void initialize();
        static void shutdown();
        static void registerEntity(int entityId, float x, float y, float z, 
                                  float radius, uint8_t entityType);
        static void updateEntityPosition(int entityId, float x, float y, float z);
        static int* getVisibleEntities(int viewerId, float viewerX, float viewerY, 
                                      float viewerZ, float viewDistance, int* outCount);
        static void tick();
        static void* compressEntityUpdates(const int* entityIds, int count, 
                                          size_t* compressedSize);
    };
}

// ===== 工厂方法（per-thread实例） =====
class HierarchicalTrackerFactory {
public:
    static HierarchicalTracker* forThread();
    static void cleanupThread();
    
private:
    static thread_local std::unique_ptr<HierarchicalTracker> threadInstance_;
};

} // namespace entity
} // namespace lattice