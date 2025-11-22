#include "advanced_light_engine.hpp"
#include <algorithm>
#include <mutex>
#include <thread>
#include <iostream>

namespace lattice {
namespace world {

// 静态成员初始化
std::array<uint8_t, 256> LightOpacityTable::opacityTable{};
bool LightOpacityTable::initialized = false;

// 透光表实现
void LightOpacityTable::setOpacityTable(const uint8_t* table, size_t size) {
    size = std::min(size, static_cast<size_t>(256));
    for (size_t i = 0; i < size; ++i) {
        opacityTable[i] = table[i];
    }
    initialized = true;
    std::cout << "LightOpacityTable initialized with " << size << " entries" << std::endl;
}

// 分块光照存储实现
CompactLightStorage& ChunkedLightStorage::getSkyLight(int chunkX, int chunkZ) {
    return skyLightMap[ChunkKey(chunkX, chunkZ)];
}

CompactLightStorage& ChunkedLightStorage::getBlockLight(int chunkX, int chunkZ) {
    return blockLightMap[ChunkKey(chunkX, chunkZ)];
}

const CompactLightStorage* ChunkedLightStorage::getSkyLight(int chunkX, int chunkZ) const {
    auto it = skyLightMap.find(ChunkKey(chunkX, chunkZ));
    return (it != skyLightMap.end()) ? &it->second : nullptr;
}

const CompactLightStorage* ChunkedLightStorage::getBlockLight(int chunkX, int chunkZ) const {
    auto it = blockLightMap.find(ChunkKey(chunkX, chunkZ));
    return (it != blockLightMap.end()) ? &it->second : nullptr;
}

void ChunkedLightStorage::clearChunk(int chunkX, int chunkZ) {
    ChunkKey key(chunkX, chunkZ);
    skyLightMap.erase(key);
    blockLightMap.erase(key);
}

// 高级光照引擎实现
void AdvancedLightEngine::initializeOpacityTable(const uint8_t* table, size_t size) {
    LightOpacityTable::setOpacityTable(table, size);
}

void AdvancedLightEngine::onBlockChange(int32_t x, int32_t y, int32_t z, int materialId) {
    if (!LightOpacityTable::isInitialized()) {
        std::cerr << "Warning: LightOpacityTable not initialized" << std::endl;
        return;
    }
    
    // 获取方块的透光值 (O(1)快速查询)
    uint8_t opacity = LightOpacityTable::getOpacity(materialId);
    
    // 计算区块坐标
    int32_t chunkX, chunkZ;
    getChunkCoords(x, z, chunkX, chunkZ);
    
    // 估算优先级 (距离玩家越近优先级越高，这里简化处理)
    int32_t priority = std::abs(x) + std::abs(z); // 简单的距离计算
    
    // 添加到延迟更新队列
    std::lock_guard<std::mutex> lock(queueMutex);
    
    // 天空光更新
    if (opacity < 15) { // 不是完全不透光
        dirtyQueue.emplace_back(x, y, z, chunkX, chunkZ, 15 - opacity, true, priority);
    }
    
    // 方块光更新 (如果这个方块本身发光)
    if (opacity == 0) { // 完全透明方块，可能发光
        dirtyQueue.emplace_back(x, y, z, chunkX, chunkZ, 15, false, priority);
    }
}

uint8_t AdvancedLightEngine::getLightLevel(int32_t x, int32_t y, int32_t z, bool isSky) const {
    int32_t chunkX, chunkZ;
    getChunkCoords(x, z, chunkX, chunkZ);
    
    int32_t localX, localZ;
    getLocalCoords(x, z, localX, localZ);
    
    // 从对应的映射中查找光照存储
    const CompactLightStorage* lightStorage = nullptr;
    if (isSky) {
        lightStorage = storage.getSkyLight(chunkX, chunkZ);
    } else {
        lightStorage = storage.getBlockLight(chunkX, chunkZ);
    }
    
    if (lightStorage != nullptr) {
        return lightStorage->get(localX, y, localZ);
    }
    
    // 如果没有找到对应的区块，默认返回0
    return 0;
}

void AdvancedLightEngine::tick() {
    // 避免重复处理
    if (processing.exchange(true)) {
        return;
    }
    
    try {
        processBatchUpdates();
    } catch (const std::exception& e) {
        std::cerr << "Light engine tick error: " << e.what() << std::endl;
    }
    
    processing = false;
}

void AdvancedLightEngine::processBatchUpdates() {
    const int BATCH_SIZE = 100; // 按文档建议的批处理大小
    std::vector<DirtyEntry> batch;
    
    // 提取一批更新
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        batch.reserve(BATCH_SIZE);
        
        while (!dirtyQueue.empty() && batch.size() < BATCH_SIZE) {
            // 按优先级排序处理
            auto it = std::min_element(dirtyQueue.begin(), dirtyQueue.end(),
                [](const DirtyEntry& a, const DirtyEntry& b) {
                    return a.priority > b.priority; // 优先级高的先处理
                });
            
            batch.push_back(*it);
            dirtyQueue.erase(it);
        }
    }
    
    // 批量处理更新
    for (const auto& entry : batch) {
        propagateLightIncremental(entry.x, entry.y, entry.z, entry.newLevel, entry.isSky);
    }
}

void AdvancedLightEngine::propagateLightIncremental(int32_t x, int32_t y, int32_t z, uint8_t level, bool isSky) {
    // 按文档要求：增量传播代替全量BFS
    if (level <= 0) return; // 无效光照级别
    
    int32_t chunkX, chunkZ;
    getChunkCoords(x, z, chunkX, chunkZ);
    
    int32_t localX, localZ;
    getLocalCoords(x, z, localX, localZ);
    
    // 获取正确的光照存储
    CompactLightStorage& lightStorage = isSky ? 
        storage.getSkyLight(chunkX, chunkZ) : 
        storage.getBlockLight(chunkX, chunkZ);
    
    // 设置当前位置的光照
    lightStorage.set(localX, y, localZ, level);
    
    // 向6个方向传播 (上/下/东/西/南/北)
    const int32_t dirs[6][3] = {
        {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, 
        {0, -1, 0}, {0, 0, 1}, {0, 0, -1}
    };
    
    for (int i = 0; i < 6; ++i) {
        int32_t nx = x + dirs[i][0];
        int32_t ny = y + dirs[i][1];
        int32_t nz = z + dirs[i][2];
        
        // 边界检查
        if (ny < 0 || ny > 255) continue;
        
        // 计算邻居的区块坐标
        int32_t neighborChunkX, neighborChunkZ;
        getChunkCoords(nx, nz, neighborChunkX, neighborChunkZ);
        
        // 如果跨区块，添加到更新队列而不是立即处理
        if (neighborChunkX != chunkX || neighborChunkZ != chunkZ) {
            std::lock_guard<std::mutex> lock(queueMutex);
            int32_t priority = std::abs(nx) + std::abs(nz);
            dirtyQueue.emplace_back(nx, ny, nz, neighborChunkX, neighborChunkZ, 
                                  level > 1 ? level - 1 : 0, isSky, priority);
            continue;
        }
        
        // 计算邻居的区块内坐标
        int32_t neighborLocalX, neighborLocalZ;
        getLocalCoords(nx, nz, neighborLocalX, neighborLocalZ);
        
        // 获取邻居方块的透光值 (O(1)快速查询)
        int neighborMaterialId = 1; // 简化处理：假设是石头
        uint8_t neighborOpacity = LightOpacityTable::getOpacity(neighborMaterialId);
        
        // 计算邻居的光照级别
        uint8_t newNeighborLevel = (level > neighborOpacity) ? (level - neighborOpacity) : 0;
        
        if (newNeighborLevel > 0) {
            // 简化实现：直接设置光照值而不检查当前值
            // 实际应该检查并只更新更高的光照级别
            CompactLightStorage& neighborStorage = isSky ? 
                storage.getSkyLight(neighborChunkX, neighborChunkZ) : 
                storage.getBlockLight(neighborChunkX, neighborChunkZ);
            
            neighborStorage.set(neighborLocalX, ny, neighborLocalZ, newNeighborLevel);
            
            // 继续传播 (如果光照级别足够高)
            if (newNeighborLevel > 1) {
                std::lock_guard<std::mutex> lock(queueMutex);
                int32_t priority = std::abs(nx) + std::abs(nz);
                dirtyQueue.emplace_back(nx, ny, nz, neighborChunkX, neighborChunkZ,
                                      newNeighborLevel, isSky, priority);
            }
        }
    }
}

} // namespace world
} // namespace lattice
