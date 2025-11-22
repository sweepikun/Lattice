#pragma once

#include <jni.h>
#include <memory>
#include <atomic>
#include "../../core/net/entity_tracker.hpp"

namespace lattice {
namespace jni {
namespace net {

// 正确的实体追踪器JNI桥接接口 - 仅负责桥接
class EntityTrackerJNIOptimized {
public:
    // 实体注册 - 桥接到core
    static jlong registerEntity(JNIEnv* env, jobject tracker, jlong entityId, 
                               jfloat posX, jfloat posY, jfloat posZ, jfloat radius);
    
    // 实体位置更新 - 桥接到core
    static void updateEntityPosition(JNIEnv* env, jobject tracker, jlong entityId, 
                                    jfloat posX, jfloat posY, jfloat posZ);
    
    // 实体移除 - 桥接到core
    static void unregisterEntity(JNIEnv* env, jobject tracker, jlong entityId);
    
    // 获取可见实体 - 桥接到core
    static jlongArray getVisibleEntities(JNIEnv* env, jobject tracker,
                                        jfloat viewerX, jfloat viewerY, jfloat viewerZ, 
                                        jfloat viewDistance);
    
    // 批量更新位置 - 桥接到core
    static void batchUpdatePositions(JNIEnv* env, jobject tracker, jlongArray entityUpdates);
    
    // 批量获取可见实体 - 桥接到core
    static jobjectArray batchGetVisibleEntities(JNIEnv* env, jobject tracker, 
                                               jlongArray viewerPositions);
    
    // 实体追踪器tick - 桥接到core
    static void tick(JNIEnv* env, jobject tracker);
    
    // 初始化线程局部追踪器 - 桥接到core
    static void initializeThreadLocal(JNIEnv* env, jobject tracker);
    
    // 关闭线程局部追踪器 - 桥接到core
    static void shutdownThreadLocal(JNIEnv* env, jobject tracker);
    
    // 性能统计桥接
    static jlong getTrackedEntitiesCount(JNIEnv* env, jobject tracker);
    static jlong getQueryCacheHitRate(JNIEnv* env, jobject tracker);
    static jlong getAverageQueryTimeMicros(JNIEnv* env, jobject tracker);
    static jlong getEntitiesProcessedCount(JNIEnv* env, jobject tracker);
    
    // 配置桥接
    static void setViewDistance(JNIEnv* env, jobject tracker, jfloat distance);
    
private:
    // 性能统计（仅统计JNI桥接调用）
    static std::atomic<uint64_t> totalOperations_;
    static std::atomic<uint64_t> totalTimeMicros_;
    static std::atomic<uint64_t> cacheHits_;
    static std::atomic<uint64_t> cacheMisses_;
    
    // 内部辅助方法
    static void logPerformanceStats(const char* operation, uint64_t durationMicros);
    static void checkJNIException(JNIEnv* env, const char* methodName);
};

// JNI方法映射
static JNINativeMethod entityTrackerMethods[] = {
    {"registerEntity", "(Ljava/lang/Object;JFFFF)J", 
     (void*)EntityTrackerJNIOptimized::registerEntity},
    {"updateEntityPosition", "(Ljava/lang/Object;JFFF)V", 
     (void*)EntityTrackerJNIOptimized::updateEntityPosition},
    {"unregisterEntity", "(Ljava/lang/Object;J)V", 
     (void*)EntityTrackerJNIOptimized::unregisterEntity},
    {"getVisibleEntities", "(Ljava/lang/Object;FFFF)[J", 
     (void*)EntityTrackerJNIOptimized::getVisibleEntities},
    {"batchUpdatePositions", "(Ljava/lang/Object;[J)V", 
     (void*)EntityTrackerJNIOptimized::batchUpdatePositions},
    {"batchGetVisibleEntities", "(Ljava/lang/Object;[J)[Ljava/lang/Object;", 
     (void*)EntityTrackerJNIOptimized::batchGetVisibleEntities},
    {"tick", "(Ljava/lang/Object;)V", 
     (void*)EntityTrackerJNIOptimized::tick},
    {"initializeThreadLocal", "(Ljava/lang/Object;)V", 
     (void*)EntityTrackerJNIOptimized::initializeThreadLocal},
    {"shutdownThreadLocal", "(Ljava/lang/Object;)V", 
     (void*)EntityTrackerJNIOptimized::shutdownThreadLocal},
    {"getTrackedEntitiesCount", "(Ljava/lang/Object;)J", 
     (void*)EntityTrackerJNIOptimized::getTrackedEntitiesCount},
    {"getQueryCacheHitRate", "(Ljava/lang/Object;)J", 
     (void*)EntityTrackerJNIOptimized::getQueryCacheHitRate},
    {"getAverageQueryTimeMicros", "(Ljava/lang/Object;)J", 
     (void*)EntityTrackerJNIOptimized::getAverageQueryTimeMicros},
    {"getEntitiesProcessedCount", "(Ljava/lang/Object;)J", 
     (void*)EntityTrackerJNIOptimized::getEntitiesProcessedCount},
    {"setViewDistance", "(Ljava/lang/Object;F)V", 
     (void*)EntityTrackerJNIOptimized::setViewDistance}
};

// 注册JNI方法
static jint registerEntityTrackerJNI(JNIEnv* env) {
    jclass entityTrackerClass = env->FindClass("com/lattice/native/EntityTracker");
    if (entityTrackerClass == nullptr) return JNI_ERR;
    
    return env->RegisterNatives(entityTrackerClass, 
                               entityTrackerMethods,
                               sizeof(entityTrackerMethods) / sizeof(entityTrackerMethods[0]));
}

} // namespace net
} // namespace jni
} // namespace lattice