#include "light_updater.hpp"
#include <algorithm>
#include <cassert>
#include <queue>
#include <unordered_map>
#include <jni.h> // 添加JNI头文件

namespace lattice {
namespace world {

LightUpdater::LightUpdater() {
    // 初始化光照更新器
}

LightUpdater::~LightUpdater() {
    // 清理资源
}

void LightUpdater::setLightLevel(const BlockPos& pos, uint8_t level, LightType type) {
    LightMap& lightMap = (type == LightType::BLOCK) ? blockLightMap : skyLightMap;
    if (level > 0) {
        lightMap[pos] = level;
    } else {
        lightMap.erase(pos);
    }
}

uint8_t LightUpdater::getLightLevel(const BlockPos& pos, LightType type) const {
    const LightMap& lightMap = (type == LightType::BLOCK) ? blockLightMap : skyLightMap;
    auto it = lightMap.find(pos);
    return (it != lightMap.end()) ? it->second : 0;
}

void LightUpdater::addLightSource(const BlockPos& pos, uint8_t level, LightType type) {
    LightMap& lightSources = (type == LightType::BLOCK) ? blockLightSources : skyLightSources;
    LightMap& lightMap = (type == LightType::BLOCK) ? blockLightMap : skyLightMap;
    
    // 更新或移除光源
    if (level > 0) {
        lightSources[pos] = level;
        
        // 添加到更新队列
        updateQueue.emplace(pos, level, LightPropagationDirection::INCREASE, type);
        pendingPositions.insert(pos);
        
        // 直接设置光源位置的光照级别
        lightMap[pos] = level;
    } else {
        removeLightSource(pos, type);
    }
}

void LightUpdater::removeLightSource(const BlockPos& pos, LightType type) {
    LightMap& lightSources = (type == LightType::BLOCK) ? blockLightSources : skyLightSources;
    LightMap& lightMap = (type == LightType::BLOCK) ? blockLightMap : skyLightMap;
    
    auto sourceIt = lightSources.find(pos);
    if (sourceIt != lightSources.end()) {
        uint8_t oldLevel = sourceIt->second;
        lightSources.erase(sourceIt);
        
        // 将光源移除添加到更新队列
        updateQueue.emplace(pos, oldLevel, LightPropagationDirection::DECREASE);
        pendingPositions.insert(pos);
        
        // 移除光源位置的光照级别
        lightMap.erase(pos);
    }
}

void LightUpdater::propagateLightUpdates() {
    // 处理所有排队的更新
    while (!updateQueue.empty()) {
        LightUpdateTask task = updateQueue.front();
        updateQueue.pop();
        
        pendingPositions.erase(task.pos);
        
        if (task.direction == LightPropagationDirection::INCREASE) {
            // 处理光照增加
            // 检查当前级别是否仍然有效
            uint8_t currentLevel = getLightLevel(task.pos, LightType::BLOCK); // 简化处理，只处理方块光
            if (currentLevel >= task.lightLevel) {
                propagateIncrease(task.pos, task.lightLevel, LightType::BLOCK);
            }
        } else {
            // 处理光照减少
            propagateDecrease(task.pos, task.lightLevel, LightType::BLOCK);
        }
    }
}

bool LightUpdater::hasUpdates() const {
    return !updateQueue.empty() || !pendingPositions.empty();
}

std::vector<BlockPos> LightUpdater::getNeighbors(const BlockPos& pos) const {
    std::vector<BlockPos> neighbors;
    neighbors.reserve(6);
    
    // 添加6个直接相邻的方块
    neighbors.emplace_back(pos.x + 1, pos.y, pos.z);
    neighbors.emplace_back(pos.x - 1, pos.y, pos.z);
    neighbors.emplace_back(pos.x, pos.y + 1, pos.z);
    neighbors.emplace_back(pos.x, pos.y - 1, pos.z);
    neighbors.emplace_back(pos.x, pos.y, pos.z + 1);
    neighbors.emplace_back(pos.x, pos.y, pos.z - 1);
    
    return neighbors;
}

void LightUpdater::propagateIncrease(const BlockPos& pos, uint8_t level, LightType type) {
    if (level <= 1) return; // 无法再传播更低的光照
    
    LightMap& lightMap = (type == LightType::BLOCK) ? blockLightMap : skyLightMap;
    
    // 获取相邻方块
    std::vector<BlockPos> neighbors = getNeighbors(pos);
    
    for (const BlockPos& neighborPos : neighbors) {
        // 计算该位置的光照阻隔
        uint8_t opacity = getOpacity(neighborPos);
        uint8_t newLevel = (level > opacity) ? (level - opacity) : 0;
        
        // 检查是否需要更新此位置的光照
        uint8_t currentLevel = getLightLevel(neighborPos, type);
        if (newLevel > currentLevel) {
            // 更新光照级别
            lightMap[neighborPos] = newLevel;
            
            // 继续传播
            if (newLevel > 1) {
                updateQueue.emplace(neighborPos, newLevel, LightPropagationDirection::INCREASE);
                pendingPositions.insert(neighborPos);
            }
        }
    }
}

void LightUpdater::propagateDecrease(const BlockPos& pos, uint8_t level, LightType type) {
    LightMap& lightMap = (type == LightType::BLOCK) ? blockLightMap : skyLightMap;
    
    // 获取相邻方块
    std::vector<BlockPos> neighbors = getNeighbors(pos);
    
    // 使用BFS队列处理光照减少传播
    std::queue<std::pair<BlockPos, uint8_t>> bfsQueue;
    std::unordered_set<BlockPos, BlockPosHash> visited;
    
    bfsQueue.emplace(pos, level);
    visited.insert(pos);
    
    while (!bfsQueue.empty()) {
        auto [currentPos, currentLevel] = bfsQueue.front();
        bfsQueue.pop();
        
        std::vector<BlockPos> currentNeighbors = getNeighbors(currentPos);
        
        for (const BlockPos& neighborPos : currentNeighbors) {
            // 避免重复访问
            if (visited.find(neighborPos) != visited.end()) {
                continue;
            }
            visited.insert(neighborPos);
            
            uint8_t neighborLevel = getLightLevel(neighborPos, type);
            
            // 如果相邻方块的光照级别小于等于当前级别，需要重新计算
            if (neighborLevel != 0 && neighborLevel <= currentLevel) {
                // 检查是否还有其他光源可以维持该位置的光照
                uint8_t maintainedLevel = 0;
                
                // 检查相邻方块是否可以提供光照
                std::vector<BlockPos> neighborNeighbors = getNeighbors(neighborPos);
                for (const BlockPos& neighborNeighborPos : neighborNeighbors) {
                    uint8_t neighborNeighborLevel = getLightLevel(neighborNeighborPos, type);
                    if (neighborNeighborLevel > 0) {
                        uint8_t opacity = getOpacity(neighborPos);
                        uint8_t potentialLevel = (neighborNeighborLevel > opacity) ? (neighborNeighborLevel - opacity) : 0;
                        maintainedLevel = std::max(maintainedLevel, potentialLevel);
                    }
                }
                
                // 检查光源
                LightMap& lightSources = (type == LightType::BLOCK) ? blockLightSources : skyLightSources;
                auto sourceIt = lightSources.find(neighborPos);
                if (sourceIt != lightSources.end()) {
                    maintainedLevel = std::max(maintainedLevel, sourceIt->second);
                }
                
                // 如果无法维持当前光照级别，则移除
                if (maintainedLevel < neighborLevel) {
                    lightMap.erase(neighborPos);
                    
                    // 如果该位置的光照级别大于1，继续传播减少
                    if (neighborLevel > 1) {
                        bfsQueue.emplace(neighborPos, neighborLevel);
                    }
                } else if (maintainedLevel > neighborLevel) {
                    // 如果可以维持更高的光照级别，更新并传播增加
                    lightMap[neighborPos] = maintainedLevel;
                    updateQueue.emplace(neighborPos, maintainedLevel, LightPropagationDirection::INCREASE);
                    pendingPositions.insert(neighborPos);
                }
            }
        }
    }
}

uint8_t LightUpdater::getOpacity(const BlockPos& pos) const {
    // 根据Minecraft 1.21.8的光照系统，透明方块（如空气）阻隔为0-1，不透明方块阻隔为15
    // 实际实现中会根据方块类型返回具体的阻隔值
    if (isTransparent(pos)) {
        // 空气等略微减少光照（根据Minecraft标准，透明方块通常减少1级光照）
        return 1; 
    } else {
        // 不透明方块完全阻隔光照
        return 15; 
    }
}

bool LightUpdater::isTransparent(const BlockPos& pos) const {
    // 通过JNI调用访问真实的Minecraft世界数据来获取方块透明度
    
    // 获取当前线程的JNI环境
    JNIEnv* env = nullptr;
    if (!g_jvm) {
        // JVM未初始化，返回默认值
        return true; // 默认返回透明（如空气方块）
    }
    
    // 获取当前线程的JNI环境
    jint result = g_jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    if (result != JNI_OK) {
        // 如果当前线程没有JNI环境，尝试将其附加到JVM
        result = g_jvm->AttachCurrentThread(reinterpret_cast<void**>(&env), nullptr);
        if (result != JNI_OK) {
            return true; // 无法获取JNI环境，返回默认值
        }
    }
    
    // 检查必要的类和方法引用是否已缓存
    if (!g_blockPosClass || !g_blockPosConstructor || !g_worldObject || 
        !g_getBlockStateMethod || !g_getBlockMethod || 
        !g_blockMaterialField || !g_isTransparentMethod) {
        return true; // 必要的引用未初始化，返回默认值
    }
    
    // 创建BlockPos对象
    jobject blockPos = env->NewObject(g_blockPosClass, g_blockPosConstructor, pos.x, pos.y, pos.z);
    if (!blockPos) {
        // 创建对象失败
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
        }
        return true;
    }
    
    // 调用world.getBlockState(blockPos)方法
    jobject blockState = env->CallObjectMethod(g_worldObject, g_getBlockStateMethod, blockPos);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        env->DeleteLocalRef(blockPos);
        return true;
    }
    
    // 检查blockState是否为null
    if (!blockState) {
        env->DeleteLocalRef(blockPos);
        return true; // 空的方块状态，返回默认值
    }
    
    // 调用blockState.getBlock()方法
    jobject block = env->CallObjectMethod(blockState, g_getBlockMethod);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        env->DeleteLocalRef(blockState);
        env->DeleteLocalRef(blockPos);
        return true;
    }
    
    // 检查block是否为null
    if (!block) {
        env->DeleteLocalRef(blockState);
        env->DeleteLocalRef(blockPos);
        return true; // 空的方块，返回默认值
    }
    
    // 获取block的material字段
    jobject material = env->GetObjectField(block, g_blockMaterialField);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        env->DeleteLocalRef(block);
        env->DeleteLocalRef(blockState);
        env->DeleteLocalRef(blockPos);
        return true;
    }
    
    // 检查material是否为null
    if (!material) {
        env->DeleteLocalRef(block);
        env->DeleteLocalRef(blockState);
        env->DeleteLocalRef(blockPos);
        return true; // 空的材质，返回默认值
    }
    
    // 调用material.isTransparent()方法
    jboolean isTransparent = env->CallBooleanMethod(material, g_isTransparentMethod);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        env->DeleteLocalRef(material);
        env->DeleteLocalRef(block);
        env->DeleteLocalRef(blockState);
        env->DeleteLocalRef(blockPos);
        return true;
    }
    
    // 清理局部引用
    env->DeleteLocalRef(material);
    env->DeleteLocalRef(block);
    env->DeleteLocalRef(blockState);
    env->DeleteLocalRef(blockPos);
    
    // 返回透明度检查结果
    return static_cast<bool>(isTransparent);
}

int LightUpdater::getBlockTypeFromWorldData(const BlockPos& pos) const {
    // 通过JNI调用访问真实的Minecraft世界数据来获取方块类型
    // 注意：这个函数需要一个指向世界对象的引用，实际实现中应该通过参数传递
    
    // 获取当前线程的JNI环境
    JNIEnv* env = nullptr;
    if (!g_jvm) {
        // JVM未初始化，返回默认值
        return 0; // 默认返回空气方块类型
    }
    
    // 获取当前线程的JNI环境
    jint result = g_jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    if (result != JNI_OK) {
        // 如果当前线程没有JNI环境，尝试将其附加到JVM
        result = g_jvm->AttachCurrentThread(reinterpret_cast<void**>(&env), nullptr);
        if (result != JNI_OK) {
            return 0; // 无法获取JNI环境，返回默认值
        }
    }
    
    // 检查必要的类和方法引用是否已缓存
    if (!g_blockPosClass || !g_blockPosConstructor || !g_worldObject) {
        return 0; // 必要的引用未初始化，返回默认值
    }
    
    // 创建BlockPos对象
    jobject blockPos = env->NewObject(g_blockPosClass, g_blockPosConstructor, pos.x, pos.y, pos.z);
    if (!blockPos) {
        // 创建对象失败
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
        }
        return 0;
    }
    
    // 调用world.getBlockState(blockPos)方法
    jobject blockState = env->CallObjectMethod(g_worldObject, g_getBlockStateMethod, blockPos);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        env->DeleteLocalRef(blockPos);
        return 0;
    }
    
    // 检查blockState是否为null
    if (!blockState) {
        env->DeleteLocalRef(blockPos);
        return 0; // 空的方块状态，返回默认值
    }
    
    // 调用blockState.getBlock()方法
    jobject block = env->CallObjectMethod(blockState, g_getBlockMethod);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        env->DeleteLocalRef(blockState);
        env->DeleteLocalRef(blockPos);
        return 0;
    }
    
    // 检查block是否为null
    if (!block) {
        env->DeleteLocalRef(blockState);
        env->DeleteLocalRef(blockPos);
        return 0; // 空的方块，返回默认值
    }
    
    // 获取block的material字段
    jobject material = env->GetObjectField(block, g_blockMaterialField);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        env->DeleteLocalRef(block);
        env->DeleteLocalRef(blockState);
        env->DeleteLocalRef(blockPos);
        return 0;
    }
    
    // 检查material是否为null
    if (!material) {
        env->DeleteLocalRef(block);
        env->DeleteLocalRef(blockState);
        env->DeleteLocalRef(blockPos);
        return 0; // 空的材质，返回默认值
    }
    
    // 调用material.isTransparent()方法
    jboolean isTransparent = env->CallBooleanMethod(material, g_isTransparentMethod);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        env->DeleteLocalRef(material);
        env->DeleteLocalRef(block);
        env->DeleteLocalRef(blockState);
        env->DeleteLocalRef(blockPos);
        return 0;
    }
    
    // 清理局部引用
    env->DeleteLocalRef(material);
    env->DeleteLocalRef(block);
    env->DeleteLocalRef(blockState);
    env->DeleteLocalRef(blockPos);
    
    // 根据isTransparent返回适当的方块类型
    // 0表示透明方块(如空气)，1表示不透明方块(如石头)
    return isTransparent ? 0 : 1;
}

} // namespace world
} // namespace lattice