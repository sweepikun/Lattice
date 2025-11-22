#pragma once
#include <jni.h>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <vector>
#include <memory>
#include <iostream>
#include <atomic>

namespace lattice {
namespace jni {

// ========== 修复的内存管理类 ==========

class SafeJNIMemoryManager {
private:
    struct MemoryBlock {
        void* ptr;
        size_t size;
        bool isJNIReference;
        MemoryBlock* next;
    };
    
    MemoryBlock* head_;
    std::mutex mutex_;
    std::atomic<size_t> totalAllocated_{0};
    std::atomic<size_t> totalFreed_{0};
    
    static constexpr size_t MAX_BLOCK_SIZE = 16 * 1024 * 1024; // 16MB
    static constexpr size_t SAFETY_MARGIN = 1024; // 1KB safety margin
    
public:
    SafeJNIMemoryManager() : head_(nullptr) {
        std::cout << "[SafeJNIMemory] Initialized with safety bounds checking" << std::endl;
    }
    
    ~SafeJNIMemoryManager() {
        cleanup();
    }
    
    // 安全的内存分配
    void* allocate(size_t size, const char* tag = "unknown") {
        if (size == 0) {
            std::cerr << "[SafeJNIMemory] Warning: Trying to allocate 0 bytes for " << tag << std::endl;
            return nullptr;
        }
        
        if (size > MAX_BLOCK_SIZE) {
            std::cerr << "[SafeJNIMemory] Error: Allocation too large (" << size << " bytes) for " << tag << std::endl;
            return nullptr;
        }
        
        // 添加安全边界
        size_t safeSize = size + SAFETY_MARGIN;
        void* ptr = malloc(safeSize);
        
        if (!ptr) {
            std::cerr << "[SafeJNIMemory] Error: Failed to allocate " << safeSize << " bytes for " << tag << std::endl;
            return nullptr;
        }
        
        // 清零内存（安全措施）
        memset(ptr, 0, safeSize);
        
        // 记录分配
        MemoryBlock* block = new MemoryBlock;
        block->ptr = ptr;
        block->size = safeSize;
        block->isJNIReference = false;
        block->next = nullptr;
        
        {
            std::lock_guard<std::mutex> lock(mutex_);
            block->next = head_;
            head_ = block;
        }
        
        totalAllocated_.fetch_add(safeSize);
        
        std::cout << "[SafeJNIMemory] Allocated " << safeSize << " bytes for " << tag << std::endl;
        
        // 返回带有安全边界的指针
        return static_cast<char*>(ptr) + (SAFETY_MARGIN / 2);
    }
    
    // 安全的内存释放
    void deallocate(void* ptr, size_t originalSize, const char* tag = "unknown") {
        if (!ptr) {
            std::cerr << "[SafeJNIMemory] Warning: Trying to deallocate nullptr for " << tag << std::endl;
            return;
        }
        
        // 恢复原始指针（减去安全边界）
        void* originalPtr = static_cast<char*>(ptr) - (SAFETY_MARGIN / 2);
        
        {
            std::lock_guard<std::mutex> lock(mutex_);
            MemoryBlock** current = &head_;
            
            while (*current) {
                if ((*current)->ptr == originalPtr) {
                    MemoryBlock* toDelete = *current;
                    *current = (*current)->next;
                    
                    // 安全清零
                    memset(toDelete->ptr, 0xDD, toDelete->size);
                    free(toDelete->ptr);
                    delete toDelete;
                    
                    totalFreed_.fetch_add(originalSize + SAFETY_MARGIN);
                    
                    std::cout << "[SafeJNIMemory] Freed " << (originalSize + SAFETY_MARGIN) 
                             << " bytes for " << tag << std::endl;
                    return;
                }
                current = &((*current)->next);
            }
        }
        
        std::cerr << "[SafeJNIMemory] Error: Could not find memory block to free for " << tag << std::endl;
    }
    
    // JNI安全包装器
    jobject allocateDirectByteBuffer(size_t size, JNIEnv* env, const char* tag = "unknown") {
        void* nativePtr = allocate(size, tag);
        if (!nativePtr) {
            return nullptr;
        }
        
        jobject byteBuffer = env->NewDirectByteBuffer(nativePtr, size);
        if (!byteBuffer) {
            deallocate(nativePtr, size, tag);
            std::cerr << "[SafeJNIMemory] Error: Failed to create DirectByteBuffer for " << tag << std::endl;
            return nullptr;
        }
        
        // 记录JNI引用
        std::lock_guard<std::mutex> lock(mutex_);
        MemoryBlock** current = &head_;
        
        while (*current) {
            if (static_cast<char*>((*current)->ptr) + (SAFETY_MARGIN / 2) == nativePtr) {
                (*current)->isJNIReference = true;
                break;
            }
            current = &((*current)->next);
        }
        
        return byteBuffer;
    }
    
    // 获取统计信息
    struct MemoryStats {
        size_t totalAllocated;
        size_t totalFreed;
        size_t currentUsage;
        size_t blockCount;
    };
    
    MemoryStats getStats() const {
        MemoryBlock* current = head_;
        size_t blockCount = 0;
        
        {
            std::lock_guard<std::mutex> lock(mutex_);
            while (current) {
                blockCount++;
                current = current->next;
            }
        }
        
        return MemoryStats{
            totalAllocated_.load(),
            totalFreed_.load(),
            totalAllocated_.load() - totalFreed_.load(),
            blockCount
        };
    }
    
    // 清理所有内存
    void cleanup() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        MemoryBlock* current = head_;
        while (current) {
            MemoryBlock* next = current->next;
            
            // 安全清零和释放
            memset(current->ptr, 0xCC, current->size);
            free(current->ptr);
            delete current;
            
            current = next;
        }
        
        head_ = nullptr;
        
        auto stats = getStats();
        std::cout << "[SafeJNIMemory] Cleanup completed. Total allocated: " 
                 << stats.totalAllocated << " bytes, freed: " << stats.totalFreed 
                 << " bytes, remaining: " << stats.currentUsage << " bytes" << std::endl;
    }
    
    // 验证内存完整性
    bool validateMemory() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        MemoryBlock* current = head_;
        size_t validatedBlocks = 0;
        
        while (current) {
            // 检查指针有效性
            if (!current->ptr) {
                std::cerr << "[SafeJNIMemory] Error: Invalid pointer in block" << std::endl;
                return false;
            }
            
            // 检查大小合理性
            if (current->size < SAFETY_MARGIN || current->size > MAX_BLOCK_SIZE + SAFETY_MARGIN) {
                std::cerr << "[SafeJNIMemory] Error: Invalid size in block: " << current->size << std::endl;
                return false;
            }
            
            validatedBlocks++;
            current = current->next;
        }
        
        std::cout << "[SafeJNIMemory] Memory validation passed. " << validatedBlocks << " blocks validated." << std::endl;
        return true;
    }
};

// ========== 优化的JNI工具类 ==========

class OptimizedJNIUtils {
private:
    static SafeJNIMemoryManager* g_memory_manager;
    static std::mutex g_init_mutex;
    
public:
    static SafeJNIMemoryManager* getMemoryManager() {
        static std::once_flag init_flag;
        std::call_once(init_flag, []() {
            g_memory_manager = new SafeJNIMemoryManager();
            std::cout << "[OptimizedJNI] Global memory manager initialized" << std::endl;
        });
        return g_memory_manager;
    }
    
    // 优化的批处理拷贝（修复版本）
    static int safeBatchCopy(JNIEnv* env, jobjectArray srcs, jobjectArray dsts, 
                            jintArray sizes, jint count) {
        if (count <= 0) return 0;
        
        auto* memoryManager = getMemoryManager();
        
        // 使用安全分配
        void** srcPtrs = (void**)memoryManager->allocate(count * sizeof(void*), "batch_srcs");
        void** dstPtrs = (void**)memoryManager->allocate(count * sizeof(void*), "batch_dsts");
        size_t* sizePtrs = (size_t*)memoryManager->allocate(count * sizeof(size_t), "batch_sizes");
        
        if (!srcPtrs || !dstPtrs || !sizePtrs) {
            std::cerr << "[OptimizedJNI] Error: Failed to allocate batch buffers" << std::endl;
            return -1;
        }
        
        try {
            // 安全地批量获取缓冲区信息
            for (int i = 0; i < count; i++) {
                jobject srcObj = env->GetObjectArrayElement(srcs, i);
                jobject dstObj = env->GetObjectArrayElement(dsts, i);
                
                if (srcObj && dstObj) {
                    srcPtrs[i] = env->GetDirectBufferAddress(srcObj);
                    dstPtrs[i] = env->GetDirectBufferAddress(dstObj);
                    
                    // 验证指针有效性
                    if (!srcPtrs[i] || !dstPtrs[i]) {
                        std::cerr << "[OptimizedJNI] Warning: Invalid buffer at index " << i << std::endl;
                        srcPtrs[i] = nullptr;
                        dstPtrs[i] = nullptr;
                    }
                } else {
                    srcPtrs[i] = nullptr;
                    dstPtrs[i] = nullptr;
                }
                
                env->DeleteLocalRef(srcObj);
                env->DeleteLocalRef(dstObj);
            }
            
            // 批量获取尺寸
            jint* tempSizes = env->GetIntArrayElements(sizes, nullptr);
            for (int i = 0; i < count; i++) {
                if (tempSizes[i] > 0 && tempSizes[i] <= MAX_BLOCK_SIZE) {
                    sizePtrs[i] = tempSizes[i];
                } else {
                    sizePtrs[i] = 0;
                    std::cerr << "[OptimizedJNI] Warning: Invalid size at index " << i 
                             << ": " << tempSizes[i] << std::endl;
                }
            }
            env->ReleaseIntArrayElements(sizes, tempSizes, JNI_ABORT);
            
            // 执行安全的拷贝操作
            int successCount = 0;
            for (int i = 0; i < count; i++) {
                if (srcPtrs[i] && dstPtrs[i] && sizePtrs[i] > 0) {
                    // 使用优化的内存拷贝
                    lattice::optimization::fast_memcpy(dstPtrs[i], srcPtrs[i], sizePtrs[i]);
                    successCount++;
                }
            }
            
            return successCount;
            
        } catch (const std::exception& e) {
            std::cerr << "[OptimizedJNI] Error during batch copy: " << e.what() << std::endl;
            return -1;
        } finally {
            // 安全清理
            memoryManager->deallocate(srcPtrs, count * sizeof(void*), "batch_srcs");
            memoryManager->deallocate(dstPtrs, count * sizeof(void*), "batch_dsts");
            memoryManager->deallocate(sizePtrs, count * sizeof(size_t), "batch_sizes");
        }
    }
    
    // 清理全局资源
    static void cleanup() {
        std::lock_guard<std::mutex> lock(g_init_mutex);
        if (g_memory_manager) {
            g_memory_manager->cleanup();
            delete g_memory_manager;
            g_memory_manager = nullptr;
            std::cout << "[OptimizedJNI] Global memory manager cleaned up" << std::endl;
        }
    }
};

// 静态成员定义
SafeJNIMemoryManager* OptimizedJNIUtils::g_memory_manager = nullptr;
std::mutex OptimizedJNIUtils::g_init_mutex;

} // namespace jni
} // namespace lattice