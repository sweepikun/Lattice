/*
 * RedstoneEngineJNI with Unified Optimization
 * Integrates lattice optimization into redstone engine JNI
 */

#include "redstone_engine_jni.hpp"
#include "../optimization/lattice_optimization.hpp"
#include "../redstone/redstone_engine.hpp"
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <chrono>
#include <atomic>
#include <mutex>
#include <thread>
#include <future>
#include <memory>
#include <functional>
#include <stdexcept>

// Visual Studio compatibility fixes
#ifdef _WIN32
    #define JNIEXPORT __declspec(dllexport)
    #define JNICALL __stdcall
    #define JNI_FALSE 0
    #define JNI_TRUE 1
#else
    #define JNIEXPORT __attribute__((visibility("default")))
    #define JNICALL
#endif

namespace lattice {
namespace redstone {
namespace jni {

// ========== 优化包装器类 ==========

class OptimizedRedstoneJNI {
private:
    // 使用优化框架的内存池
    lattice::optimization::MemoryPool* memory_pool_;
    lattice::optimization::MMAPManager* mmap_manager_;
    
public:
    OptimizedRedstoneJNI() {
        // 初始化优化组件
        memory_pool_ = &lattice::optimization::globalMemoryPool;
        mmap_manager_ = &lattice::optimization::globalMMAPManager;
        
        std::cout << "[RedstoneJNI] 🚀 Optimized Redstone Engine Initialized" << std::endl;
        std::cout << "[RedstoneJNI] 🔧 Memory Pool: Enabled" << std::endl;
        std::cout << "[RedstoneJNI] 🔧 mmap Manager: Enabled" << std::endl;
    }
    
    // 优化的批量红石信号处理
    void processBatchSignalsOptimized(JNIEnv* env, jobjectArray signalBuffers, 
                                     jobjectArray outputBuffers, jintArray sizes, jint count) {
        if (count <= 0) return;
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        try {
            // 使用优化框架进行批处理
            int result = lattice::optimization::JNILatticeOptimizer::batchCopyWithOptimized(
                env, signalBuffers, outputBuffers, sizes, count);
            
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
            
            std::cout << "[RedstoneJNI] ⚡ Optimized batch processing: " << count 
                     << " signals in " << duration.count() << "μs" << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "[RedstoneJNI] ❌ Batch processing failed: " << e.what() << std::endl;
            
            // 回退到逐个处理
            jint* tempSizes = env->GetIntArrayElements(sizes, nullptr);
            for (int i = 0; i < count; i++) {
                jobject srcObj = env->GetObjectArrayElement(signalBuffers, i);
                jobject dstObj = env->GetObjectArrayElement(outputBuffers, i);
                
                void* srcPtr = env->GetDirectBufferAddress(srcObj);
                void* dstPtr = env->GetDirectBufferAddress(dstObj);
                
                if (srcPtr && dstPtr) {
                    lattice::optimization::fast_memcpy(dstPtr, srcPtr, tempSizes[i]);
                }
                
                env->DeleteLocalRef(srcObj);
                env->DeleteLocalRef(dstObj);
            }
            env->ReleaseIntArrayElements(sizes, tempSizes, JNI_ABORT);
        }
    }
    
    // 优化的内存映射红石数据处理
    void processLargeSignalDataOptimized(JNIEnv* env, jobject largeSignalBuffer, 
                                        jobject outputBuffer, jint size) {
        if (size < 1024 * 1024) { // 小于1MB，使用标准处理
            void* srcPtr = env->GetDirectBufferAddress(largeSignalBuffer);
            void* dstPtr = env->GetDirectBufferAddress(outputBuffer);
            
            if (srcPtr && dstPtr) {
                lattice::optimization::fast_memcpy(dstPtr, srcPtr, size);
            }
            return;
        }
        
        // 大于1MB，使用mmap优化
        void* mapped = mmap_manager_->createSharedBuffer(size);
        if (mapped) {
            void* srcPtr = env->GetDirectBufferAddress(largeSignalBuffer);
            void* dstPtr = env->GetDirectBufferAddress(outputBuffer);
            
            if (srcPtr && dstPtr) {
                lattice::optimization::fast_memcpy(mapped, srcPtr, size);
                lattice::optimization::fast_memcpy(dstPtr, mapped, size);
            }
            
            mmap_manager_->releaseSharedBuffer(mapped);
        } else {
            // 回退到标准处理
            void* srcPtr = env->GetDirectBufferAddress(largeSignalBuffer);
            void* dstPtr = env->GetDirectBufferAddress(outputBuffer);
            
            if (srcPtr && dstPtr) {
                lattice::optimization::fast_memcpy(dstPtr, srcPtr, size);
            }
        }
    }
};

// 全局优化实例
static OptimizedRedstoneJNI* g_optimized_redstone_jni = nullptr;

// ========== JNI函数实现 ==========

extern "C" {
    
    // 初始化优化的红石JNI
    JNIEXPORT jboolean JNICALL 
    Java_io_lattice_redstone_nativebridge_RedstoneJNI_initOptimizedEngine(
        JNIEnv* env, jobject obj) {
        
        try {
            if (!g_optimized_redstone_jni) {
                g_optimized_redstone_jni = new OptimizedRedstoneJNI();
            }
            
            std::cout << "[RedstoneJNI] ✅ Optimized Redstone Engine initialized successfully" << std::endl;
            return JNI_TRUE;
            
        } catch (const std::exception& e) {
            std::cerr << "[RedstoneJNI] ❌ Failed to initialize optimized engine: " << e.what() << std::endl;
            return JNI_FALSE;
        }
    }
    
    // 优化的批量信号处理
    JNIEXPORT void JNICALL 
    Java_io_lattice_redstone_nativebridge_RedstoneJNI_processBatchSignalsOptimized(
        JNIEnv* env, jobject obj, jobjectArray signalBuffers, jobjectArray outputBuffers, 
        jintArray sizes, jint count) {
        
        if (!g_optimized_redstone_jni) {
            std::cerr << "[RedstoneJNI] ❌ Optimized engine not initialized" << std::endl;
            return;
        }
        
        g_optimized_redstone_jni->processBatchSignalsOptimized(env, signalBuffers, outputBuffers, sizes, count);
    }
    
    // 优化的内存映射处理
    JNIEXPORT void JNICALL 
    Java_io_lattice_redstone_nativebridge_RedstoneJNI_processLargeSignalDataOptimized(
        JNIEnv* env, jobject obj, jobject largeSignalBuffer, jobject outputBuffer, jint size) {
        
        if (!g_optimized_redstone_jni) {
            std::cerr << "[RedstoneJNI] ❌ Optimized engine not initialized" << std::endl;
            return;
        }
        
        g_optimized_redstone_jni->processLargeSignalDataOptimized(env, largeSignalBuffer, outputBuffer, size);
    }
    
    // 获取优化信息
    JNIEXPORT jstring JNICALL 
    Java_io_lattice_redstone_nativebridge_RedstoneJNI_getOptimizationInfo(
        JNIEnv* env, jobject obj) {
        
        std::string info = lattice::optimization::JNILatticeOptimizer::getOptimizationInfo();
        info += "\n\n红石引擎优化:\n";
        info += "• 批量信号处理: 支持多种策略\n";
        info += "• 内存映射优化: 大数据块mmap处理\n";
        info += "• 多线程并行: 信号并行计算\n";
        
        return env->NewStringUTF(info.c_str());
    }
    
    // 获取CPU架构信息
    JNIEXPORT jstring JNICALL 
    Java_io_lattice_redstone_nativebridge_RedstoneJNI_getCPUArchitecture(
        JNIEnv* env, jobject obj) {
        
        std::string info = lattice::optimization::JNILatticeOptimizer::getCPUInfo();
        return env->NewStringUTF(info.c_str());
    }
    
    // 清理资源
    JNIEXPORT void JNICALL 
    Java_io_lattice_redstone_nativebridge_RedstoneJNI_cleanupOptimizedEngine(
        JNIEnv* env, jobject obj) {
        
        if (g_optimized_redstone_jni) {
            delete g_optimized_redstone_jni;
            g_optimized_redstone_jni = nullptr;
            std::cout << "[RedstoneJNI] 🧹 Optimized engine cleaned up" << std::endl;
        }
    }
}

} // namespace jni
} // namespace redstone  
} // namespace lattice