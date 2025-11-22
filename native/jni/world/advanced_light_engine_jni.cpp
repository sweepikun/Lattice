#include "advanced_light_engine_jni.hpp"
#include "../FinalLatticeJNI_Fixed.h"

namespace lattice {
namespace jni {
namespace world {

// ========== 优化的光照引擎类 ==========
class OptimizedAdvancedLightEngineJNI {
private:
    // 使用框架的智能内存管理
    std::unique_ptr<jni_framework::JNIMemoryManager> memory_manager_;
    std::unique_ptr<jni_framework::JVMThreadPool> lighting_pool_;
    std::unique_ptr<jni_framework::PerformanceMetrics> metrics_;
    
    // 框架配置
    jni_framework::FrameworkConfig config_;
    
    // 光照引擎实例
    lattice::world::AdvancedLightEngine* light_engine_;
    
public:
    OptimizedAdvancedLightEngineJNI() {
        // 配置框架
        config_.memory.max_pool_size = 128 * 1024 * 1024; // 128MB 光照计算缓冲区
        config_.threading.thread_count = std::thread::hardware_concurrency();
        config_.performance.enable_metrics = true;
        
        // 初始化组件
        memory_manager_ = std::make_unique<jni_framework::JNIMemoryManager>(config_.memory);
        lighting_pool_ = std::make_unique<jni_framework::JVMThreadPool>(config_.threading.thread_count);
        metrics_ = std::make_unique<jni_framework::PerformanceMetrics>();
        
        // 初始化光照引擎
        light_engine_ = new lattice::world::AdvancedLightEngine();
    }
    
    ~OptimizedAdvancedLightEngineJNI() {
        delete light_engine_;
    }
    
    // 优化的不透明度表初始化
    void initOpacityTableOptimized(JNIEnv* env, jintArray materialIds, jbyteArray opacities) {
        auto start_time = std::chrono::high_resolution_clock::now();
        
        try {
            jsize idLength = env->GetArrayLength(materialIds);
            jsize opacityLength = env->GetArrayLength(opacities);
            
            if (idLength != opacityLength) {
                std::cerr << "Material IDs and opacities array length mismatch" << std::endl;
                return;
            }
            
            if (idLength == 0) {
                std::cout << "Empty opacity table, skipping initialization" << std::endl;
                return;
            }
            
            // 使用SafeBuffer进行零拷贝操作
            jint* material_ids = env->GetIntArrayElements(materialIds, nullptr);
            jbyte* opacity_values = env->GetByteArrayElements(opacities, nullptr);
            
            size_t table_size = static_cast<size_t>(idLength);
            auto table_buffer = memory_manager_->allocateDirect(table_size * sizeof(jni_framework::SafeBuffer), "opacity_table");
            
            // 批量处理不透明度数据
            for (jsize i = 0; i < idLength; ++i) {
                auto buffer = memory_manager_->allocateDirect(sizeof(jbyte), "opacity_entry");
                *static_cast<jbyte*>(buffer) = opacity_values[i];
                
                // 使用光照线程池异步处理
                lighting_pool_->submit([this, buffer, material_ids[i], light_engine_]() {
                    try {
                        // 设置材料不透明度
                        light_engine_->setOpacity(material_ids[i], *static_cast<const jbyte*>(buffer));
                        
                        auto task_end = std::chrono::high_resolution_clock::now();
                        metrics_->recordLatency(task_end - std::chrono::high_resolution_clock::now());
                        
                    } catch (const std::exception& e) {
                        std::cerr << "Failed to set opacity for material " << material_ids[i] << ": " << e.what() << std::endl;
                    }
                    
                    // 清理缓冲区
                    memory_manager_->deallocate(buffer);
                });
            }
            
            // 释放原始数组
            env->ReleaseIntArrayElements(materialIds, material_ids, JNI_ABORT);
            env->ReleaseByteArrayElements(opacities, opacity_values, JNI_ABORT);
            
            // 清理表缓冲区
            memory_manager_->deallocate(table_buffer);
            
        } catch (const jni_framework::JNIFrameworkException& e) {
            std::cerr << "Optimized opacity table initialization failed: " << e.what() << std::endl;
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto total_duration = end_time - start_time;
        metrics_->recordLatency(total_duration);
    }
    
    // 优化的光照计算
    void calculateLightOptimized(JNIEnv* env, jint worldId, jintArray blockX, jintArray blockY, jintArray blockZ, jobjectArray results) {
        auto start_time = std::chrono::high_resolution_clock::now();
        
        try {
            jsize coordLength = env->GetArrayLength(blockX);
            if (coordLength == 0 || coordLength != env->GetArrayLength(blockY) || 
                coordLength != env->GetArrayLength(blockZ) || coordLength != env->GetArrayLength(results)) {
                std::cerr << "Coordinate arrays length mismatch" << std::endl;
                return;
            }
            
            // 使用线程池进行批量光照计算
            lighting_pool_->submit([this, env, worldId, blockX, blockY, blockZ, results, coordLength]() {
                try {
                    auto task_start = std::chrono::high_resolution_clock::now();
                    
                    // 批量获取坐标
                    jint* x_coords = env->GetIntArrayElements(blockX, nullptr);
                    jint* y_coords = env->GetIntArrayElements(blockY, nullptr);
                    jint* z_coords = env->GetIntArrayElements(blockZ, nullptr);
                    
                    // 批量计算光照
                    for (jsize i = 0; i < coordLength; ++i) {
                        try {
                            auto block_start = std::chrono::high_resolution_clock::now();
                            
                            // 计算光照值
                            int lightValue = light_engine_->calculateBlockLight(
                                worldId, x_coords[i], y_coords[i], z_coords[i]
                            );
                            
                            // 设置结果
                            jobject resultObj = env->GetObjectArrayElement(results, i);
                            if (resultObj) {
                                // 假设有setLightValue方法
                                jmethodID setLightMethod = env->GetMethodID(
                                    env->GetObjectClass(resultObj), "setLightValue", "(I)V"
                                );
                                if (setLightMethod) {
                                    env->CallVoidMethod(resultObj, setLightMethod, lightValue);
                                }
                                env->DeleteLocalRef(resultObj);
                            }
                            
                            auto block_end = std::chrono::high_resolution_clock::now();
                            metrics_->recordLatency(block_end - block_start);
                            
                        } catch (const std::exception& e) {
                            std::cerr << "Failed to calculate light for block (" 
                                      << x_coords[i] << ", " << y_coords[i] << ", " << z_coords[i] 
                                      << "): " << e.what() << std::endl;
                        }
                    }
                    
                    // 释放数组
                    env->ReleaseIntArrayElements(blockX, x_coords, JNI_ABORT);
                    env->ReleaseIntArrayElements(blockY, y_coords, JNI_ABORT);
                    env->ReleaseIntArrayElements(blockZ, z_coords, JNI_ABORT);
                    
                    auto task_end = std::chrono::high_resolution_clock::now();
                    metrics_->recordLatency(task_end - task_start);
                    
                } catch (const std::exception& e) {
                    std::cerr << "Batch lighting calculation failed: " << e.what() << std::endl;
                }
            });
            
        } catch (const jni_framework::JNIFrameworkException& e) {
            std::cerr << "Optimized lighting calculation setup failed: " << e.what() << std::endl;
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto total_duration = end_time - start_time;
        metrics_->recordLatency(total_duration);
    }
    
    // 优化的路径查找
    void findPathOptimized(JNIEnv* env, jint worldId, jint startX, jint startY, jint startZ,
                          jint endX, jint endY, jint endZ, jobject pathCallback) {
        auto start_time = std::chrono::high_resolution_clock::now();
        
        try {
            // 构建路径查找任务
            struct PathFindTask {
                int worldId;
                int startX, startY, startZ;
                int endX, endY, endZ;
                jobject pathCallback;
                jni_framework::JNIMemoryManager* memory_manager;
                lattice::world::AdvancedLightEngine* light_engine;
            };
            
            PathFindTask task{
                .worldId = worldId,
                .startX = startX,
                .startY = startY,
                .startZ = startZ,
                .endX = endX,
                .endY = endY,
                .endZ = endZ,
                .pathCallback = pathCallback,
                .memory_manager = memory_manager_.get(),
                .light_engine = light_engine_
            };
            
            // 异步路径查找
            lighting_pool_->submit([task, this, env]() {
                try {
                    auto task_start = std::chrono::high_resolution_clock::now();
                    
                    // 执行路径查找
                    auto path = task.light_engine->findPath(
                        task.worldId,
                        task.startX, task.startY, task.startZ,
                        task.endX, task.endY, task.endZ
                    );
                    
                    // 使用SafeBuffer存储路径数据
                    if (!path.empty()) {
                        size_t path_size = path.size() * sizeof(std::tuple<int, int, int>);
                        auto path_buffer = task.memory_manager->allocateDirect(path_size, "path_data");
                        
                        // 复制路径数据
                        memcpy(path_buffer, path.data(), path_size);
                        
                        // 异步回调通知完成
                        if (task.pathCallback && env) {
                            env->CallVoidMethod(task.pathCallback, 
                                              env->GetMethodID(env->GetObjectClass(task.pathCallback), 
                                                              "onPathFound", "([III)V"), 1);
                        }
                        
                        // 清理路径缓冲区
                        task.memory_manager->deallocate(path_buffer);
                    } else {
                        // 无路径
                        if (task.pathCallback && env) {
                            env->CallVoidMethod(task.pathCallback, 
                                              env->GetMethodID(env->GetObjectClass(task.pathCallback), 
                                                              "onPathFound", "([III)V"), 0);
                        }
                    }
                    
                    auto task_end = std::chrono::high_resolution_clock::now();
                    metrics_->recordLatency(task_end - task_start);
                    
                } catch (const std::exception& e) {
                    std::cerr << "Optimized path finding failed: " << e.what() << std::endl;
                    
                    if (task.pathCallback && env) {
                        env->CallVoidMethod(task.pathCallback, 
                                          env->GetMethodID(env->GetObjectClass(task.pathCallback), 
                                                          "onPathFound", "([III)V"), -1);
                    }
                }
            });
            
        } catch (const jni_framework::JNIFrameworkException& e) {
            std::cerr << "Optimized path finding setup failed: " << e.what() << std::endl;
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto total_duration = end_time - start_time;
        metrics_->recordLatency(total_duration);
    }
    
    // 获取性能报告
    std::string getPerformanceReport() const {
        std::ostringstream report;
        report << "Optimized AdvancedLightEngineJNI Performance Report:\n";
        report << "  Total Operations: " << metrics_->total_operations.load() << "\n";
        report << "  Average Latency: " << metrics_->getAverageLatencyUs() << " μs\n";
        report << "  Memory Usage: " << memory_manager_->getCurrentUsage() << " bytes\n";
        report << "  Peak Memory: " << memory_manager_->getPeakUsage() << " bytes\n";
        report << "  Lighting Pool Size: " << lighting_pool_->getThreadCount() << "\n";
        return report.str();
    }
};

// 全局优化实例
static std::unique_ptr<OptimizedAdvancedLightEngineJNI> g_optimized_light_engine;

// ========== 优化后的JNI实现 ==========

extern "C" {

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    try {
        // 初始化优化实例
        g_optimized_light_engine = std::make_unique<OptimizedAdvancedLightEngineJNI>();
        std::cout << "Optimized AdvancedLightEngineJNI loaded with advanced performance features!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Failed to initialize Optimized AdvancedLightEngineJNI: " << e.what() << std::endl;
    }
    
    return JNI_VERSION_1_8;
}

JNIEXPORT void JNICALL JNI_OnUnload(JavaVM* vm, void* reserved) {
    std::cout << "Optimized AdvancedLightEngineJNI unloading..." << std::endl;
    g_optimized_light_engine.reset();
}

JNIEXPORT void JNICALL 
Java_io_lattice_world_AdvancedLightEngine_initOpacityTable(
    JNIEnv* env, jclass, jintArray materialIds, jbyteArray opacities) {
    
    try {
        if (!g_optimized_light_engine) {
            throw std::runtime_error("Optimized AdvancedLightEngineJNI not initialized");
        }
        
        g_optimized_light_engine->initOpacityTableOptimized(env, materialIds, opacities);
        
    } catch (const std::exception& e) {
        std::cerr << "Optimized opacity table initialization failed: " << e.what() << std::endl;
        // 回退到原始实现
        if (g_lightEngine) {
            jsize idLength = env->GetArrayLength(materialIds);
            jsize opacityLength = env->GetArrayLength(opacities);
            
            if (idLength == opacityLength && idLength > 0) {
                jint* material_ids = env->GetIntArrayElements(materialIds, nullptr);
                jbyte* opacity_values = env->GetByteArrayElements(opacities, nullptr);
                
                for (jsize i = 0; i < idLength; ++i) {
                    g_lightEngine->setOpacity(material_ids[i], opacity_values[i]);
                }
                
                env->ReleaseIntArrayElements(materialIds, material_ids, JNI_ABORT);
                env->ReleaseByteArrayElements(opacities, opacity_values, JNI_ABORT);
            }
        }
    }
}

JNIEXPORT void JNICALL
Java_io_lattice_world_AdvancedLightEngine_calculateLightAsync(
    JNIEnv* env, jclass, jint worldId, jintArray blockX, jintArray blockY, 
    jintArray blockZ, jobjectArray results) {
    
    try {
        if (!g_optimized_light_engine) {
            throw std::runtime_error("Optimized AdvancedLightEngineJNI not initialized");
        }
        
        g_optimized_light_engine->calculateLightOptimized(env, worldId, blockX, blockY, blockZ, results);
        
    } catch (const std::exception& e) {
        std::cerr << "Optimized lighting calculation failed: " << e.what() << std::endl;
        // 回退到原始实现
        if (g_lightEngine) {
            jsize coordLength = env->GetArrayLength(blockX);
            if (coordLength > 0 && coordLength == env->GetArrayLength(blockY) && 
                coordLength == env->GetArrayLength(blockZ) && coordLength == env->GetArrayLength(results)) {
                
                jint* x_coords = env->GetIntArrayElements(blockX, nullptr);
                jint* y_coords = env->GetIntArrayElements(blockY, nullptr);
                jint* z_coords = env->GetIntArrayElements(blockZ, nullptr);
                
                for (jsize i = 0; i < coordLength; ++i) {
                    int lightValue = g_lightEngine->calculateBlockLight(
                        worldId, x_coords[i], y_coords[i], z_coords[i]
                    );
                    
                    jobject resultObj = env->GetObjectArrayElement(results, i);
                    if (resultObj) {
                        jmethodID setLightMethod = env->GetMethodID(
                            env->GetObjectClass(resultObj), "setLightValue", "(I)V"
                        );
                        if (setLightMethod) {
                            env->CallVoidMethod(resultObj, setLightMethod, lightValue);
                        }
                        env->DeleteLocalRef(resultObj);
                    }
                }
                
                env->ReleaseIntArrayElements(blockX, x_coords, JNI_ABORT);
                env->ReleaseIntArrayElements(blockY, y_coords, JNI_ABORT);
                env->ReleaseIntArrayElements(blockZ, z_coords, JNI_ABORT);
            }
        }
    }
}

JNIEXPORT void JNICALL
Java_io_lattice_world_AdvancedLightEngine_findPathAsync(
    JNIEnv* env, jclass, jint worldId, jint startX, jint startY, jint startZ,
    jint endX, jint endY, jint endZ, jobject pathCallback) {
    
    try {
        if (!g_optimized_light_engine) {
            throw std::runtime_error("Optimized AdvancedLightEngineJNI not initialized");
        }
        
        g_optimized_light_engine->findPathOptimized(env, worldId, startX, startY, startZ,
                                                   endX, endY, endZ, pathCallback);
        
    } catch (const std::exception& e) {
        std::cerr << "Optimized path finding failed: " << e.what() << std::endl;
        // 回退到原始实现
        if (g_lightEngine) {
            auto path = g_lightEngine->findPath(worldId, startX, startY, startZ, endX, endY, endZ);
            if (!path.empty() && pathCallback) {
                env->CallVoidMethod(pathCallback, 
                                  env->GetMethodID(env->GetObjectClass(pathCallback), 
                                                  "onPathFound", "([III)V"), 1);
            } else if (pathCallback) {
                env->CallVoidMethod(pathCallback, 
                                  env->GetMethodID(env->GetObjectClass(pathCallback), 
                                                  "onPathFound", "([III)V"), 0);
            }
        }
    }
}

JNIEXPORT jstring JNICALL
Java_io_lattice_world_AdvancedLightEngine_getPerformanceReport(
    JNIEnv* env, jclass) {
    try {
        if (g_optimized_light_engine) {
            std::string report = g_optimized_light_engine->getPerformanceReport();
            return env->NewStringUTF(report.c_str());
        }
        return env->NewStringUTF("Optimized AdvancedLightEngineJNI not initialized");
    } catch (const std::exception& e) {
        std::cerr << "Failed to get performance report: " << e.what() << std::endl;
        return env->NewStringUTF("Performance report generation failed");
    }
}

} // extern "C"

} // namespace world
} // namespace jni
} // namespace lattice