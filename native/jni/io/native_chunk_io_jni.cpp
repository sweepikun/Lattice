#include "native_chunk_io_jni.hpp"
#include <stdexcept>
#include <vector>
#include <memory>
#include "../FinalLatticeJNI_Fixed.h"

namespace lattice {
namespace jni {
namespace io {

// ========== 优化的JNI包装器类 ==========
class OptimizedNativeChunkIOJNI {
private:
    // 使用框架的智能内存管理
    std::unique_ptr<jni_framework::JNIMemoryManager> memory_manager_;
    std::unique_ptr<jni_framework::JVMThreadPool> async_pool_;
    std::unique_ptr<jni_framework::PerformanceMetrics> metrics_;
    
    // 框架配置
    jni_framework::FrameworkConfig config_;
    
public:
    OptimizedNativeChunkIOJNI() {
        // 配置框架
        config_.memory.max_pool_size = 32 * 1024 * 1024; // 32MB
        config_.threading.thread_count = std::thread::hardware_concurrency();
        config_.performance.enable_metrics = true;
        
        // 初始化组件
        memory_manager_ = std::make_unique<jni_framework::JNIMemoryManager>(config_.memory);
        async_pool_ = std::make_unique<jni_framework::JVMThreadPool>(config_.threading.thread_count);
        metrics_ = std::make_unique<jni_framework::PerformanceMetrics>();
    }
    
    ~OptimizedNativeChunkIOJNI() = default;
    
    // 优化的异步块加载
    void loadChunkAsyncOptimized(JNIEnv* env, jint worldId, jint chunkX, jint chunkZ, jobject callback) {
        auto start_time = std::chrono::high_resolution_clock::now();
        
        try {
            // 使用SafeBuffer进行智能内存分配
            auto chunk_buffer = memory_manager_->allocateDirect(sizeof(lattice::io::AsyncChunkIO::ChunkData), "chunk_load");
            
            // 构建任务描述
            struct LoadTask {
                int worldId;
                int chunkX;
                int chunkZ;
                jobject callback;
                void* buffer;
                jni_framework::JNIMemoryManager* memory_manager;
            };
            
            LoadTask task{
                .worldId = worldId,
                .chunkX = chunkX,
                .chunkZ = chunkZ,
                .callback = callback,
                .buffer = chunk_buffer,
                .memory_manager = memory_manager_.get()
            };
            
            // 异步提交到线程池
            async_pool_->submit([task, this, env]() {
                try {
                    auto task_start = std::chrono::high_resolution_clock::now();
                    
                    // 执行异步块加载逻辑
                    lattice::io::AsyncChunkIO::loadChunkAsync(task.worldId, task.chunkX, task.chunkZ);
                    
                    auto task_end = std::chrono::high_resolution_clock::now();
                    auto duration = task_end - task_start;
                    
                    // 记录性能指标
                    metrics_->recordLatency(duration);
                    
                    // 通知Java回调
                    if (task.callback && env) {
                        env->CallVoidMethod(task.callback, NativeChunkIOJNI::chunkCallback_onComplete_, 1);
                    }
                    
                } catch (const std::exception& e) {
                    std::cerr << "Chunk loading failed: " << e.what() << std::endl;
                    
                    if (task.callback && env) {
                        env->CallVoidMethod(task.callback, NativeChunkIOJNI::chunkCallback_onComplete_, 0);
                    }
                }
                
                // 清理资源
                task.memory_manager->deallocate(task.buffer);
            });
            
        } catch (const jni_framework::JNIFrameworkException& e) {
            std::cerr << "Optimized chunk loading failed: " << e.what() << std::endl;
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto total_duration = end_time - start_time;
        metrics_->recordLatency(total_duration);
    }
    
    // 优化的批量保存
    void saveChunksBatchOptimized(JNIEnv* env, jobjectArray chunksArray, jobject batchCallback) {
        auto start_time = std::chrono::high_resolution_clock::now();
        
        try {
            jsize chunkCount = env->GetArrayLength(chunksArray);
            if (chunkCount == 0) return;
            
            // 使用SafeBuffer进行批量内存管理
            size_t total_size = chunkCount * sizeof(lattice::io::AsyncChunkIO::ChunkData);
            auto batch_buffer = memory_manager_->allocateDirect(total_size, "batch_save");
            
            // 构建批量任务
            struct BatchSaveTask {
                jobjectArray chunksArray;
                jobject batchCallback;
                void* buffer;
                size_t chunkCount;
                jni_framework::JNIMemoryManager* memory_manager;
                jni_framework::JVMThreadPool* async_pool;
                jni_framework::PerformanceMetrics* metrics;
            };
            
            BatchSaveTask task{
                .chunksArray = chunksArray,
                .batchCallback = batchCallback,
                .buffer = batch_buffer,
                .chunkCount = static_cast<size_t>(chunkCount),
                .memory_manager = memory_manager_.get(),
                .async_pool = async_pool_.get(),
                .metrics = metrics_.get()
            };
            
            // 提交批量任务
            async_pool_->submit([task, this, env]() {
                try {
                    auto task_start = std::chrono::high_resolution_clock::now();
                    
                    // 批量保存逻辑
                    for (jsize i = 0; i < static_cast<jsize>(task.chunkCount); ++i) {
                        jobject chunkObj = env->GetObjectArrayElement(task.chunksArray, i);
                        
                        // 获取chunk数据
                        jint chunkX = env->GetIntField(chunkObj, NativeChunkIOJNI::chunk_x_field_);
                        jint chunkZ = env->GetIntField(chunkObj, NativeChunkIOJNI::chunk_z_field_);
                        jbyteArray chunkData = (jbyteArray)env->GetObjectField(chunkObj, NativeChunkIOJNI::chunk_data_field_);
                        
                        // 保存chunk
                        lattice::io::AsyncChunkIO::saveChunkAsync(task.memory_manager->getCurrentWorldId(), chunkX, chunkZ, chunkData);
                        
                        env->DeleteLocalRef(chunkObj);
                    }
                    
                    auto task_end = std::chrono::high_resolution_clock::now();
                    auto duration = task_end - task_start;
                    
                    // 记录性能指标
                    task.metrics->recordLatency(duration);
                    
                    // 通知完成
                    if (task.batchCallback && env) {
                        env->CallVoidMethod(task.batchCallback, NativeChunkIOJNI::batchCallback_onBatchComplete_, 1);
                    }
                    
                } catch (const std::exception& e) {
                    std::cerr << "Batch save failed: " << e.what() << std::endl;
                    
                    if (task.batchCallback && env) {
                        env->CallVoidMethod(task.batchCallback, NativeChunkIOJNI::batchCallback_onBatchComplete_, 0);
                    }
                }
                
                // 清理资源
                task.memory_manager->deallocate(task.buffer);
            });
            
        } catch (const jni_framework::JNIFrameworkException& e) {
            std::cerr << "Optimized batch save failed: " << e.what() << std::endl;
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto total_duration = end_time - start_time;
        metrics_->recordLatency(total_duration);
    }
    
    // 获取性能报告
    std::string getPerformanceReport() const {
        std::ostringstream report;
        report << "Optimized NativeChunkIOJNI Performance Report:\n";
        report << "  Total Operations: " << metrics_->total_operations.load() << "\n";
        report << "  Average Latency: " << metrics_->getAverageLatencyUs() << " μs\n";
        report << "  Memory Usage: " << memory_manager_->getCurrentUsage() << " bytes\n";
        report << "  Peak Memory: " << memory_manager_->getPeakUsage() << " bytes\n";
        report << "  Thread Pool Size: " << async_pool_->getThreadCount() << "\n";
        return report.str();
    }
};

// 全局优化实例
static std::unique_ptr<OptimizedNativeChunkIOJNI> g_optimized_io;

// ========== 优化后的JNI实现 ==========

extern "C" {

JNIEXPORT void JNICALL
Java_com_lattice_world_NativeChunkIO_initialize(JNIEnv* env, jclass clazz, 
                                               jboolean enableDirectIO, jint maxOps) {
    try {
        // 初始化优化实例
        if (!g_optimized_io) {
            g_optimized_io = std::make_unique<OptimizedNativeChunkIOJNI>();
        }
        
        // 原初始化逻辑保持不变
        if (NativeChunkIOJNI::initialized_.exchange(true)) return; // 防止重复初始化
        
        NativeChunkIOJNI::maxConcurrentIO_.store(maxOps);
        NativeChunkIOJNI::directIOEnabled_.store(enableDirectIO ? true : false);
        
        // 配置 I/O 系统
        lattice::io::AsyncChunkIO::setMaxConcurrentIO(maxOps);
        lattice::io::AsyncChunkIO::enableDirectIO(enableDirectIO ? true : false);
        
        // 缓存JNI方法ID
        NativeChunkIOJNI::cacheMethodIDs(env);
        
        std::cout << "Optimized NativeChunkIOJNI initialized with advanced performance features!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Failed to initialize Optimized NativeChunkIOJNI: " << e.what() << std::endl;
        throw;
    }
}

JNIEXPORT void JNICALL
Java_com_lattice_world_NativeChunkIO_loadChunkAsync(JNIEnv* env, jclass clazz,
                                                  jint worldId, jint chunkX, jint chunkZ,
                                                  jobject callback) {
    try {
        if (!g_optimized_io) {
            throw std::runtime_error("Optimized NativeChunkIOJNI not initialized");
        }
        
        // 使用优化版本
        g_optimized_io->loadChunkAsyncOptimized(env, worldId, chunkX, chunkZ, callback);
        
    } catch (const std::exception& e) {
        std::cerr << "Optimized chunk loading failed: " << e.what() << std::endl;
        // 回退到原始实现
        NativeChunkIOJNI::loadChunkAsync(env, clazz, worldId, chunkX, chunkZ, callback);
    }
}

JNIEXPORT void JNICALL
Java_com_lattice_world_NativeChunkIO_saveChunksBatch(JNIEnv* env, jclass clazz,
                                                   jobjectArray chunksArray,
                                                   jobject batchCallback) {
    try {
        if (!g_optimized_io) {
            throw std::runtime_error("Optimized NativeChunkIOJNI not initialized");
        }
        
        // 使用优化版本
        g_optimized_io->saveChunksBatchOptimized(env, chunksArray, batchCallback);
        
    } catch (const std::exception& e) {
        std::cerr << "Optimized batch saving failed: " << e.what() << std::endl;
        // 回退到原始实现
        NativeChunkIOJNI::saveChunksBatch(env, clazz, chunksArray, batchCallback);
    }
}

JNIEXPORT jint JNICALL
Java_com_lattice_world_NativeChunkIO_getPlatformFeatures(JNIEnv* env, jclass clazz) {
    try {
        int features = NativeChunkIOJNI::getPlatformFeatures(env, clazz);
        
        // 添加优化特性
        if (g_optimized_io) {
            features |= 0x10000000; // 优化特性标志位
        }
        
        return features;
        
    } catch (const std::exception& e) {
        std::cerr << "Failed to get optimized platform features: " << e.what() << std::endl;
        return NativeChunkIOJNI::getPlatformFeatures(env, clazz);
    }
}

JNIEXPORT void JNICALL
Java_com_lattice_world_NativeChunkIO_setMaxConcurrentIO(JNIEnv* env, jclass clazz, jint maxOps) {
    NativeChunkIOJNI::setMaxConcurrentIO(env, clazz, maxOps);
}

JNIEXPORT void JNICALL
Java_com_lattice_world_NativeChunkIO_enableDirectIO(JNIEnv* env, jclass clazz, jboolean enable) {
    NativeChunkIOJNI::enableDirectIO(env, clazz, enable);
}

JNIEXPORT jstring JNICALL
Java_com_lattice_world_NativeChunkIO_getPerformanceReport(JNIEnv* env, jclass clazz) {
    try {
        if (g_optimized_io) {
            std::string report = g_optimized_io->getPerformanceReport();
            return env->NewStringUTF(report.c_str());
        }
        return env->NewStringUTF("Optimized NativeChunkIOJNI not initialized");
    } catch (const std::exception& e) {
        std::cerr << "Failed to get performance report: " << e.what() << std::endl;
        return env->NewStringUTF("Performance report generation failed");
    }
}

} // extern "C"

} // namespace io
} // namespace jni
} // namespace lattice