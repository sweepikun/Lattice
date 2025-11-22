#include "async_compression_jni.hpp"
#include "../FinalLatticeJNI_Fixed.h"

namespace lattice {
namespace jni {
namespace net {

// ========== 优化的异步压缩器类 ==========
class OptimizedAsyncCompressionJNI {
private:
    // 使用框架的智能内存管理
    std::unique_ptr<jni_framework::JNIMemoryManager> memory_manager_;
    std::unique_ptr<jni_framework::JVMThreadPool> compression_pool_;
    std::unique_ptr<jni_framework::PerformanceMetrics> metrics_;
    
    // 框架配置
    jni_framework::FrameworkConfig config_;
    
    // 回调引用
    static jclass callbackClass_;
    static jmethodID callbackMethod_;
    static JavaVM* jvm_;
    
public:
    OptimizedAsyncCompressionJNI() {
        // 配置框架
        config_.memory.max_pool_size = 64 * 1024 * 1024; // 64MB 压缩缓冲区
        config_.threading.thread_count = std::thread::hardware_concurrency();
        config_.performance.enable_metrics = true;
        
        // 初始化组件
        memory_manager_ = std::make_unique<jni_framework::JNIMemoryManager>(config_.memory);
        compression_pool_ = std::make_unique<jni_framework::JVMThreadPool>(config_.threading.thread_count);
        metrics_ = std::make_unique<jni_framework::PerformanceMetrics>();
    }
    
    ~OptimizedAsyncCompressionJNI() = default;
    
    // 优化的异步压缩
    void compressAsyncOptimized(JNIEnv* env, jbyteArray inputData, jboolean compress, jobject callback) {
        auto start_time = std::chrono::high_resolution_clock::now();
        
        try {
            if (!inputData) {
                throw std::invalid_argument("Input data cannot be null");
            }
            
            jsize input_size = env->GetArrayLength(inputData);
            if (input_size == 0) {
                // 空数据直接回调成功
                if (callback) {
                    env->CallVoidMethod(callback, callbackMethod_, compress, 0);
                }
                return;
            }
            
            // 使用SafeBuffer进行零拷贝操作
            jbyte* input_bytes = env->GetByteArrayElements(inputData, nullptr);
            auto input_buffer = memory_manager_->allocateDirect(input_size, "compression_input");
            
            // 复制数据到缓冲区
            memcpy(input_buffer, input_bytes, input_size);
            env->ReleaseByteArrayElements(inputData, input_bytes, JNI_ABORT);
            
            // 构建压缩任务
            struct CompressionTask {
                void* input_buffer;
                size_t input_size;
                jboolean compress;
                jobject callback;
                jni_framework::JNIMemoryManager* memory_manager;
            };
            
            CompressionTask task{
                .input_buffer = input_buffer,
                .input_size = static_cast<size_t>(input_size),
                .compress = compress,
                .callback = callback,
                .memory_manager = memory_manager_.get()
            };
            
            // 提交到压缩线程池
            compression_pool_->submit([task, this, env]() {
                try {
                    auto task_start = std::chrono::high_resolution_clock::now();
                    
                    size_t output_size = task.input_size;
                    auto output_buffer = task.memory_manager->allocateDirect(output_size, "compression_output");
                    
                    // 执行压缩/解压缩
                    if (task.compress) {
                        // 压缩逻辑
                        lattice::net::AsyncCompressor::compressSync(
                            static_cast<const uint8_t*>(task.input_buffer),
                            task.input_size,
                            static_cast<uint8_t*>(output_buffer),
                            &output_size
                        );
                    } else {
                        // 解压缩逻辑
                        lattice::net::AsyncCompressor::decompressSync(
                            static_cast<const uint8_t*>(task.input_buffer),
                            task.input_size,
                            static_cast<uint8_t*>(output_buffer),
                            &output_size
                        );
                    }
                    
                    // 创建结果数组
                    jbyteArray result = env->NewByteArray(static_cast<jsize>(output_size));
                    env->SetByteArrayRegion(result, 0, static_cast<jsize>(output_size), 
                                          static_cast<const jbyte*>(output_buffer));
                    
                    auto task_end = std::chrono::high_resolution_clock::now();
                    auto duration = task_end - task_start;
                    
                    // 记录性能指标
                    metrics_->recordLatency(duration);
                    
                    // 异步回调
                    if (task.callback) {
                        JNIEnv* callback_env = nullptr;
                        if (jvm_->GetEnv((void**)&callback_env, JNI_VERSION_1_8) == JNI_OK) {
                            callback_env->CallVoidMethod(task.callback, callbackMethod_, task.compress, static_cast<jint>(output_size));
                        }
                    }
                    
                    // 清理资源
                    task.memory_manager->deallocate(output_buffer);
                    
                } catch (const std::exception& e) {
                    std::cerr << "Optimized compression failed: " << e.what() << std::endl;
                    
                    // 错误回调
                    if (task.callback) {
                        JNIEnv* callback_env = nullptr;
                        if (jvm_->GetEnv((void**)&callback_env, JNI_VERSION_1_8) == JNI_OK) {
                            callback_env->CallVoidMethod(task.callback, callbackMethod_, task.compress, -1);
                        }
                    }
                }
                
                // 清理输入缓冲区
                task.memory_manager->deallocate(task.input_buffer);
            });
            
        } catch (const jni_framework::JNIFrameworkException& e) {
            std::cerr << "Optimized compression setup failed: " << e.what() << std::endl;
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto total_duration = end_time - start_time;
        metrics_->recordLatency(total_duration);
    }
    
    // 获取性能报告
    std::string getPerformanceReport() const {
        std::ostringstream report;
        report << "Optimized AsyncCompressionJNI Performance Report:\n";
        report << "  Total Operations: " << metrics_->total_operations.load() << "\n";
        report << "  Average Latency: " << metrics_->getAverageLatencyUs() << " μs\n";
        report << "  Memory Usage: " << memory_manager_->getCurrentUsage() << " bytes\n";
        report << "  Peak Memory: " << memory_manager_->getPeakUsage() << " bytes\n";
        report << "  Compression Pool Size: " << compression_pool_->getThreadCount() << "\n";
        return report.str();
    }
};

// 静态成员初始化
jclass OptimizedAsyncCompressionJNI::callbackClass_ = nullptr;
jmethodID OptimizedAsyncCompressionJNI::callbackMethod_ = nullptr;
JavaVM* OptimizedAsyncCompressionJNI::jvm_ = nullptr;

// 全局优化实例
static std::unique_ptr<OptimizedAsyncCompressionJNI> g_optimized_compression;

// ========== 优化后的JNI实现 ==========

extern "C" {

// JNI_OnLoad函数，在库加载时调用
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    jvm_ = vm;
    
    try {
        // 初始化优化实例
        g_optimized_compression = std::make_unique<OptimizedAsyncCompressionJNI>();
        std::cout << "Optimized AsyncCompressionJNI loaded with advanced performance features!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Failed to initialize Optimized AsyncCompressionJNI: " << e.what() << std::endl;
    }
    
    return JNI_VERSION_1_8;
}

// 初始化异步压缩器
JNIEXPORT void JNICALL Java_io_lattice_network_AsyncCompression_initializeAsyncCompressor
  (JNIEnv *env, jclass clazz, jint workerCount) {
    try {
        // 获取回调类和方法的引用
        if (OptimizedAsyncCompressionJNI::callbackClass_ == nullptr) {
            jclass localCallbackClass = env->FindClass("io/lattice/network/AsyncCompression$CompressionCallback");
            if (localCallbackClass != nullptr) {
                OptimizedAsyncCompressionJNI::callbackClass_ = (jclass)env->NewGlobalRef(localCallbackClass);
                OptimizedAsyncCompressionJNI::callbackMethod_ = env->GetMethodID(OptimizedAsyncCompressionJNI::callbackClass_, "onComplete", "(ZI)V");
            }
        }
        
        // 原始初始化逻辑保持不变
        if (workerCount > 0) {
            lattice::net::AsyncCompressor::initialize(workerCount);
        }
        
        std::cout << "Optimized AsyncCompressionJNI initialized with " << workerCount << " workers" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Failed to initialize Optimized AsyncCompressionJNI: " << e.what() << std::endl;
        // 回退到原始实现
        if (workerCount > 0) {
            lattice::net::AsyncCompressor::initialize(workerCount);
        }
    }
}

// 优化的异步压缩
JNIEXPORT void JNICALL Java_io_lattice_network_AsyncCompression_compressAsync
  (JNIEnv *env, jclass clazz, jbyteArray inputData, jboolean compress, jobject callback) {
    try {
        if (!g_optimized_compression) {
            throw std::runtime_error("Optimized AsyncCompressionJNI not initialized");
        }
        
        g_optimized_compression->compressAsyncOptimized(env, inputData, compress, callback);
        
    } catch (const std::exception& e) {
        std::cerr << "Optimized compression failed: " << e.what() << std::endl;
        // 回退到原始实现
        lattice::net::AsyncCompressor::compressAsync(env, inputData, compress, callback);
    }
}

// 获取性能报告
JNIEXPORT jstring JNICALL Java_io_lattice_network_AsyncCompression_getPerformanceReport
  (JNIEnv *env, jclass clazz) {
    try {
        if (g_optimized_compression) {
            std::string report = g_optimized_compression->getPerformanceReport();
            return env->NewStringUTF(report.c_str());
        }
        return env->NewStringUTF("Optimized AsyncCompressionJNI not initialized");
    } catch (const std::exception& e) {
        std::cerr << "Failed to get performance report: " << e.what() << std::endl;
        return env->NewStringUTF("Performance report generation failed");
    }
}

// 获取压缩统计信息
JNIEXPORT jlong JNICALL Java_io_lattice_network_AsyncCompression_getTotalOperations
  (JNIEnv *env, jclass clazz) {
    if (g_optimized_compression) {
        // 这里应该实现获取metrics的方法
        return 0; // 占位符
    }
    return 0;
}

// JNI_OnUnload函数
JNIEXPORT void JNICALL JNI_OnUnload(JavaVM* vm, void* reserved) {
    if (OptimizedAsyncCompressionJNI::callbackClass_) {
        // 清理全局引用
        vm->GetEnv((void**)&env, JNI_VERSION_1_8);
        // 清理逻辑...
    }
    
    g_optimized_compression.reset();
    std::cout << "Optimized AsyncCompressionJNI unloaded" << std::endl;
}

} // extern "C"

} // namespace net
} // namespace jni
} // namespace lattice