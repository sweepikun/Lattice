#include <jni.h>
#include "../core/net/entity_tracker.hpp"
#include "FinalLatticeJNI_Fixed.h"
#include <vector>
#include <memory>
#include <atomic>
#include <iostream>

namespace lattice {
namespace jni {

// ========== 优化的实体桥接类 ==========
class OptimizedEntityBridge {
private:
    // 使用框架的智能内存管理
    std::unique_ptr<jni_framework::JNIMemoryManager> memory_manager_;
    std::unique_ptr<jni_framework::JVMThreadPool> entity_pool_;
    std::unique_ptr<jni_framework::PerformanceMetrics> metrics_;
    
    // 框架配置
    jni_framework::FrameworkConfig config_;
    
public:
    OptimizedEntityBridge() {
        // 配置框架
        config_.memory.max_pool_size = 64 * 1024 * 1024; // 64MB 实体数据缓冲区
        config_.threading.thread_count = std::thread::hardware_concurrency();
        config_.performance.enable_metrics = true;
        
        // 初始化组件
        memory_manager_ = std::make_unique<jni_framework::JNIMemoryManager>(config_.memory);
        entity_pool_ = std::make_unique<jni_framework::JVMThreadPool>(config_.threading.thread_count);
        metrics_ = std::make_unique<jni_framework::PerformanceMetrics>();
    }
    
    ~OptimizedEntityBridge() = default;
    
    // 优化的实体追踪
    void trackEntityOptimized(JNIEnv* env, jint entityId, jint worldId, jfloat posX, jfloat posY, jfloat posZ) {
        auto start_time = std::chrono::high_resolution_clock::now();
        
        try {
            // 使用SafeBuffer进行零拷贝实体数据操作
            struct EntityData {
                int entityId;
                int worldId;
                float posX, posY, posZ;
                int64_t timestamp;
            };
            
            auto entity_buffer = memory_manager_->allocateDirect(sizeof(EntityData), "entity_tracking");
            EntityData* data = static_cast<EntityData*>(entity_buffer);
            
            data->entityId = entityId;
            data->worldId = worldId;
            data->posX = posX;
            data->posY = posY;
            data->posZ = posZ;
            data->timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::high_resolution_clock::now().time_since_epoch()
            ).count();
            
            // 异步提交到实体线程池
            entity_pool_->submit([data, this]() {
                try {
                    auto task_start = std::chrono::high_resolution_clock::now();
                    
                    // 获取或创建线程本地追踪器
                    thread_local lattice::entity::EntityTracker* tracker = nullptr;
                    if (!tracker) {
                        tracker = new lattice::entity::EntityTracker();
                        tracker->initialize();
                    }
                    
                    // 执行实体追踪
                    tracker->trackEntity(data->entityId, data->worldId, data->posX, data->posY, data->posZ);
                    
                    auto task_end = std::chrono::high_resolution_clock::now();
                    auto duration = task_end - task_start;
                    
                    // 记录性能指标
                    metrics_->recordLatency(duration);
                    
                } catch (const std::exception& e) {
                    std::cerr << "Optimized entity tracking failed: " << e.what() << std::endl;
                }
                
                // 清理缓冲区
                memory_manager_->deallocate(entity_buffer);
            });
            
        } catch (const jni_framework::JNIFrameworkException& e) {
            std::cerr << "Optimized entity tracking setup failed: " << e.what() << std::endl;
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto total_duration = end_time - start_time;
        metrics_->recordLatency(total_duration);
    }
    
    // 优化的批量实体操作
    void batchEntitiesOptimized(JNIEnv* env, jintArray entityIds, jintArray worldIds, 
                               jfloatArray posX, jfloatArray posY, jfloatArray posZ, 
                               jint operationType, jobject callback) {
        auto start_time = std::chrono::high_resolution_clock::now();
        
        try {
            jsize entityCount = env->GetArrayLength(entityIds);
            if (entityCount == 0 || entityCount != env->GetArrayLength(worldIds) ||
                entityCount != env->GetArrayLength(posX) || entityCount != env->GetArrayLength(posY) ||
                entityCount != env->GetArrayLength(posZ)) {
                std::cerr << "Entity batch arrays length mismatch" << std::endl;
                return;
            }
            
            // 批量获取数据
            jint* entity_ids = env->GetIntArrayElements(entityIds, nullptr);
            jint* world_ids = env->GetIntArrayElements(worldIds, nullptr);
            jfloat* x_coords = env->GetFloatArrayElements(posX, nullptr);
            jfloat* y_coords = env->GetFloatArrayElements(posY, nullptr);
            jfloat* z_coords = env->GetFloatArrayElements(posZ, nullptr);
            
            // 使用SafeBuffer批量处理
            size_t batch_size = static_cast<size_t>(entityCount);
            auto batch_buffer = memory_manager_->allocateDirect(
                batch_size * sizeof(std::tuple<jint, jint, jfloat, jfloat, jfloat>), 
                "entity_batch"
            );
            
            // 填充批量数据
            auto* batch_data = static_cast<std::tuple<jint, jint, jfloat, jfloat, jfloat>*>(batch_buffer);
            for (jsize i = 0; i < entityCount; ++i) {
                batch_data[i] = std::make_tuple(entity_ids[i], world_ids[i], 
                                               x_coords[i], y_coords[i], z_coords[i]);
            }
            
            // 释放原始数组
            env->ReleaseIntArrayElements(entityIds, entity_ids, JNI_ABORT);
            env->ReleaseIntArrayElements(worldIds, world_ids, JNI_ABORT);
            env->ReleaseFloatArrayElements(posX, x_coords, JNI_ABORT);
            env->ReleaseFloatArrayElements(posY, y_coords, JNI_ABORT);
            env->ReleaseFloatArrayElements(posZ, z_coords, JNI_ABORT);
            
            // 构建批量任务
            struct BatchEntityTask {
                void* batch_buffer;
                size_t entity_count;
                jint operation_type;
                jobject callback;
                jni_framework::JNIMemoryManager* memory_manager;
                jni_framework::PerformanceMetrics* metrics;
            };
            
            BatchEntityTask task{
                .batch_buffer = batch_buffer,
                .entity_count = batch_size,
                .operation_type = operationType,
                .callback = callback,
                .memory_manager = memory_manager_.get(),
                .metrics = metrics_.get()
            };
            
            // 异步批量处理
            entity_pool_->submit([task, this, env]() {
                try {
                    auto task_start = std::chrono::high_resolution_clock::now();
                    
                    auto* batch_data = static_cast<std::tuple<jint, jint, jfloat, jfloat, jfloat>*>(task.batch_buffer);
                    
                    for (size_t i = 0; i < task.entity_count; ++i) {
                        auto [entity_id, world_id, pos_x, pos_y, pos_z] = batch_data[i];
                        
                        // 执行相应的批量操作
                        switch (task.operation_type) {
                            case 1: // TRACK
                                // 追踪实体
                                {
                                    thread_local lattice::entity::EntityTracker* tracker = nullptr;
                                    if (!tracker) {
                                        tracker = new lattice::entity::EntityTracker();
                                        tracker->initialize();
                                    }
                                    tracker->trackEntity(entity_id, world_id, pos_x, pos_y, pos_z);
                                }
                                break;
                            case 2: // UNTRACK
                                // 取消追踪
                                {
                                    thread_local lattice::entity::EntityTracker* tracker = nullptr;
                                    if (!tracker) {
                                        tracker = new lattice::entity::EntityTracker();
                                        tracker->initialize();
                                    }
                                    tracker->untrackEntity(entity_id);
                                }
                                break;
                            default:
                                std::cerr << "Unknown batch operation type: " << task.operation_type << std::endl;
                        }
                    }
                    
                    auto task_end = std::chrono::high_resolution_clock::now();
                    auto duration = task_end - task_start;
                    
                    // 记录性能指标
                    task.metrics->recordLatency(duration);
                    
                    // 异步回调通知完成
                    if (task.callback && env) {
                        jclass callbackClass = env->GetObjectClass(task.callback);
                        jmethodID onCompleteMethod = env->GetMethodID(callbackClass, "onBatchComplete", "(I)V");
                        if (onCompleteMethod) {
                            env->CallVoidMethod(task.callback, onCompleteMethod, 1);
                        }
                        env->DeleteLocalRef(callbackClass);
                    }
                    
                } catch (const std::exception& e) {
                    std::cerr << "Optimized batch entity operation failed: " << e.what() << std::endl;
                    
                    if (task.callback && env) {
                        jclass callbackClass = env->GetObjectClass(task.callback);
                        jmethodID onCompleteMethod = env->GetMethodID(callbackClass, "onBatchComplete", "(I)V");
                        if (onCompleteMethod) {
                            env->CallVoidMethod(task.callback, onCompleteMethod, 0);
                        }
                        env->DeleteLocalRef(callbackClass);
                    }
                }
                
                // 清理批量缓冲区
                task.memory_manager->deallocate(task.batch_buffer);
            });
            
        } catch (const jni_framework::JNIFrameworkException& e) {
            std::cerr << "Optimized batch entity operation setup failed: " << e.what() << std::endl;
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto total_duration = end_time - start_time;
        metrics_->recordLatency(total_duration);
    }
    
    // 获取性能报告
    std::string getPerformanceReport() const {
        std::ostringstream report;
        report << "Optimized EntityBridge Performance Report:\n";
        report << "  Total Operations: " << metrics_->total_operations.load() << "\n";
        report << "  Average Latency: " << metrics_->getAverageLatencyUs() << " μs\n";
        report << "  Memory Usage: " << memory_manager_->getCurrentUsage() << " bytes\n";
        report << "  Peak Memory: " << memory_manager_->getPeakUsage() << " bytes\n";
        report << "  Entity Pool Size: " << entity_pool_->getThreadCount() << "\n";
        return report.str();
    }
};

// 全局优化实例
static std::unique_ptr<OptimizedEntityBridge> g_optimized_entity_bridge;

// 全局状态跟踪（保持向后兼容）
static std::atomic<int64_t> total_operations{0};
static std::atomic<int64_t> failed_operations{0};
static std::atomic<bool> native_available{true};

extern "C" {

// JNI_OnLoad
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    try {
        // 初始化优化实例
        g_optimized_entity_bridge = std::make_unique<OptimizedEntityBridge>();
        std::cout << "Optimized EntityBridge loaded with advanced performance features!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Failed to initialize Optimized EntityBridge: " << e.what() << std::endl;
        native_available = false;
    }
    
    return JNI_VERSION_1_8;
}

// JNI_OnUnload
JNIEXPORT void JNICALL JNI_OnUnload(JavaVM* vm, void* reserved) {
    std::cout << "Optimized EntityBridge unloading..." << std::endl;
    g_optimized_entity_bridge.reset();
}

// 优化的实体追踪
JNIEXPORT void JNICALL Java_com_lattice_entity_EntityBridge_trackEntity(
    JNIEnv* env, jclass, jint entityId, jint worldId, jfloat posX, jfloat posY, jfloat posZ) {
    
    total_operations++;
    
    try {
        if (!g_optimized_entity_bridge) {
            throw std::runtime_error("Optimized EntityBridge not initialized");
        }
        
        g_optimized_entity_bridge->trackEntityOptimized(env, entityId, worldId, posX, posY, posZ);
        
    } catch (const std::exception& e) {
        std::cerr << "Optimized entity tracking failed: " << e.what() << std::endl;
        failed_operations++;
        
        // 回退到原始实现
        try {
            thread_local lattice::entity::EntityTracker* tracker = nullptr;
            if (!tracker) {
                tracker = new lattice::entity::EntityTracker();
                tracker->initialize();
            }
            
            if (!tracker->trackEntity(entityId, worldId, posX, posY, posZ)) {
                throw std::runtime_error("Entity tracking failed");
            }
        } catch (const std::exception& fallback_error) {
            std::cerr << "Fallback entity tracking also failed: " << fallback_error.what() << std::endl;
        }
    }
}

// 优化的批量实体操作
JNIEXPORT void JNICALL Java_com_lattice_entity_EntityBridge_batchEntities(
    JNIEnv* env, jclass, jintArray entityIds, jintArray worldIds, 
    jfloatArray posX, jfloatArray posY, jfloatArray posZ, 
    jint operationType, jobject callback) {
    
    total_operations++;
    
    try {
        if (!g_optimized_entity_bridge) {
            throw std::runtime_error("Optimized EntityBridge not initialized");
        }
        
        g_optimized_entity_bridge->batchEntitiesOptimized(env, entityIds, worldIds, 
                                                         posX, posY, posZ, operationType, callback);
        
    } catch (const std::exception& e) {
        std::cerr << "Optimized batch entity operation failed: " << e.what() << std::endl;
        failed_operations++;
        
        // 回退到原始实现（单线程版本）
        try {
            jsize entityCount = env->GetArrayLength(entityIds);
            jint* entity_ids = env->GetIntArrayElements(entityIds, nullptr);
            jint* world_ids = env->GetIntArrayElements(worldIds, nullptr);
            jfloat* x_coords = env->GetFloatArrayElements(posX, nullptr);
            jfloat* y_coords = env->GetFloatArrayElements(posY, nullptr);
            jfloat* z_coords = env->GetFloatArrayElements(posZ, nullptr);
            
            thread_local lattice::entity::EntityTracker* tracker = nullptr;
            if (!tracker) {
                tracker = new lattice::entity::EntityTracker();
                tracker->initialize();
            }
            
            for (jsize i = 0; i < entityCount; ++i) {
                if (operationType == 1) { // TRACK
                    tracker->trackEntity(entity_ids[i], world_ids[i], x_coords[i], y_coords[i], z_coords[i]);
                } else if (operationType == 2) { // UNTRACK
                    tracker->untrackEntity(entity_ids[i]);
                }
            }
            
            env->ReleaseIntArrayElements(entityIds, entity_ids, JNI_ABORT);
            env->ReleaseIntArrayElements(worldIds, world_ids, JNI_ABORT);
            env->ReleaseFloatArrayElements(posX, x_coords, JNI_ABORT);
            env->ReleaseFloatArrayElements(posY, y_coords, JNI_ABORT);
            env->ReleaseFloatArrayElements(posZ, z_coords, JNI_ABORT);
            
        } catch (const std::exception& fallback_error) {
            std::cerr << "Fallback batch entity operation also failed: " << fallback_error.what() << std::endl;
        }
    }
}

// 获取优化状态
JNIEXPORT jboolean JNICALL Java_com_lattice_entity_EntityBridge_isNativeAvailable(
    JNIEnv* env, jclass) {
    return native_available ? JNI_TRUE : JNI_FALSE;
}

// 获取统计信息
JNIEXPORT jlong JNICALL Java_com_lattice_entity_EntityBridge_getTotalOperations(
    JNIEnv* env, jclass) {
    return total_operations.load();
}

JNIEXPORT jlong JNICALL Java_com_lattice_entity_EntityBridge_getFailedOperations(
    JNIEnv* env, jclass) {
    return failed_operations.load();
}

// 获取性能报告
JNIEXPORT jstring JNICALL Java_com_lattice_entity_EntityBridge_getPerformanceReport(
    JNIEnv* env, jclass) {
    try {
        if (g_optimized_entity_bridge) {
            std::string report = g_optimized_entity_bridge->getPerformanceReport();
            return env->NewStringUTF(report.c_str());
        }
        return env->NewStringUTF("Optimized EntityBridge not initialized");
    } catch (const std::exception& e) {
        std::cerr << "Failed to get performance report: " << e.what() << std::endl;
        return env->NewStringUTF("Performance report generation failed");
    }
}

} // extern "C"

} // namespace jni
} // namespace lattice