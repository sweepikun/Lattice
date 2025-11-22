#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include <unordered_map>
#include <array>
#include <atomic>

namespace lattice {
namespace world {

// 4-bit位压缩光照存储 - 按文档要求实现
class CompactLightStorage {
private:
    std::vector<uint8_t> data; // 压缩数据：每字节存2个光照值
    
public:
    CompactLightStorage() : data(2048, 0) {} // 16x16x16 = 4096方块 / 2 = 2048字节
    
    // 快速获取光照值 (O(1))
    inline uint8_t get(int x, int y, int z) const {
        int index = (y << 8) | (z << 4) | x; // 快速索引计算
        uint8_t byte = data[index >> 1];
        return (index & 1) ? (byte >> 4) : (byte & 0x0F);
    }
    
    // 快速设置光照值 (O(1)) 
    inline void set(int x, int y, int z, uint8_t value) {
        int index = (y << 8) | (z << 4) | x;
        int byteIndex = index >> 1;
        if (index & 1) {
            data[byteIndex] = (data[byteIndex] & 0x0F) | ((value & 0x0F) << 4);
        } else {
            data[byteIndex] = (data[byteIndex] & 0xF0) | (value & 0x0F);
        }
    }
    
    // 清除所有光照
    void clear() {
        std::fill(data.begin(), data.end(), 0);
    }
};

// 透光表 - 按文档要求实现
class LightOpacityTable {
private:
    static std::array<uint8_t, 256> opacityTable; // Material ID -> 透光值 (0-15)
    static bool initialized;
    
public:
    // 启动时从Java层设置透光表
    static void setOpacityTable(const uint8_t* table, size_t size);
    
    // 快速查询透光值 (O(1))
    static inline uint8_t getOpacity(int materialId) {
        if (materialId >= 0 && materialId < 256 && initialized) {
            return opacityTable[materialId];
        }
        return 15; // 默认不透明
    }
    
    // 检查是否已初始化
    static inline bool isInitialized() {
        return initialized;
    }
};

// 分块光照存储 - 按文档要求实现
class ChunkedLightStorage {
private:
    struct ChunkKey {
        int32_t x, z;
        
        ChunkKey(int32_t cx, int32_t cz) : x(cx), z(cz) {}
        
        bool operator==(const ChunkKey& other) const {
            return x == other.x && z == other.z;
        }
    };
    
    struct ChunkKeyHash {
        std::size_t operator()(const ChunkKey& key) const {
            return std::hash<int32_t>{}(key.x) ^ (std::hash<int32_t>{}(key.z) << 1);
        }
    };
    
    std::unordered_map<ChunkKey, CompactLightStorage, ChunkKeyHash> skyLightMap;
    std::unordered_map<ChunkKey, CompactLightStorage, ChunkKeyHash> blockLightMap;
    
public:
    // 获取指定区块的光照存储
    CompactLightStorage& getSkyLight(int chunkX, int chunkZ);
    CompactLightStorage& getBlockLight(int chunkX, int chunkZ);
    
    // Const版本的访问方法
    const CompactLightStorage* getSkyLight(int chunkX, int chunkZ) const;
    const CompactLightStorage* getBlockLight(int chunkX, int chunkZ) const;
    
    // 清除指定区块的光照
    void clearChunk(int chunkX, int chunkZ);
};

// 高级光照引擎 - 文档要求的完整实现
class AdvancedLightEngine {
private:
    ChunkedLightStorage storage;
    
    // 延迟更新队列 - 按文档要求实现
    struct DirtyEntry {
        int32_t x, y, z;
        int32_t chunkX, chunkZ;
        uint8_t newLevel;
        bool isSky;
        int32_t priority; // 距离玩家越近优先级越高
        
        DirtyEntry(int32_t px, int32_t py, int32_t pz, int32_t cx, int32_t cz, 
                  uint8_t level, bool sky, int32_t pri)
            : x(px), y(py), z(pz), chunkX(cx), chunkZ(cz), 
              newLevel(level), isSky(sky), priority(pri) {}
    };
    
    std::vector<DirtyEntry> dirtyQueue;
    std::mutex queueMutex;
    std::atomic<bool> processing{false};
    
public:
    // 初始化透光表
    static void initializeOpacityTable(const uint8_t* table, size_t size);
    
    // 方块变化处理 - 按文档要求
    void onBlockChange(int32_t x, int32_t y, int32_t z, int materialId);
    
    // 每tick调用 - 处理批量更新
    void tick();
    
    // 获取光照值 (O(1)快速查询)
    uint8_t getLightLevel(int32_t x, int32_t y, int32_t z, bool isSky) const;
    
private:
    // 计算区块坐标
    static inline void getChunkCoords(int32_t x, int32_t z, int32_t& chunkX, int32_t& chunkZ) {
        chunkX = x >> 4; // x / 16
        chunkZ = z >> 4; // z / 16
    }
    
    // 计算区块内坐标
    static inline void getLocalCoords(int32_t x, int32_t z, int32_t& localX, int32_t& localZ) {
        localX = x & 15; // x % 16
        localZ = z & 15; // z % 16
    }
    
    // 增量光照传播
    void propagateLightIncremental(int32_t x, int32_t y, int32_t z, uint8_t level, bool isSky);
    
    // 批量处理更新
    void processBatchUpdates();
};

} // namespace world
} // namespace lattice
