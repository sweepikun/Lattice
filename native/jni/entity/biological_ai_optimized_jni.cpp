#include "biological_ai_optimized_jni.hpp"
#include <chrono>
#include <cstring>

namespace lattice {
namespace jni {
namespace entity {

// 初始化性能统计
std::atomic<uint64_t> BiologicalAIJNIOptimized::totalOperations_{0};
std::atomic<uint64_t> BiologicalAIJNIOptimized::totalTimeMicros_{0};
std::atomic<uint64_t> BiologicalAIJNIOptimized::entitiesProcessed_{0};
std::atomic<uint64_t> BiologicalAIJNIOptimized::memoryUsage_{0};

jlong BiologicalAIJNIOptimized::initializeAIEngine(JNIEnv* env, jstring minecraftDataPath, jstring gameVersion) {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // 1. 参数验证
    if (minecraftDataPath == nullptr || gameVersion == nullptr) {
        return 0;
    }
    
    try {
        // 2. 转换为C++字符串
        const char* dataPath = env->GetStringUTFChars(minecraftDataPath, nullptr);
        const char* version = env->GetStringUTFChars(gameVersion, nullptr);
        
        if (!dataPath || !version) {
            if (dataPath) env->ReleaseStringUTFChars(minecraftDataPath, dataPath);
            if (version) env->ReleaseStringUTFChars(gameVersion, version);
            return 0;
        }
        
        std::string pathStr(dataPath);
        std::string versionStr(version);
        
        // 3. 创建AI引擎实例（桥接到core）
        auto* aiEngine = new lattice::entity::AIEngine();
        
        // 4. 初始化AI引擎
        bool success = aiEngine->initialize(pathStr, versionStr);
        if (!success) {
            delete aiEngine;
            aiEngine = nullptr;
        }
        
        // 5. 释放字符串
        env->ReleaseStringUTFChars(minecraftDataPath, dataPath);
        env->ReleaseStringUTFChars(gameVersion, version);
        
        // 6. 返回引擎指针
        return reinterpret_cast<jlong>(aiEngine);
        
    } catch (const std::exception& e) {
        // 7. 错误处理
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), e.what());
        return 0;
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    logPerformanceStats("initializeAIEngine", 
                       std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count());
}

void BiologicalAIJNIOptimized::shutdownAIEngine(JNIEnv* env, jlong enginePtr) {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // 1. 参数验证
    if (enginePtr == 0) {
        return;
    }
    
    try {
        // 2. 获取AI引擎实例
        auto* aiEngine = reinterpret_cast<lattice::entity::AIEngine*>(enginePtr);
        if (aiEngine) {
            // 3. 关闭AI引擎
            aiEngine->shutdown();
            
            // 4. 清理内存
            delete aiEngine;
        }
        
    } catch (const std::exception& e) {
        // 5. 错误处理
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), e.what());
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    logPerformanceStats("shutdownAIEngine", 
                       std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count());
}

jboolean BiologicalAIJNIOptimized::registerEntity(JNIEnv* env, jlong enginePtr, jlong entityId, jstring entityType) {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // 1. 参数验证
    if (enginePtr == 0 || entityId <= 0 || entityType == nullptr) {
        return JNI_FALSE;
    }
    
    try {
        // 2. 转换为C++字符串
        const char* typeStr = env->GetStringUTFChars(entityType, nullptr);
        if (!typeStr) {
            return JNI_FALSE;
        }
        
        std::string entityTypeStr(typeStr);
        env->ReleaseStringUTFChars(entityType, typeStr);
        
        // 3. 获取AI引擎实例
        auto* aiEngine = reinterpret_cast<lattice::entity::AIEngine*>(enginePtr);
        if (!aiEngine) {
            return JNI_FALSE;
        }
        
        // 4. 注册实体到AI引擎
        bool success = aiEngine->registerEntity(static_cast<uint64_t>(entityId), entityTypeStr);
        
        // 5. 更新统计
        if (success) {
            entitiesProcessed_++;
        }
        
        return success ? JNI_TRUE : JNI_FALSE;
        
    } catch (const std::exception& e) {
        // 6. 错误处理
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), e.what());
        return JNI_FALSE;
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    logPerformanceStats("registerEntity", 
                       std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count());
}

void BiologicalAIJNIOptimized::unregisterEntity(JNIEnv* env, jlong enginePtr, jlong entityId) {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // 1. 参数验证
    if (enginePtr == 0 || entityId <= 0) {
        return;
    }
    
    try {
        // 2. 获取AI引擎实例
        auto* aiEngine = reinterpret_cast<lattice::entity::AIEngine*>(enginePtr);
        if (aiEngine) {
            // 3. 移除实体
            aiEngine->unregisterEntity(static_cast<uint64_t>(entityId));
        }
        
    } catch (const std::exception& e) {
        // 4. 错误处理
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), e.what());
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    logPerformanceStats("unregisterEntity", 
                       std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count());
}

void BiologicalAIJNIOptimized::updateEntityState(JNIEnv* env, jlong enginePtr, jlong entityId, 
                                                jfloat x, jfloat y, jfloat z, jfloat health, jboolean isAlive) {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // 1. 参数验证
    if (enginePtr == 0 || entityId <= 0) {
        return;
    }
    
    try {
        // 2. 转换实体状态
        lattice::entity::EntityState state;
        convertEntityState(env, entityId, x, y, z, health, isAlive, state);
        
        // 3. 获取AI引擎实例
        auto* aiEngine = reinterpret_cast<lattice::entity::AIEngine*>(enginePtr);
        if (aiEngine) {
            // 4. 更新实体状态
            aiEngine->updateEntityState(static_cast<uint64_t>(entityId), state);
        }
        
    } catch (const std::exception& e) {
        // 5. 错误处理
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), e.what());
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    logPerformanceStats("updateEntityState", 
                       std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count());
}

void BiologicalAIJNIOptimized::updateWorldView(JNIEnv* env, jlong enginePtr, jlong entityId, 
                                              jlong worldViewPtr) {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // 1. 参数验证
    if (enginePtr == 0 || entityId <= 0 || worldViewPtr == 0) {
        return;
    }
    
    try {
        // 2. 转换世界视图
        lattice::entity::WorldView worldView;
        convertWorldView(env, worldViewPtr, worldView);
        
        // 3. 获取AI引擎实例
        auto* aiEngine = reinterpret_cast<lattice::entity::AIEngine*>(enginePtr);
        if (aiEngine) {
            // 4. 更新世界视图
            aiEngine->updateWorldView(static_cast<uint64_t>(entityId), worldView);
        }
        
    } catch (const std::exception& e) {
        // 5. 错误处理
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), e.what());
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    logPerformanceStats("updateWorldView", 
                       std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count());
}

void BiologicalAIJNIOptimized::executeAIDecision(JNIEnv* env, jlong enginePtr, jlong entityId, jlong deltaTime) {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // 1. 参数验证
    if (enginePtr == 0 || entityId <= 0) {
        return;
    }
    
    try {
        // 2. 获取AI引擎实例
        auto* aiEngine = reinterpret_cast<lattice::entity::AIEngine*>(enginePtr);
        if (aiEngine) {
            // 3. 执行实体AI决策
            aiEngine->tickEntity(static_cast<uint64_t>(entityId), static_cast<uint64_t>(deltaTime));
        }
        
    } catch (const std::exception& e) {
        // 4. 错误处理
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), e.what());
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    logPerformanceStats("executeAIDecision", 
                       std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count());
}

void BiologicalAIJNIOptimized::batchProcessEntities(JNIEnv* env, jlong enginePtr, jlongArray entityIds, jlong deltaTime) {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // 1. 参数验证
    if (enginePtr == 0 || entityIds == nullptr) {
        return;
    }
    
    jsize entityCount = env->GetArrayLength(entityIds);
    if (entityCount <= 0) {
        return;
    }
    
    // 2. 获取实体ID数组
    jlong* ids = env->GetLongArrayElements(entityIds, nullptr);
    if (!ids) {
        return;
    }
    
    try {
        // 3. 获取AI引擎实例
        auto* aiEngine = reinterpret_cast<lattice::entity::AIEngine*>(enginePtr);
        if (aiEngine) {
            // 4. 批量处理每个实体
            for (jsize i = 0; i < entityCount; i++) {
                if (ids[i] > 0) {
                    aiEngine->tickEntity(static_cast<uint64_t>(ids[i]), static_cast<uint64_t>(deltaTime));
                }
            }
        }
        
    } catch (const std::exception& e) {
        // 5. 错误处理
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), e.what());
    } finally {
        // 6. 释放Java数组
        env->ReleaseLongArrayElements(entityIds, ids, 0);
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    logPerformanceStats("batchProcessEntities", 
                       std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count());
}

void BiologicalAIJNIOptimized::tick(JNIEnv* env, jlong enginePtr) {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // 1. 参数验证
    if (enginePtr == 0) {
        return;
    }
    
    try {
        // 2. 获取AI引擎实例
        auto* aiEngine = reinterpret_cast<lattice::entity::AIEngine*>(enginePtr);
        if (aiEngine) {
            // 3. 执行AI引擎主循环
            aiEngine->tick();
        }
        
    } catch (const std::exception& e) {
        // 4. 错误处理
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), e.what());
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    logPerformanceStats("tick", 
                       std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count());
}

jlong BiologicalAIJNIOptimized::getEntityConfig(JNIEnv* env, jlong enginePtr, jlong entityId) {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // 1. 参数验证
    if (enginePtr == 0 || entityId <= 0) {
        return 0;
    }
    
    try {
        // 2. 获取AI引擎实例
        auto* aiEngine = reinterpret_cast<lattice::entity::AIEngine*>(enginePtr);
        if (!aiEngine) {
            return 0;
        }
        
        // 3. 获取实体配置
        auto* config = aiEngine->getEntityConfig(static_cast<uint64_t>(entityId));
        if (config) {
            // 4. 返回配置指针（简化实现）
            return reinterpret_cast<jlong>(config);
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        // 5. 错误处理
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), e.what());
        return 0;
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    logPerformanceStats("getEntityConfig", 
                       std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count());
}

jlong BiologicalAIJNIOptimized::getTotalTicks(JNIEnv* env, jlong enginePtr) {
    // 1. 参数验证
    if (enginePtr == 0) {
        return 0;
    }
    
    try {
        // 2. 获取AI引擎实例
        auto* aiEngine = reinterpret_cast<lattice::entity::AIEngine*>(enginePtr);
        if (aiEngine) {
            // 3. 获取性能统计
            const auto& stats = aiEngine->getPerformanceStats();
            return static_cast<jlong>(stats.totalTicks);
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        // 4. 错误处理
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), e.what());
        return 0;
    }
}

jlong BiologicalAIJNIOptimized::getAverageTickTimeMicros(JNIEnv* env, jlong enginePtr) {
    // 1. 参数验证
    if (enginePtr == 0) {
        return 0;
    }
    
    try {
        // 2. 获取AI引擎实例
        auto* aiEngine = reinterpret_cast<lattice::entity::AIEngine*>(enginePtr);
        if (aiEngine) {
            // 3. 获取性能统计
            const auto& stats = aiEngine->getPerformanceStats();
            return static_cast<jlong>(stats.avgTickTime);
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        // 4. 错误处理
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), e.what());
        return 0;
    }
}

jlong BiologicalAIJNIOptimized::getMaxTickTimeMicros(JNIEnv* env, jlong enginePtr) {
    // 1. 参数验证
    if (enginePtr == 0) {
        return 0;
    }
    
    try {
        // 2. 获取AI引擎实例
        auto* aiEngine = reinterpret_cast<lattice::entity::AIEngine*>(enginePtr);
        if (aiEngine) {
            // 3. 获取性能统计
            const auto& stats = aiEngine->getPerformanceStats();
            return static_cast<jlong>(stats.maxTickTime);
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        // 4. 错误处理
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), e.what());
        return 0;
    }
}

jlong BiologicalAIJNIOptimized::getEntitiesProcessed(JNIEnv* env, jlong enginePtr) {
    // 1. 参数验证
    if (enginePtr == 0) {
        return 0;
    }
    
    try {
        // 2. 获取AI引擎实例
        auto* aiEngine = reinterpret_cast<lattice::entity::AIEngine*>(enginePtr);
        if (aiEngine) {
            // 3. 获取性能统计
            const auto& stats = aiEngine->getPerformanceStats();
            return static_cast<jlong>(stats.entitiesProcessed);
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        // 4. 错误处理
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), e.what());
        return 0;
    }
}

jlong BiologicalAIJNIOptimized::getMemoryUsage(JNIEnv* env, jlong enginePtr) {
    // 1. 参数验证
    if (enginePtr == 0) {
        return 0;
    }
    
    try {
        // 2. 获取AI引擎实例
        auto* aiEngine = reinterpret_cast<lattice::entity::AIEngine*>(enginePtr);
        if (aiEngine) {
            // 3. 获取性能统计
            const auto& stats = aiEngine->getPerformanceStats();
            return static_cast<jlong>(stats.memoryUsage.load());
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        // 4. 错误处理
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), e.what());
        return 0;
    }
}

void BiologicalAIJNIOptimized::resetPerformanceStats(JNIEnv* env, jlong enginePtr) {
    // 1. 参数验证
    if (enginePtr == 0) {
        return;
    }
    
    try {
        // 2. 获取AI引擎实例
        auto* aiEngine = reinterpret_cast<lattice::entity::AIEngine*>(enginePtr);
        if (aiEngine) {
            // 3. 重置性能统计
            aiEngine->resetPerformanceStats();
        }
        
        // 4. 重置JNI统计
        totalOperations_ = 0;
        totalTimeMicros_ = 0;
        entitiesProcessed_ = 0;
        memoryUsage_ = 0;
        
    } catch (const std::exception& e) {
        // 5. 错误处理
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), e.what());
    }
}

void BiologicalAIJNIOptimized::logPerformanceStats(const char* operation, uint64_t durationMicros) {
    totalOperations_++;
    totalTimeMicros_.fetch_add(durationMicros);
}

void BiologicalAIJNIOptimized::convertEntityState(JNIEnv* env, jlong entityId, jfloat x, jfloat y, jfloat z, 
                                                 jfloat health, jboolean isAlive, 
                                                 lattice::entity::EntityState& state) {
    // 转换Java参数到C++ EntityState结构
    state.entityId = static_cast<uint64_t>(entityId);
    state.x = x;
    state.y = y;
    state.z = z;
    state.health = health;
    state.maxHealth = health; // 简化：假设当前血量等于最大血量
    state.isAlive = (isAlive == JNI_TRUE);
    state.rotationYaw = 0.0f; // 默认值
    state.rotationPitch = 0.0f; // 默认值
    state.velocityX = 0.0f; // 默认值
    state.velocityY = 0.0f; // 默认值
    state.velocityZ = 0.0f; // 默认值
    state.isInWater = false; // 默认值
    state.isInLava = false; // 默认值
    state.isOnFire = false; // 默认值
    state.lightLevel = 15.0f; // 默认值（最大光照）
    state.lastAttackTime = 0; // 默认值
    state.targetId = 0; // 默认值
    state.targetDistance = 0.0f; // 默认值
    state.behaviorState = lattice::entity::EntityState::BehaviorState::IDLE; // 默认行为状态
}

void BiologicalAIJNIOptimized::convertWorldView(JNIEnv* env, jlong worldViewPtr, lattice::entity::WorldView& worldView) {
    // 简化实现：假设worldViewPtr指向一个简化的世界视图数据
    // 实际实现中需要根据Java端的世界视图结构进行详细转换
    
    // 清空现有数据
    worldView.nearbyBlocks.clear();
    worldView.nearbyEntities.clear();
    
    // 初始化光照网格（默认值）
    for (int x = 0; x < 16; x++) {
        for (int y = 0; y < 256; y++) {
            for (int z = 0; z < 16; z++) {
                worldView.lightGrid[x][y][z] = 15.0f; // 最大光照
            }
        }
    }
    
    // 初始化生物群系网格（默认值）
    for (int x = 0; x < 16; x++) {
        for (int z = 0; z < 16; z++) {
            worldView.biomeGrid[x][z] = "plains"; // 默认平原生物群系
        }
    }
    
    // 设置时间戳
    worldView.timestamp = static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
}

} // namespace entity
} // namespace jni
} // namespace lattice