#include "hierarchical_tracker.hpp"
#include <algorithm>
#include <shared_mutex>
#include <thread>
#include <iostream>
#include <emmintrin.h> // SSE2
#include <cstring>

using lattice::net::MemoryArena;

namespace lattice {
namespace entity {

// 距离平方计算辅助函数
inline float distanceSquared(const Position& a, const Position& b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    float dz = a.z - b.z;
    return dx*dx + dy*dy + dz*dz;
}

// ===== QuadTree 实现 =====

QuadTree::QuadTree(const Position& center, float size) 
    : treeSize_(size) {
    root_ = std::make_unique<QuadNode>(center, size);
}

void QuadTree::subdivide(QuadTree::QuadNode* node) {
    if (node->isLeaf() && node->entityIds.size() > 4) {
        float halfSize = node->size / 2.0f;
        Position childCenter = node->center;
        
        // 四个子节点
        for (int i = 0; i < 4; i++) {
            childCenter.x = node->center.x + (i % 2 == 0 ? -halfSize/2 : halfSize/2);
            childCenter.z = node->center.z + (i < 2 ? -halfSize/2 : halfSize/2);
            node->children[i] = std::make_unique<QuadNode>(childCenter, halfSize);
        }
        
        // 重新分配实体到子节点
        for (int entityId : node->entityIds) {
            for (int i = 0; i < 4; i++) {
                if (node->children[i]->contains(Position())) {
                    insertRecursive(node->children[i].get(), entityId, Position());
                    break;
                }
            }
        }
        node->entityIds.clear();
    }
}

void QuadTree::insert(int entityId, const Position& pos) {
    insertRecursive(root_.get(), entityId, pos);
}

void QuadTree::insertRecursive(QuadTree::QuadNode* node, int entityId, const Position& pos) {
    if (!node->contains(pos)) return;
    
    if (node->isLeaf()) {
        node->entityIds.push_back(entityId);
        subdivide(node);
    } else {
        for (auto& child : node->children) {
            if (child && child->contains(pos)) {
                insertRecursive(child.get(), entityId, pos);
                break;
            }
        }
    }
}

void QuadTree::remove(int entityId, const Position& pos) {
    removeRecursive(root_.get(), entityId, pos);
}

void QuadTree::removeRecursive(QuadTree::QuadNode* node, int entityId, const Position& pos) {
    if (!node->contains(pos)) return;
    
    if (node->isLeaf()) {
        auto it = std::find(node->entityIds.begin(), node->entityIds.end(), entityId);
        if (it != node->entityIds.end()) {
            node->entityIds.erase(it);
        }
    } else {
        for (auto& child : node->children) {
            if (child && child->contains(pos)) {
                removeRecursive(child.get(), entityId, pos);
                break;
            }
        }
    }
}

std::vector<int> QuadTree::queryRange(const Position& center, float range) const {
    std::vector<int> results;
    queryRecursive(root_.get(), center, range, results);
    return results;
}

void QuadTree::queryRecursive(const QuadTree::QuadNode* node, const Position& center,
                             float range, std::vector<int>& results) const {
    float distToCenter = std::sqrt(distanceSquared(center, node->center));
    if (distToCenter > range + node->size) return;
    
    if (node->isLeaf()) {
        for (int entityId : node->entityIds) {
            // 在这里检查实际距离
            results.push_back(entityId);
        }
    } else {
        for (auto& child : node->children) {
            if (child) {
                queryRecursive(child.get(), center, range, results);
            }
        }
    }
}

// ===== SIMD距离计算器实现 =====

bool SIMDDistanceCalculator::avx2Supported_ = false;

bool SIMDDistanceCalculator::checkAVX2Support() {
#if defined(__AVX2__) && defined(__x86_64__)
    unsigned int eax, ebx, ecx, edx;
    // 使用内联汇编或者编译器特定的CPUID函数
    asm volatile ("cpuid"
                  : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx)
                  : "a" (1), "c" (0));
    return (ecx & (1 << 28)) != 0;
#else
    return false;
#endif
}

bool SIMDDistanceCalculator::isAVX2Supported() {
    static bool initialized = false;
    if (!initialized) {
        avx2Supported_ = checkAVX2Support();
        initialized = true;
    }
    return avx2Supported_;
}

void SIMDDistanceCalculator::calculateDistancesAVX2(
    const std::vector<int>& /*entityIds*/,
    const std::vector<EnhancedEntity*>& entities,
    const Position& viewerPos,
    float maxDistSq,
    std::vector<int>& result) {
    
    if (!isAVX2Supported() || entities.size() < SIMD_BATCH_SIZE) {
        calculateDistancesScalar(entities, viewerPos, maxDistSq, result);
        return;
    }
    
    // 准备SIMD寄存器
    __m256 viewerXVec = _mm256_set1_ps(viewerPos.x);
    __m256 viewerZVec = _mm256_set1_ps(viewerPos.z);
    __m256 maxDistSqVec = _mm256_set1_ps(maxDistSq);
    
    size_t i = 0;
    for (; i + SIMD_BATCH_SIZE <= entities.size(); i += SIMD_BATCH_SIZE) {
        // 批量加载8个实体的位置
        __m256 entityX[SIMD_BATCH_SIZE];
        __m256 entityZ[SIMD_BATCH_SIZE];
        
        for (int j = 0; j < SIMD_BATCH_SIZE; j++) {
            entityX[j] = _mm256_set1_ps(entities[i + j]->pos.x);
            entityZ[j] = _mm256_set1_ps(entities[i + j]->pos.z);
        }
        
        // 计算距离平方
        __m256 dx[SIMD_BATCH_SIZE], dz[SIMD_BATCH_SIZE], distSq[SIMD_BATCH_SIZE];
        for (int j = 0; j < SIMD_BATCH_SIZE; j++) {
            dx[j] = _mm256_sub_ps(entityX[j], viewerXVec);
            dz[j] = _mm256_sub_ps(entityZ[j], viewerZVec);
            distSq[j] = _mm256_add_ps(_mm256_mul_ps(dx[j], dx[j]), 
                                    _mm256_mul_ps(dz[j], dz[j]));
        }
        
        // 批量比较和收集结果
        for (int j = 0; j < SIMD_BATCH_SIZE; j++) {
            __m256 mask = _mm256_cmp_ps(distSq[j], maxDistSqVec, _CMP_LE_OQ);
            if (_mm256_movemask_ps(mask)) {
                result.push_back(entities[i + j]->id);
            }
        }
    }
    
    // 处理剩余实体
    for (; i < entities.size(); i++) {
        float dx = entities[i]->pos.x - viewerPos.x;
        float dz = entities[i]->pos.z - viewerPos.z;
        if (dx*dx + dz*dz <= maxDistSq) {
            result.push_back(entities[i]->id);
        }
    }
}

void SIMDDistanceCalculator::calculateDistancesScalar(
    const std::vector<EnhancedEntity*>& candidates,
    const Position& viewerPos,
    float maxDistSq,
    std::vector<int>& result) {
    
    for (EnhancedEntity* entity : candidates) {
        float dx = entity->pos.x - viewerPos.x;
        float dz = entity->pos.z - viewerPos.z;
        if (dx*dx + dz*dz <= maxDistSq) {
            result.push_back(entity->id);
        }
    }
}

// ===== 异步实体同步实现 =====

AsyncEntitySync::AsyncEntitySync() {
    taskPool_ = std::make_unique<MemoryArena>();
    workerThread_ = std::thread(&AsyncEntitySync::workerLoop, this);
}

AsyncEntitySync::~AsyncEntitySync() {
    running_ = false;
    if (workerThread_.joinable()) {
        workerThread_.join();
    }
}

void AsyncEntitySync::enqueueUpdate(int entityId, const Position& pos,
                                   float yaw, float pitch, uint8_t flags) {
    void* mem = taskPool_->allocate(sizeof(SyncTask));
    SyncTask* task = new (mem) SyncTask(entityId, pos, yaw, pitch, flags, 0);
    
    taskQueue_.enqueue(*task); // 使用普通的enqueue而不是try_enqueue
}

void AsyncEntitySync::workerLoop() {
    while (running_) {
        SyncTask task;
        if (taskQueue_.try_dequeue(&task)) {
            processTask(task);
            // 手动调用析构函数
            task.~SyncTask();
        } else {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }
}

void AsyncEntitySync::processTask(const SyncTask& /*task*/) {
    // 在后台线程处理实体同步
    // 这里可以添加实际的同步逻辑
    std::this_thread::sleep_for(std::chrono::microseconds(1)); // 模拟工作
}

// ===== 分层空间索引实现 =====

HierarchicalTracker::HierarchicalTracker(int worldHeight) 
    : worldHeight_(worldHeight) {
    queryCache_.reserve(MAX_QUERY_CACHE);
    memoryPool_ = std::make_unique<MemoryArena>();
    asyncSync_ = std::make_unique<AsyncEntitySync>();
}

HierarchicalTracker::~HierarchicalTracker() = default;

void HierarchicalTracker::registerEntity(int id, float x, float y, float z,
                                        float radius, EntityType type) {
    std::unique_lock lock(rwMutex_);
    
    // 由于EnhancedEntity包含atomic，使用默认构造然后赋值
    entities_[id] = EnhancedEntity{};
    auto& entity = entities_[id];
    entity.id = id;
    entity.pos = Position(x, y, z);
    entity.radius = radius;
    entity.type = type;
    entity.priority = static_cast<EntityPriority>(static_cast<uint8_t>(type));
    entity.lod = EntityLOD::LOD_FULL;
    entity.lastUpdateTick = currentTick_;
    addToRegion(id, entity.pos);
    
    // 为玩家创建预测器
    if (type == EntityType::PLAYER) {
        playerPredictors_[id] = PlayerPredictor();
        playerPredictors_[id].lastUpdateTick = currentTick_;
        playerPredictors_[id].lastPos = entity.pos;
        playerPredictors_[id].predictedPos = entity.pos;
    }
    
    stats_.entitiesProcessed.fetch_add(1, std::memory_order_relaxed);
}

void HierarchicalTracker::unregisterEntity(int id) {
    std::unique_lock lock(rwMutex_);
    
    auto it = entities_.find(id);
    if (it != entities_.end()) {
        removeFromRegion(id, it->second.pos);
        entities_.erase(it);
        playerPredictors_.erase(id);
    }
}

void HierarchicalTracker::updateEntityPosition(int id, float x, float y, float z) {
    std::unique_lock lock(rwMutex_);
    
    auto it = entities_.find(id);
    if (it == entities_.end()) return;
    
    Position newPos(x, y, z);
    Position oldPos = it->second.pos;
    it->second.pos = newPos;
    it->second.lastUpdateTick = currentTick_;
    it->second.accessCount++;
    
    // 更新玩家预测器
    auto predictorIt = playerPredictors_.find(id);
    if (predictorIt != playerPredictors_.end()) {
        predictorIt->second.update(newPos, currentTick_);
    }
    
    // 跨区域移动时更新索引
    int oldChunkX = oldPos.chunkX();
    int oldChunkZ = oldPos.chunkZ();
    int newChunkX = newPos.chunkX();
    int newChunkZ = newPos.chunkZ();
    
    if (oldChunkX != newChunkX || oldChunkZ != newChunkZ) {
        removeFromRegion(id, oldPos);
        addToRegion(id, newPos);
    }
    
    // 异步同步更新
    asyncSync_->enqueueUpdate(id, newPos, 0.0f, 0.0f, 0);
    stats_.asyncTasks.fetch_add(1, std::memory_order_relaxed);
}

void HierarchicalTracker::addToRegion(int entityId, const Position& pos) {
    int chunkX = pos.chunkX();
    int chunkZ = pos.chunkZ();
    
    Region* region = getOrCreateRegion(chunkX, chunkZ);
    region->entityIds.push_back(entityId);
    
    // 在区域的四叉树中插入实体
    if (!region->quadTree) {
        float regionSize = CHUNK_SIZE * 2.0f; // 双倍chunk大小
        Position center(pos.x, pos.y, pos.z);
        region->quadTree = std::make_unique<QuadTree>(center, regionSize);
    }
    region->quadTree->insert(entityId, pos);
}

void HierarchicalTracker::removeFromRegion(int entityId, const Position& pos) {
    int chunkX = pos.chunkX();
    int chunkZ = pos.chunkZ();
    
    auto chunkIt = regions_.find(chunkX);
    if (chunkIt == regions_.end()) return;
    
    auto& chunkRegions = chunkIt->second;
    auto regionIt = chunkRegions.find(chunkZ);
    if (regionIt == chunkRegions.end()) return;
    
    Region& region = regionIt->second;
    
    // 从向量中移除
    auto& entityIds = region.entityIds;
    entityIds.erase(std::remove(entityIds.begin(), entityIds.end(), entityId), entityIds.end());
    
    // 从四叉树中移除
    if (region.quadTree) {
        region.quadTree->remove(entityId, pos);
    }
    
    // 清理空区域
    if (entityIds.empty()) {
        chunkIt->second.erase(chunkZ);
        if (chunkIt->second.empty()) {
            regions_.erase(chunkX);
        }
    }
}

Region* HierarchicalTracker::getOrCreateRegion(int chunkX, int chunkZ) {
    return &regions_[chunkX][chunkZ];
}

std::vector<int> HierarchicalTracker::getVisibleEntities(int viewerId, float viewerX,
                                                        float viewerY, float viewerZ,
                                                        float viewDistance) {
    auto startTime = std::chrono::steady_clock::now();
    
    std::unique_lock lock(rwMutex_);
    
    Position viewerPos(viewerX, viewerY, viewerZ);
    float maxDistSq = viewDistance * viewDistance;
    
    // 检查缓存
    if (const std::vector<int>* cached = getFromCache(viewerPos, viewDistance)) {
        stats_.cacheHits.fetch_add(1, std::memory_order_relaxed);
        return *cached;
    }
    
    std::vector<int> result;
    
    // 1. 查询当前视野
    std::vector<int> currentView = queryCurrentView(viewerPos, viewDistance);
    
    // 2. 预测性加载（如果启用）
    if (predictiveLoading_) {
        auto predictorIt = playerPredictors_.find(viewerId);
        if (predictorIt != playerPredictors_.end()) {
            const Position& predictedPos = predictorIt->second.getPredictedPosition();
            std::vector<int> predictedView = queryPredictedView(predictedPos, viewDistance * 0.8f);
            mergeQueryResults(currentView, predictedView, result);
        } else {
            result = std::move(currentView);
        }
    } else {
        result = std::move(currentView);
    }
    
    // 3. SIMD加速距离计算
    if (simdEnabled_ && result.size() >= SIMD_BATCH_SIZE) {
        std::vector<EnhancedEntity*> candidates;
        candidates.reserve(result.size());
        
        for (int entityId : result) {
            auto it = entities_.find(entityId);
            if (it != entities_.end()) {
                candidates.push_back(&it->second);
            }
        }
        
        std::vector<int> filtered;
        calculateDistancesSIMD(candidates, viewerPos, maxDistSq, filtered);
        result = std::move(filtered);
        stats_.simdOperations.fetch_add(1, std::memory_order_relaxed);
    }
    
    // 4. 缓存结果
    addToCache(viewerPos, viewDistance, result);
    
    stats_.totalQueries.fetch_add(1, std::memory_order_relaxed);
    
    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - startTime);
    stats_.averageQueryTimeNs.store(duration.count(), std::memory_order_relaxed);
    
    return result;
}

std::vector<int> HierarchicalTracker::queryCurrentView(const Position& viewerPos, float viewDistance) {
    std::vector<int> result;
    
    // 查询邻近区域
    int chunkX = viewerPos.chunkX();
    int chunkZ = viewerPos.chunkZ();
    
    for (int dx = -1; dx <= 1; dx++) {
        for (int dz = -1; dz <= 1; dz++) {
            auto chunkIt = regions_.find(chunkX + dx);
            if (chunkIt != regions_.end()) {
                auto& chunkRegions = chunkIt->second;
                auto regionIt = chunkRegions.find(chunkZ + dz);
                if (regionIt != chunkRegions.end()) {
                    const Region& region = regionIt->second;
                    // 使用四叉树进行细粒度查询
                    if (region.quadTree) {
                        auto quadResults = region.quadTree->queryRange(viewerPos, viewDistance);
                        result.insert(result.end(), quadResults.begin(), quadResults.end());
                    } else {
                        result.insert(result.end(), region.entityIds.begin(), region.entityIds.end());
                    }
                }
            }
        }
    }
    
    return result;
}

std::vector<int> HierarchicalTracker::queryPredictedView(const Position& predictedPos, float viewDistance) {
    // 预测视图的查询逻辑与当前视图类似，但范围稍小
    return queryCurrentView(predictedPos, viewDistance);
}

void HierarchicalTracker::mergeQueryResults(const std::vector<int>& currentView,
                                           const std::vector<int>& predictedView,
                                           std::vector<int>& result) {
    result = currentView; // 优先当前视野
    result.reserve(currentView.size() + predictedView.size());
    
    // 合并预测视图中的新实体
    for (int entityId : predictedView) {
        if (std::find(currentView.begin(), currentView.end(), entityId) == currentView.end()) {
            result.push_back(entityId);
        }
    }
}

void HierarchicalTracker::calculateDistancesSIMD(const std::vector<EnhancedEntity*>& candidates,
                                                const Position& viewerPos,
                                                float maxDistSq,
                                                std::vector<int>& result) {
    SIMDDistanceCalculator::calculateDistancesAVX2(
        std::vector<int>(), candidates, viewerPos, maxDistSq, result);
}

void HierarchicalTracker::tick() {
    std::unique_lock lock(rwMutex_);
    currentTick_++;
    
    // 按优先级处理实体
    if (priorityScheduling_) {
        processEntitiesByPriority();
    }
    
    // 清理任务
    if (currentTick_ % TICK_CACHE_CLEANUP == 0) {
        cleanupCache();
    }
    
    if (currentTick_ % (ENTITY_CLEANUP_TICKS / 10) == 0) {
        cleanupOldEntities();
    }
}

void HierarchicalTracker::processEntitiesByPriority() {
    // 按优先级分组实体
    std::array<std::vector<EnhancedEntity*>, 4> priorityGroups;
    
    for (auto& pair : entities_) {
        EnhancedEntity* entity = &pair.second;
        priorityGroups[static_cast<int>(entity->priority)].push_back(entity);
    }
    
    // 高优先级优先处理
    for (int priority = 0; priority < 4; priority++) {
        for (EnhancedEntity* entity : priorityGroups[priority]) {
            // 动态跳过低优先级更新
            if (priority >= static_cast<int>(EntityPriority::MEDIUM) && 
                currentTick_ % 2 != 0 && 
                !entity->isMoving()) {
                continue;
            }
            
            // 更新实体逻辑
            // 这里可以添加更复杂的处理逻辑
        }
    }
}

void HierarchicalTracker::cleanupCache() {
    std::erase_if(queryCache_, [this](const QueryCache& cache) {
        return !cache.isValid(currentTick_);
    });
}

void HierarchicalTracker::addToCache(const Position& viewerPos, float viewDistance,
                                    const std::vector<int>& result) {
    if (queryCache_.size() >= MAX_QUERY_CACHE) {
        queryCache_.erase(queryCache_.begin());
    }
    
    QueryCache cache;
    cache.viewerPos = viewerPos;
    cache.viewDistance = viewDistance;
    cache.cacheTick = currentTick_;
    cache.result = result;
    
    queryCache_.push_back(std::move(cache));
}

const std::vector<int>* HierarchicalTracker::getFromCache(const Position& viewerPos, float viewDistance) {
    for (const auto& cache : queryCache_) {
        if (cache.isValid(currentTick_) &&
            std::abs(cache.viewerPos.x - viewerPos.x) < 0.5f &&
            std::abs(cache.viewerPos.z - viewerPos.z) < 0.5f &&
            std::abs(cache.viewDistance - viewDistance) < 0.5f) {
            return &cache.result;
        }
    }
    return nullptr;
}

void HierarchicalTracker::cleanupOldEntities() {
    auto it = entities_.begin();
    while (it != entities_.end()) {
        if (currentTick_ - it->second.lastUpdateTick > ENTITY_CLEANUP_TICKS) {
            removeFromRegion(it->first, it->second.pos);
            it = entities_.erase(it);
        } else {
            ++it;
        }
    }
}

size_t HierarchicalTracker::getActiveRegionCount() const {
    size_t count = 0;
    for (const auto& chunkPair : regions_) {
        count += chunkPair.second.size();
    }
    return count;
}

void HierarchicalTracker::resetStats() {
    stats_.totalQueries = 0;
    stats_.cacheHits = 0;
    stats_.simdOperations = 0;
    stats_.asyncTasks = 0;
    stats_.entitiesProcessed = 0;
    stats_.averageQueryTimeNs = 0;
}

// ===== JNI桥接实现 =====

thread_local HierarchicalTracker* JNIHierarchicalTracker::instance = nullptr;

void JNIHierarchicalTracker::initialize() {
    if (!instance) {
        instance = HierarchicalTrackerFactory::forThread();
    }
}

void JNIHierarchicalTracker::shutdown() {
    if (instance) {
        HierarchicalTrackerFactory::cleanupThread();
        instance = nullptr;
    }
}

void JNIHierarchicalTracker::registerEntity(int entityId, float x, float y, float z,
                                           float radius, uint8_t entityType) {
    if (instance) {
        instance->registerEntity(entityId, x, y, z, radius, 
                                static_cast<EntityType>(entityType));
    }
}

void JNIHierarchicalTracker::updateEntityPosition(int entityId, float x, float y, float z) {
    if (instance) {
        instance->updateEntityPosition(entityId, x, y, z);
    }
}

int* JNIHierarchicalTracker::getVisibleEntities(int viewerId, float viewerX, float viewerY,
                                                float viewerZ, float viewDistance, int* outCount) {
    if (!instance) {
        *outCount = 0;
        return nullptr;
    }
    
    std::vector<int> visible = instance->getVisibleEntities(viewerId, viewerX, viewerY, 
                                                           viewerZ, viewDistance);
    
    *outCount = static_cast<int>(visible.size());
    if (*outCount > 0) {
        int* result = new int[*outCount];
        std::copy(visible.begin(), visible.end(), result);
        return result;
    }
    return nullptr;
}

void JNIHierarchicalTracker::tick() {
    if (instance) {
        instance->tick();
    }
}

void* JNIHierarchicalTracker::compressEntityUpdates(const int* entityIds, int count,
                                                   size_t* compressedSize) {
    if (!instance || count == 0) {
        *compressedSize = 0;
        return nullptr;
    }
    
    // 序列化实体更新
    std::vector<uint8_t> uncompressedData;
    uncompressedData.reserve(count * 16); // 预估大小
    
    for (int i = 0; i < count; i++) {
        // 简单的序列化格式：entityId + 位置
        uint8_t idBytes[4];
        memcpy(idBytes, &entityIds[i], 4);
        uncompressedData.insert(uncompressedData.end(), idBytes, idBytes + 4);
    }
    
    // 使用NativeCompressor压缩
    auto compressor = lattice::net::NativeCompressor::forThread(6);
    if (!compressor) {
        *compressedSize = 0;
        return nullptr;
    }
    
    size_t maxCompressedSize = uncompressedData.size() * 1.1 + 16;
    void* compressedData = malloc(maxCompressedSize);
    
    size_t actualSize = compressor->compressZlib(
        reinterpret_cast<const char*>(uncompressedData.data()),
        uncompressedData.size(),
        reinterpret_cast<char*>(compressedData),
        maxCompressedSize
    );
    
    *compressedSize = actualSize;
    return compressedData;
}

// ===== 工厂方法实现 =====

thread_local std::unique_ptr<HierarchicalTracker> HierarchicalTrackerFactory::threadInstance_;

HierarchicalTracker* HierarchicalTrackerFactory::forThread() {
    if (!threadInstance_) {
        threadInstance_ = std::make_unique<HierarchicalTracker>();
    }
    return threadInstance_.get();
}

void HierarchicalTrackerFactory::cleanupThread() {
    threadInstance_.reset();
}

} // namespace entity
} // namespace lattice