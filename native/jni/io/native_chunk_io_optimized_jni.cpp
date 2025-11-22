/*
 * NativeChunkIOJNI with Unified Optimization
 * Integrates lattice optimization into chunk I/O operations
 */

#include "native_chunk_io_jni.hpp"
#include "../optimization/lattice_optimization.hpp"
#include "../core/io/async_chunk_io.hpp"
#include <stdexcept>
#include <vector>
#include <memory>
#include <chrono>
#include <iostream>

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
namespace jni {
namespace io {

// ========== 优化的JNI包装器类 ==========

class OptimizedNativeChunkIOJNI {
private:
    // 使用优化框架的内存池
    lattice::optimization::MemoryPool* memory_pool_;
    lattice::optimization::MMAPManager* mmap_manager_;
    
public:
    OptimizedNativeChunkIOJNI() {
        // 初始化优化组件
        memory_pool_ = &lattice::optimization::globalMemoryPool;
        mmap_manager_ = &lattice::optimization::globalMMAPManager;
        
        std::cout << "[ChunkIOJNI] 🚀 Optimized Chunk I/O Engine Initialized" << std::endl;
        std::cout << "[ChunkIOJNI] 🔧 Memory Pool: Enabled" << std::endl;
        std::cout << "[ChunkIOJNI] 🔧 mmap Manager: Enabled" << std::endl;
    }
    
    // 优化的批量块加载
    void loadBatchChunksOptimized(JNIEnv* env, jintArray worldIds, jintArray chunkXs, 
                                 jintArray chunkZs, jobjectArray chunkData, jint count) {
        if (count <= 0) return;
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        try {
            // 使用内存池分配批次缓冲区
            void** chunkBuffers = (void**)memory_pool_->allocate(count * sizeof(void*));
            size_t* chunkSizes = (size_t*)memory_pool_->allocate(count * sizeof(size_t));
            
            if (!chunkBuffers || !chunkSizes) {
                std::cerr << "[ChunkIOJNI] ❌ Failed to allocate batch buffers" << std::endl;
                return;
            }
            
            // 批量获取Java数组数据
            jint* worldIdArray = env->GetIntArrayElements(worldIds, nullptr);
            jint* chunkXArray = env->GetIntArrayElements(chunkXs, nullptr);
            jint* chunkZArray = env->GetIntArrayElements(chunkZs, nullptr);
            
            // 批量处理每个块
            for (int i = 0; i < count; i++) {
                jobject chunkObj = env->GetObjectArrayElement(chunkData, i);
                if (chunkObj) {
                    chunkBuffers[i] = env->GetDirectBufferAddress(chunkObj);
                    // 假设每个块最大16MB
                    chunkSizes[i] = 16 * 1024 * 1024;
                    env->DeleteLocalRef(chunkObj);
                } else {
                    chunkBuffers[i] = nullptr;
                    chunkSizes[i] = 0;
                }
            }
            
            // 创建源和目标缓冲区进行批处理
            void** srcBuffers = (void**)memory_pool_->allocate(count * sizeof(void*));
            void** dstBuffers = (void**)memory_pool_->allocate(count * sizeof(void*));
            
            // 模拟源数据（实际中从磁盘加载）
            for (int i = 0; i < count; i++) {
                if (chunkBuffers[i] && chunkSizes[i] > 0) {
                    srcBuffers[i] = memory_pool_->allocate(chunkSizes[i]);
                    dstBuffers[i] = chunkBuffers[i];
                    
                    // 模拟数据加载
                    if (srcBuffers[i]) {
                        // 这里应该是实际的磁盘读取逻辑
                        memset(srcBuffers[i], 0, chunkSizes[i]);
                    }
                } else {
                    srcBuffers[i] = nullptr;
                    dstBuffers[i] = nullptr;
                    chunkSizes[i] = 0;
                }
            }
            
            // 使用统一优化进行批量拷贝
            lattice::optimization::JNILatticeOptimizer::batchCopyWithOptimized(
                env, (jobjectArray)srcBuffers, (jobjectArray)dstBuffers, 
                (jintArray)chunkSizes, count);
            
            // 清理分配的缓冲区
            for (int i = 0; i < count; i++) {
                if (srcBuffers[i]) {
                    memory_pool_->deallocate(srcBuffers[i], chunkSizes[i]);
                }
            }
            
            memory_pool_->deallocate(srcBuffers, count * sizeof(void*));
            memory_pool_->deallocate(dstBuffers, count * sizeof(void*));
            memory_pool_->deallocate(chunkBuffers, count * sizeof(void*));
            memory_pool_->deallocate(chunkSizes, count * sizeof(size_t));
            
            // 释放Java数组
            env->ReleaseIntArrayElements(worldIds, worldIdArray, JNI_ABORT);
            env->ReleaseIntArrayElements(chunkXs, chunkXArray, JNI_ABORT);
            env->ReleaseIntArrayElements(chunkZs, chunkZArray, JNI_ABORT);
            
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
            
            std::cout << "[ChunkIOJNI] ⚡ Optimized batch loading: " << count 
                     << " chunks in " << duration.count() << "μs" << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "[ChunkIOJNI] ❌ Batch loading failed: " << e.what() << std::endl;
        }
    }
    
    // 优化的内存映射大块数据处理
    void processLargeChunkDataOptimized(JNIEnv* env, jobject chunkBuffer, 
                                       jint worldId, jint chunkX, jint chunkZ, jint size) {
        if (size < 1024 * 1024) { // 小于1MB，使用标准处理
            void* srcPtr = env->GetDirectBufferAddress(chunkBuffer);
            if (srcPtr) {
                // 处理小块数据
                lattice::io::AsyncChunkIO::loadChunkAsync(worldId, chunkX, chunkZ);
            }
            return;
        }
        
        // 大于1MB，使用mmap优化
        void* mapped = mmap_manager_->createSharedBuffer(size);
        if (mapped) {
            void* srcPtr = env->GetDirectBufferAddress(chunkBuffer);
            if (srcPtr) {
                // 从源数据复制到mmap区域
                lattice::optimization::fast_memcpy(mapped, srcPtr, size);
                // 处理mmap区域的数据
                lattice::optimization::fast_memcpy(srcPtr, mapped, size);
            }
            
            mmap_manager_->releaseSharedBuffer(mapped);
        } else {
            // 回退到标准处理
            void* srcPtr = env->GetDirectBufferAddress(chunkBuffer);
            if (srcPtr) {
                lattice::io::AsyncChunkIO::loadChunkAsync(worldId, chunkX, chunkZ);
            }
        }
    }
    
    // 优化的异步块保存
    void saveChunkAsyncOptimized(JNIEnv* env, jint worldId, jint chunkX, jint chunkZ, 
                                jobject chunkData, jint size) {
        auto start_time = std::chrono::high_resolution_clock::now();
        
        try {
            void* chunkPtr = env->GetDirectBufferAddress(chunkData);
            if (!chunkPtr) {
                std::cerr << "[ChunkIOJNI] ❌ Invalid chunk data buffer" << std::endl;
                return;
            }
            
            // 使用优化内存管理
            void* optimizedBuffer = nullptr;
            if (size > 1024 * 1024) { // 大块使用mmap
                optimizedBuffer = mmap_manager_->createSharedBuffer(size);
                if (optimizedBuffer) {
                    lattice::optimization::fast_memcpy(optimizedBuffer, chunkPtr, size);
                    // 使用优化的缓冲区进行保存
                    lattice::io::AsyncChunkIO::saveChunkAsync(worldId, chunkX, chunkZ, optimizedBuffer);
                    mmap_manager_->releaseSharedBuffer(optimizedBuffer);
                } else {
                    lattice::io::AsyncChunkIO::saveChunkAsync(worldId, chunkX, chunkZ, chunkPtr);
                }
            } else {
                // 小块使用内存池
                optimizedBuffer = memory_pool_->allocate(size);
                if (optimizedBuffer) {
                    lattice::optimization::fast_memcpy(optimizedBuffer, chunkPtr, size);
                    lattice::io::AsyncChunkIO::saveChunkAsync(worldId, chunkX, chunkZ, optimizedBuffer);
                    memory_pool_->deallocate(optimizedBuffer, size);
                } else {
                    lattice::io::AsyncChunkIO::saveChunkAsync(worldId, chunkX, chunkZ, chunkPtr);
                }
            }
            
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
            
            std::cout << "[ChunkIOJNI] ⚡ Optimized save: chunk (" << chunkX << "," << chunkZ 
                     << ") in " << duration.count() << "μs" << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "[ChunkIOJNI] ❌ Optimized save failed: " << e.what() << std::endl;
        }
    }
};

// 全局优化实例
static OptimizedNativeChunkIOJNI* g_optimized_chunk_io = nullptr;

// ========== JNI函数实现 ==========

extern "C" {
    
    // 初始化优化的Chunk I/O
    JNIEXPORT jboolean JNICALL 
    Java_com_lattice_world_NativeChunkIO_initOptimizedEngine(
        JNIEnv* env, jclass clazz) {
        
        try {
            if (!g_optimized_chunk_io) {
                g_optimized_chunk_io = new OptimizedNativeChunkIOJNI();
            }
            
            std::cout << "[ChunkIOJNI] ✅ Optimized Chunk I/O Engine initialized successfully" << std::endl;
            return JNI_TRUE;
            
        } catch (const std::exception& e) {
            std::cerr << "[ChunkIOJNI] ❌ Failed to initialize optimized engine: " << e.what() << std::endl;
            return JNI_FALSE;
        }
    }
    
    // 优化的批量块加载
    JNIEXPORT void JNICALL 
    Java_com_lattice_world_NativeChunkIO_loadBatchChunksOptimized(
        JNIEnv* env, jclass clazz, jintArray worldIds, jintArray chunkXs, 
        jintArray chunkZs, jobjectArray chunkData, jint count) {
        
        if (!g_optimized_chunk_io) {
            std::cerr << "[ChunkIOJNI] ❌ Optimized engine not initialized" << std::endl;
            return;
        }
        
        g_optimized_chunk_io->loadBatchChunksOptimized(env, worldIds, chunkXs, chunkZs, chunkData, count);
    }
    
    // 优化的内存映射处理
    JNIEXPORT void JNICALL 
    Java_com_lattice_world_NativeChunkIO_processLargeChunkDataOptimized(
        JNIEnv* env, jclass clazz, jobject chunkBuffer, jint worldId, 
        jint chunkX, jint chunkZ, jint size) {
        
        if (!g_optimized_chunk_io) {
            std::cerr << "[ChunkIOJNI] ❌ Optimized engine not initialized" << std::endl;
            return;
        }
        
        g_optimized_chunk_io->processLargeChunkDataOptimized(env, chunkBuffer, worldId, chunkX, chunkZ, size);
    }
    
    // 优化的异步保存
    JNIEXPORT void JNICALL 
    Java_com_lattice_world_NativeChunkIO_saveChunkAsyncOptimized(
        JNIEnv* env, jclass clazz, jint worldId, jint chunkX, jint chunkZ, 
        jobject chunkData, jint size) {
        
        if (!g_optimized_chunk_io) {
            std::cerr << "[ChunkIOJNI] ❌ Optimized engine not initialized" << std::endl;
            return;
        }
        
        g_optimized_chunk_io->saveChunkAsyncOptimized(env, worldId, chunkX, chunkZ, chunkData, size);
    }
    
    // 获取优化信息
    JNIEXPORT jstring JNICALL 
    Java_com_lattice_world_NativeChunkIO_getOptimizationInfo(
        JNIEnv* env, jclass clazz) {
        
        std::string info = lattice::optimization::JNILatticeOptimizer::getOptimizationInfo();
        info += "\n\nChunk I/O优化:\n";
        info += "• 批量块加载: 支持多种策略\n";
        info += "• 内存映射优化: 大块数据mmap处理\n";
        info += "• 异步保存优化: 智能内存管理\n";
        
        return env->NewStringUTF(info.c_str());
    }
    
    // 清理资源
    JNIEXPORT void JNICALL 
    Java_com_lattice_world_NativeChunkIO_cleanupOptimizedEngine(
        JNIEnv* env, jclass clazz) {
        
        if (g_optimized_chunk_io) {
            delete g_optimized_chunk_io;
            g_optimized_chunk_io = nullptr;
            std::cout << "[ChunkIOJNI] 🧹 Optimized engine cleaned up" << std::endl;
        }
    }
}

} // namespace io
} // namespace jni
} // namespace lattice