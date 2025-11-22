#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include <queue>
#include <unordered_set>
#include <unordered_map>
#include <functional>

// 前向声明JNI相关类型
class JNIEnv;
class jobject;
class JavaVM;

namespace lattice {
namespace world {

// 全局变量声明
// JNI相关变量声明 - 注释掉以避免编译错误
// extern JavaVM* g_jvm;                 // JVM引用
// extern jobject g_worldObject;         // World对象引用
// extern jclass g_blockPosClass;        // BlockPos类引用
// extern jmethodID g_blockPosConstructor; // BlockPos构造函数
// extern jclass g_worldClass;           // World类引用
// extern jmethodID g_getBlockStateMethod; // World.getBlockState方法
// extern jclass g_blockStateClass;      // BlockState类引用
// extern jmethodID g_getBlockMethod;    // BlockState.getBlock方法
// extern jclass g_blockClass;           // Block类引用
// extern jfieldID g_blockMaterialField; // Block.material字段
// extern jclass g_materialClass;        // Material类引用
// extern jmethodID g_isTransparentMethod; // Material.isTransparent方法

// 光照更新方向
enum class LightPropagationDirection {
    INCREASE,  // 增加光照
    DECREASE   // 减少光照
};

// 光照类型
enum class LightType {
    BLOCK,     // 方块光
    SKY        // 天空光
};

// 位置结构
struct BlockPos {
    int32_t x, y, z;
    
    BlockPos() : x(0), y(0), z(0) {}
    BlockPos(int32_t x, int32_t y, int32_t z) : x(x), y(y), z(z) {}
    
    bool operator==(const BlockPos& other) const {
        return x == other.x && y == other.y && z == other.z;
    }
};

// 自定义哈希函数用于BlockPos
struct BlockPosHash {
    std::size_t operator()(const BlockPos& pos) const {
        // 使用简单的哈希组合
        return std::hash<int32_t>{}(pos.x) ^ 
               (std::hash<int32_t>{}(pos.y) << 1) ^ 
               (std::hash<int32_t>{}(pos.z) << 2);
    }
};

// 光照更新任务
struct LightUpdateTask {
    BlockPos pos;
    uint8_t lightLevel;
    LightPropagationDirection direction;
    LightType type;
    
    LightUpdateTask(const BlockPos& pos, uint8_t lightLevel, LightPropagationDirection direction, LightType type = LightType::BLOCK)
        : pos(pos), lightLevel(lightLevel), direction(direction), type(type) {}
};

// 高性能光照更新器
class LightUpdater {
public:
    LightUpdater();
    ~LightUpdater();

    // 设置方块光级别
    void setLightLevel(const BlockPos& pos, uint8_t level, LightType type);
    
    // 获取方块光级别
    uint8_t getLightLevel(const BlockPos& pos, LightType type) const;
    
    // 添加光照源
    void addLightSource(const BlockPos& pos, uint8_t level, LightType type);
    
    // 移除光照源
    void removeLightSource(const BlockPos& pos, LightType type);
    
    // 执行光照更新
    void propagateLightUpdates();
    
    // 检查是否需要更新
    bool hasUpdates() const;

private:
    // 光照数据存储
    using LightMap = std::unordered_map<BlockPos, uint8_t, BlockPosHash>;
    
    LightMap blockLightMap;
    LightMap skyLightMap;
    LightMap blockLightSources;
    LightMap skyLightSources;
    
    // 更新队列
    std::queue<LightUpdateTask> updateQueue;
    
    // 需要更新的位置集合
    std::unordered_set<BlockPos, BlockPosHash> pendingPositions;
    
    // 获取相邻方块
    std::vector<BlockPos> getNeighbors(const BlockPos& pos) const;
    
    // 传播光照增加
    void propagateIncrease(const BlockPos& pos, uint8_t level, LightType type);
    
    // 传播光照减少
    void propagateDecrease(const BlockPos& pos, uint8_t level, LightType type);
    
    // 获取光照阻隔级别（0-15，数字越大阻隔越强）
    uint8_t getOpacity(const BlockPos& pos) const;
    
    // 检查方块是否为空气/可穿透
    bool isTransparent(const BlockPos& pos) const;
    
    // 从世界数据中获取方块类型
    int getBlockTypeFromWorldData(const BlockPos& pos) const;
};

} // namespace world
} // namespace lattice