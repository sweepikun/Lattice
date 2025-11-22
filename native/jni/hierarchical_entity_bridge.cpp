#include "hierarchical_tracker.hpp"
#include "jni.h"
#include "jni_helper.hpp"
#include <vector>
#include <memory>
#include <atomic>
#include <chrono>

namespace lattice {
namespace entity {
namespace jni {

// ===== 全局状态管理 =====
struct GlobalState {
    std::atomic<bool> nativeEnabled{true};
    std::atomic<uint64_t> totalFailures{0};
    std::atomic<uint64_t> totalCalls{0};
    std::atomic<double> failureRate{0.0};
    
    static constexpr double FALLBACK_THRESHOLD = 0.10; // 10%失败率
    
    void recordCall(bool success) {
        totalCalls.fetch_add(1, std::memory_order_relaxed);
        if (!success) {
            totalFailures.fetch_add(1, std::memory_order_relaxed);
        }
        
        // 动态计算失败率
        uint64_t calls = totalCalls.load(std::memory_order_relaxed);
        uint64_t failures = totalFailures.load(std::memory_order_relaxed);
        
        if (calls > 0) {
            failureRate.store(static_cast<double>(failures) / calls, std::memory_order_relaxed);
        }
    }
    
    bool shouldFallback() const {
        return failureRate.load(std::memory_order_relaxed) > FALLBACK_THRESHOLD;
    }
    
    void reset() {
        totalFailures = 0;
        totalCalls = 0;
        failureRate = 0.0;
    }
};

static GlobalState g_state;

// ===== JNI辅助函数 =====
class JNIHelper {
public:
    static bool checkJNIExceptions(JNIEnv* env) {
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
            return true;
        }
        return false;
    }
    
    static void logError(const char* format, ...) {
        va_list args;
        va_start(args, format);
        // 这里可以集成实际的日志系统
        std::vprintf(format, args);
        va_end(args);
        std::printf("\n");
    }
    
    static void logInfo(const char* format, ...) {
        va_list args;
        va_start(args, format);
        std::vprintf(format, args);
        va_end(args);
        std::printf("\n");
    }
};

// ===== JNI方法实现 =====

extern "C" {

// ===== 生命周期管理 =====

JNIEXPORT void JNICALL
Java_net_lattice_entity_HierarchicalEntityTracker_nativeInitialize(JNIEnv* env, jclass clazz) {
    try {
        JNIHierarchicalTracker::initialize();
        JNIHelper::logInfo("Hierarchical Entity Tracker initialized successfully");
    } catch (const std::exception& e) {
        JNIHelper::logError("Failed to initialize Hierarchical Entity Tracker: %s", e.what());
        g_state.recordCall(false);
        throw;
    } catch (...) {
        JNIHelper::logError("Unknown error during Hierarchical Entity Tracker initialization");
        g_state.recordCall(false);
        throw;
    }
    g_state.recordCall(true);
}

JNIEXPORT void JNICALL
Java_net_lattice_entity_HierarchicalEntityTracker_nativeShutdown(JNIEnv* env, jclass clazz) {
    try {
        JNIHierarchicalTracker::shutdown();
        JNIHelper::logInfo("Hierarchical Entity Tracker shutdown completed");
    } catch (const std::exception& e) {
        JNIHelper::logError("Error during Hierarchical Entity Tracker shutdown: %s", e.what());
        g_state.recordCall(false);
    } catch (...) {
        JNIHelper::logError("Unknown error during Hierarchical Entity Tracker shutdown");
        g_state.recordCall(false);
    }
    g_state.recordCall(true);
}

JNIEXPORT jboolean JNICALL
Java_net_lattice_entity_HierarchicalEntityTracker_isNativeEnabled(JNIEnv* env, jclass clazz) {
    return !g_state.shouldFallback() && g_state.nativeEnabled.load();
}

JNIEXPORT void JNICALL
Java_net_lattice_entity_HierarchicalEntityTracker_setNativeEnabled(JNIEnv* env, jclass clazz, jboolean enabled) {
    g_state.nativeEnabled.store(enabled);
    JNIHelper::logInfo("Native entity tracking %s", enabled ? "enabled" : "disabled");
}

// ===== 实体管理 =====

JNIEXPORT void JNICALL
Java_net_lattice_entity_HierarchicalEntityTracker_nativeRegisterEntity(JNIEnv* env, jclass clazz,
                                                                       jint entityId, jfloat x, jfloat y, jfloat z,
                                                                       jfloat radius, jbyte entityType) {
    if (g_state.shouldFallback() || !g_state.nativeEnabled.load()) {
        g_state.recordCall(false);
        return;
    }
    
    try {
        JNIHierarchicalTracker::registerEntity(entityId, x, y, z, radius, static_cast<uint8_t>(entityType));
        g_state.recordCall(true);
    } catch (const std::exception& e) {
        JNIHelper::logError("Failed to register entity %d: %s", entityId, e.what());
        g_state.recordCall(false);
        throw;
    } catch (...) {
        JNIHelper::logError("Unknown error registering entity %d", entityId);
        g_state.recordCall(false);
        throw;
    }
}

JNIEXPORT void JNICALL
Java_net_lattice_entity_HierarchicalEntityTracker_nativeUpdateEntityPosition(JNIEnv* env, jclass clazz,
                                                                             jint entityId, jfloat x, jfloat y, jfloat z) {
    if (g_state.shouldFallback() || !g_state.nativeEnabled.load()) {
        g_state.recordCall(false);
        return;
    }
    
    try {
        JNIHierarchicalTracker::updateEntityPosition(entityId, x, y, z);
        g_state.recordCall(true);
    } catch (const std::exception& e) {
        JNIHelper::logError("Failed to update position for entity %d: %s", entityId, e.what());
        g_state.recordCall(false);
        throw;
    } catch (...) {
        JNIHelper::logError("Unknown error updating position for entity %d", entityId);
        g_state.recordCall(false);
        throw;
    }
}

JNIEXPORT void JNICALL
Java_net_lattice_entity_HierarchicalEntityTracker_nativeUnregisterEntity(JNIEnv* env, jclass clazz, jint entityId) {
    if (g_state.shouldFallback() || !g_state.nativeEnabled.load()) {
        g_state.recordCall(false);
        return;
    }
    
    try {
        // 这里需要从HierarchicalTracker添加unregister方法
        // 暂时跳过实现
        g_state.recordCall(true);
    } catch (const std::exception& e) {
        JNIHelper::logError("Failed to unregister entity %d: %s", entityId, e.what());
        g_state.recordCall(false);
        throw;
    } catch (...) {
        JNIHelper::logError("Unknown error unregistering entity %d", entityId);
        g_state.recordCall(false);
        throw;
    }
}

// ===== 可见性查询（核心优化） =====

JNIEXPORT jintArray JNICALL
Java_net_lattice_entity_HierarchicalEntityTracker_nativeGetVisibleEntities(JNIEnv* env, jclass clazz,
                                                                            jint viewerId, jfloat viewerX, jfloat viewerY, jfloat viewerZ,
                                                                            jfloat viewDistance) {
    if (g_state.shouldFallback() || !g_state.nativeEnabled.load()) {
        g_state.recordCall(false);
        return env->NewIntArray(0);
    }
    
    auto startTime = std::chrono::steady_clock::now();
    
    try {
        int outCount = 0;
        int* result = JNIHierarchicalTracker::getVisibleEntities(viewerId, viewerX, viewerY, viewerZ,
                                                                viewDistance, &outCount);
        
        if (result && outCount > 0) {
            jintArray array = env->NewIntArray(outCount);
            env->SetIntArrayRegion(array, 0, outCount, result);
            delete[] result;
            
            auto endTime = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
            
            JNIHelper::logInfo("Query returned %d entities in %ld μs", outCount, duration.count());
            
            g_state.recordCall(true);
            return array;
        }
        
        g_state.recordCall(true);
        return env->NewIntArray(0);
        
    } catch (const std::exception& e) {
        JNIHelper::logError("Failed to query visible entities for viewer %d: %s", viewerId, e.what());
        g_state.recordCall(false);
        
        // 返回空数组而不是抛出异常，避免Java层崩溃
        return env->NewIntArray(0);
    } catch (...) {
        JNIHelper::logError("Unknown error querying visible entities for viewer %d", viewerId);
        g_state.recordCall(false);
        return env->NewIntArray(0);
    }
}

// ===== 批量操作 =====

JNIEXPORT void JNICALL
Java_net_lattice_entity_HierarchicalEntityTracker_nativeBatchUpdatePositions(JNIEnv* env, jclass clazz,
                                                                             jintArray entityIds, jfloatArray positions) {
    if (g_state.shouldFallback() || !g_state.nativeEnabled.load()) {
        g_state.recordCall(false);
        return;
    }
    
    try {
        jsize idCount = env->GetArrayLength(entityIds);
        jsize posCount = env->GetArrayLength(positions);
        
        if (idCount * 3 != posCount) {
            throw std::invalid_argument("Position array size doesn't match entity count");
        }
        
        jint* ids = env->GetIntArrayElements(entityIds, nullptr);
        jfloat* pos = env->GetFloatArrayElements(positions, nullptr);
        
        // 批量更新位置
        for (jsize i = 0; i < idCount; i++) {
            JNIHierarchicalTracker::updateEntityPosition(ids[i], pos[i*3], pos[i*3+1], pos[i*3+2]);
        }
        
        env->ReleaseIntArrayElements(entityIds, ids, 0);
        env->ReleaseFloatArrayElements(positions, pos, 0);
        
        g_state.recordCall(true);
        
    } catch (const std::exception& e) {
        JNIHelper::logError("Failed to batch update positions: %s", e.what());
        g_state.recordCall(false);
        throw;
    } catch (...) {
        JNIHelper::logError("Unknown error in batch position update");
        g_state.recordCall(false);
        throw;
    }
}

// ===== 压缩网络数据传输 =====

JNIEXPORT jbyteArray JNICALL
Java_net_lattice_entity_HierarchicalEntityTracker_nativeCompressEntityUpdates(JNIEnv* env, jclass clazz,
                                                                              jintArray entityIds) {
    if (g_state.shouldFallback() || !g_state.nativeEnabled.load()) {
        g_state.recordCall(false);
        return nullptr;
    }
    
    try {
        jsize count = env->GetArrayLength(entityIds);
        if (count == 0) {
            return nullptr;
        }
        
        jint* ids = env->GetIntArrayElements(entityIds, nullptr);
        
        size_t compressedSize = 0;
        void* compressedData = JNIHierarchicalTracker::compressEntityUpdates(
            reinterpret_cast<const int*>(ids), count, &compressedSize);
        
        env->ReleaseIntArrayElements(entityIds, ids, 0);
        
        if (compressedData && compressedSize > 0) {
            jbyteArray result = env->NewByteArray(static_cast<jsize>(compressedSize));
            env->SetByteArrayRegion(result, 0, static_cast<jsize>(compressedSize),
                                   static_cast<const jbyte*>(compressedData));
            
            free(compressedData);
            g_state.recordCall(true);
            return result;
        }
        
        if (compressedData) {
            free(compressedData);
        }
        
        g_state.recordCall(true);
        return nullptr;
        
    } catch (const std::exception& e) {
        JNIHelper::logError("Failed to compress entity updates: %s", e.what());
        g_state.recordCall(false);
        return nullptr;
    } catch (...) {
        JNIHelper::logError("Unknown error compressing entity updates");
        g_state.recordCall(false);
        return nullptr;
    }
}

// ===== Tick循环 =====

JNIEXPORT void JNICALL
Java_net_lattice_entity_HierarchicalEntityTracker_nativeTick(JNIEnv* env, jclass clazz) {
    if (g_state.shouldFallback() || !g_state.nativeEnabled.load()) {
        g_state.recordCall(false);
        return;
    }
    
    try {
        JNIHierarchicalTracker::tick();
        g_state.recordCall(true);
    } catch (const std::exception& e) {
        JNIHelper::logError("Error in entity tracking tick: %s", e.what());
        g_state.recordCall(false);
        throw;
    } catch (...) {
        JNIHelper::logError("Unknown error in entity tracking tick");
        g_state.recordCall(false);
        throw;
    }
}

// ===== 性能监控 =====

JNIEXPORT jstring JNICALL
Java_net_lattice_entity_HierarchicalEntityTracker_nativeGetPerformanceStats(JNIEnv* env, jclass clazz) {
    try {
        auto& stats = HierarchicalTrackerFactory::forThread()->getStats();
        
        char buffer[512];
        snprintf(buffer, sizeof(buffer),
                "Queries: %lu, Cache hits: %lu, SIMD ops: %lu, Async tasks: %lu, "
                "Avg query time: %.2f ns, Entities: %lu",
                stats.totalQueries.load(), stats.cacheHits.load(),
                stats.simdOperations.load(), stats.asyncTasks.load(),
                stats.averageQueryTimeNs.load(), stats.entitiesProcessed.load());
        
        g_state.recordCall(true);
        return env->NewStringUTF(buffer);
        
    } catch (const std::exception& e) {
        JNIHelper::logError("Failed to get performance stats: %s", e.what());
        g_state.recordCall(false);
        return env->NewStringUTF("Stats unavailable");
    } catch (...) {
        JNIHelper::logError("Unknown error getting performance stats");
        g_state.recordCall(false);
        return env->NewStringUTF("Stats unavailable");
    }
}

JNIEXPORT void JNICALL
Java_net_lattice_entity_HierarchicalEntityTracker_nativeResetStats(JNIEnv* env, jclass clazz) {
    try {
        auto tracker = HierarchicalTrackerFactory::forThread();
        if (tracker) {
            tracker->resetStats();
        }
        g_state.recordCall(true);
    } catch (const std::exception& e) {
        JNIHelper::logError("Failed to reset stats: %s", e.what());
        g_state.recordCall(false);
        throw;
    } catch (...) {
        JNIHelper::logError("Unknown error resetting stats");
        g_state.recordCall(false);
        throw;
    }
}

// ===== 配置管理 =====

JNIEXPORT void JNICALL
Java_net_lattice_entity_HierarchicalEntityTracker_nativeSetViewDistance(JNIEnv* env, jclass clazz, jfloat distance) {
    if (g_state.shouldFallback() || !g_state.nativeEnabled.load()) {
        g_state.recordCall(false);
        return;
    }
    
    try {
        auto tracker = HierarchicalTrackerFactory::forThread();
        if (tracker) {
            tracker->setViewDistance(distance);
        }
        g_state.recordCall(true);
    } catch (const std::exception& e) {
        JNIHelper::logError("Failed to set view distance: %s", e.what());
        g_state.recordCall(false);
        throw;
    } catch (...) {
        JNIHelper::logError("Unknown error setting view distance");
        g_state.recordCall(false);
        throw;
    }
}

JNIEXPORT void JNICALL
Java_net_lattice_entity_HierarchicalEntityTracker_nativeSetFeatureFlags(JNIEnv* env, jclass clazz,
                                                                         jboolean simdEnabled, jboolean predictiveLoading,
                                                                         jboolean priorityScheduling) {
    if (g_state.shouldFallback() || !g_state.nativeEnabled.load()) {
        g_state.recordCall(false);
        return;
    }
    
    try {
        auto tracker = HierarchicalTrackerFactory::forThread();
        if (tracker) {
            tracker->setSimdEnabled(simdEnabled);
            tracker->setPredictiveLoading(predictiveLoading);
            tracker->setPriorityScheduling(priorityScheduling);
        }
        g_state.recordCall(true);
    } catch (const std::exception& e) {
        JNIHelper::logError("Failed to set feature flags: %s", e.what());
        g_state.recordCall(false);
        throw;
    } catch (...) {
        JNIHelper::logError("Unknown error setting feature flags");
        g_state.recordCall(false);
        throw;
    }
}

// ===== 故障恢复 =====

JNIEXPORT void JNICALL
Java_net_lattice_entity_HierarchicalEntityTracker_nativeResetFailureTracking(JNIEnv* env, jclass clazz) {
    g_state.reset();
    JNIHelper::logInfo("Failure tracking reset");
    g_state.recordCall(true);
}

JNIEXPORT jdouble JNICALL
Java_net_lattice_entity_HierarchicalEntityTracker_nativeGetFailureRate(JNIEnv* env, jclass clazz) {
    g_state.recordCall(true);
    return g_state.failureRate.load();
}

} // extern "C"

} // namespace jni
} // namespace entity
} // namespace lattice