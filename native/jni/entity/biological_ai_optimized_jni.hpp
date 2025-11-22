#pragma once

#include <jni.h>
#include <memory>
#include <atomic>
#include "../../entity/biological_ai.hpp"

namespace lattice {
namespace jni {
namespace entity {

// 正确的生物AI引擎JNI桥接接口 - 仅负责桥接
class BiologicalAIJNIOptimized {
public:
    // AI引擎初始化 - 桥接到core
    static jlong initializeAIEngine(JNIEnv* env, jstring minecraftDataPath, jstring gameVersion);
    
    // 关闭AI引擎 - 桥接到core
    static void shutdownAIEngine(JNIEnv* env, jlong enginePtr);
    
    // 实体注册 - 桥接到core
    static jboolean registerEntity(JNIEnv* env, jlong enginePtr, jlong entityId, jstring entityType);
    
    // 实体移除 - 桥接到core
    static void unregisterEntity(JNIEnv* env, jlong enginePtr, jlong entityId);
    
    // 实体状态更新 - 桥接到core
    static void updateEntityState(JNIEnv* env, jlong enginePtr, jlong entityId, 
                                 jfloat x, jfloat y, jfloat z, jfloat health, jboolean isAlive);
    
    // 世界视图更新 - 桥接到core
    static void updateWorldView(JNIEnv* env, jlong enginePtr, jlong entityId, 
                               jlong worldViewPtr);
    
    // 执行AI决策 - 桥接到core
    static void executeAIDecision(JNIEnv* env, jlong enginePtr, jlong entityId, jlong deltaTime);
    
    // 批量实体处理 - 桥接到core
    static void batchProcessEntities(JNIEnv* env, jlong enginePtr, jlongArray entityIds, jlong deltaTime);
    
    // AI引擎主循环 - 桥接到core
    static void tick(JNIEnv* env, jlong enginePtr);
    
    // 获取实体配置 - 桥接到core
    static jlong getEntityConfig(JNIEnv* env, jlong enginePtr, jlong entityId);
    
    // 性能统计桥接
    static jlong getTotalTicks(JNIEnv* env, jlong enginePtr);
    static jlong getAverageTickTimeMicros(JNIEnv* env, jlong enginePtr);
    static jlong getMaxTickTimeMicros(JNIEnv* env, jlong enginePtr);
    static jlong getEntitiesProcessed(JNIEnv* env, jlong enginePtr);
    static jlong getMemoryUsage(JNIEnv* env, jlong enginePtr);
    
    // 性能统计重置
    static void resetPerformanceStats(JNIEnv* env, jlong enginePtr);
    
private:
    // 性能统计（仅统计JNI桥接调用）
    static std::atomic<uint64_t> totalOperations_;
    static std::atomic<uint64_t> totalTimeMicros_;
    static std::atomic<uint64_t> entitiesProcessed_;
    static std::atomic<uint64_t> memoryUsage_;
    
    // 内部辅助方法
    static void logPerformanceStats(const char* operation, uint64_t durationMicros);
    static void convertEntityState(JNIEnv* env, jlong entityId, jfloat x, jfloat y, jfloat z, 
                                  jfloat health, jboolean isAlive, 
                                  lattice::entity::EntityState& state);
    static void convertWorldView(JNIEnv* env, jlong worldViewPtr, lattice::entity::WorldView& worldView);
};

// JNI方法映射
static JNINativeMethod biologicalAIMethods[] = {
    {"initializeAIEngine", "(Ljava/lang/String;Ljava/lang/String;)J", 
     (void*)BiologicalAIJNIOptimized::initializeAIEngine},
    {"shutdownAIEngine", "(J)V", 
     (void*)BiologicalAIJNIOptimized::shutdownAIEngine},
    {"registerEntity", "(JJLjava/lang/String;)Z", 
     (void*)BiologicalAIJNIOptimized::registerEntity},
    {"unregisterEntity", "(JJ)V", 
     (void*)BiologicalAIJNIOptimized::unregisterEntity},
    {"updateEntityState", "(JJFFFFFZ)V", 
     (void*)BiologicalAIJNIOptimized::updateEntityState},
    {"updateWorldView", "(JJJ)V", 
     (void*)BiologicalAIJNIOptimized::updateWorldView},
    {"executeAIDecision", "(JJJ)V", 
     (void*)BiologicalAIJNIOptimized::executeAIDecision},
    {"batchProcessEntities", "(JJ[J)V", 
     (void*)BiologicalAIJNIOptimized::batchProcessEntities},
    {"tick", "(J)V", 
     (void*)BiologicalAIJNIOptimized::tick},
    {"getEntityConfig", "(JJ)J", 
     (void*)BiologicalAIJNIOptimized::getEntityConfig},
    {"getTotalTicks", "(J)J", 
     (void*)BiologicalAIJNIOptimized::getTotalTicks},
    {"getAverageTickTimeMicros", "(J)J", 
     (void*)BiologicalAIJNIOptimized::getAverageTickTimeMicros},
    {"getMaxTickTimeMicros", "(J)J", 
     (void*)BiologicalAIJNIOptimized::getMaxTickTimeMicros},
    {"getEntitiesProcessed", "(J)J", 
     (void*)BiologicalAIJNIOptimized::getEntitiesProcessed},
    {"getMemoryUsage", "(J)J", 
     (void*)BiologicalAIJNIOptimized::getMemoryUsage},
    {"resetPerformanceStats", "(J)V", 
     (void*)BiologicalAIJNIOptimized::resetPerformanceStats}
};

// 注册JNI方法
static jint registerBiologicalAIJNI(JNIEnv* env) {
    jclass aiEngineClass = env->FindClass("com/lattice/native/BiologicalAIEngine");
    if (aiEngineClass == nullptr) return JNI_ERR;
    
    return env->RegisterNatives(aiEngineClass, 
                               biologicalAIMethods,
                               sizeof(biologicalAIMethods) / sizeof(biologicalAIMethods[0]));
}

} // namespace entity
} // namespace jni
} // namespace lattice