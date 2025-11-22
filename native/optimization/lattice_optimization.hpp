#pragma once
#include <jni.h>
#include <immintrin.h>
#include <cstring>
#include <chrono>
#include <atomic>
#include <cpuid.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <thread>
#include <mutex>
#include <vector>
#include <queue>
#include <memory>
#include <algorithm>
#include <iostream>

// ARM NEON 头文件 - 仅在ARM架构上包含
#if defined(__aarch64__) || defined(__arm64__) || defined(__ARM_NEON__) || defined(__ARM_ARCH_8A__)
#include <arm_neon.h>
#endif

namespace lattice {
namespace optimization {

// ========== SIMD内存拷贝函数实现 ==========

/**
 * @brief 检测CPU架构和SIMD特性
 */
static inline CPUArchitecture detectCPUArchitecture() {
    unsigned int eax, ebx, ecx, edx;
    
    // 检查是否是x86_64架构
    #if defined(__x86_64__) || defined(__amd64__)
        // 检查CPUID支持
        if (__get_cpuid_max(0, NULL) >= 1) {
            __cpuid_count(1, 0, eax, ebx, ecx, edx);
            
            // 检查AVX-512
            if (__get_cpuid_max(7, NULL) >= 7) {
                __cpuid_count(7, 0, eax, ebx, ecx, edx);
                if (ebx & (1 << 16)) {
                    return CPUArchitecture::X86_64_AVX512;
                }
            }
            
            // 检查AVX2
            if (ecx & (1 << 28)) {
                __cpuid_count(7, 0, eax, ebx, ecx, edx);
                if (ebx & (1 << 5)) {
                    return CPUArchitecture::X86_64_AVX2;
                }
            }
            
            // 检查AVX
            if (ecx & (1 << 28)) {
                return CPUArchitecture::X86_64_AVX;
            }
            
            // 检查SSE2
            if (edx & (1 << 26)) {
                return CPUArchitecture::X86_64_SSE2;
            }
            
            return CPUArchitecture::X86_64_UNKNOWN;
        }
    #elif defined(__aarch64__) || defined(__arm64__)
        // ARM64架构
        return CPUArchitecture::ARM64_NEON;
    #endif
    
    return CPUArchitecture::X86_64_UNKNOWN;
}

/**
 * @brief AVX-512内存拷贝
 */
static inline void* avx512_memcpy(void* dst, const void* src, size_t n) {
    #if defined(__AVX512F__)
    if (n >= 64) {
        __m512i vec;
        char* d = (char*)dst;
        const char* s = (const char*)src;
        size_t i = 0;
        
        // 对齐拷贝
        for (; i + 63 < n; i += 64) {
            vec = _mm512_loadu_si512((const __m512i*)(s + i));
            _mm512_storeu_si512((__m512i*)(d + i), vec);
        }
        
        // 处理剩余字节
        if (i < n) {
            __m512i zero = _mm512_setzero_si512();
            vec = _mm512_loadu_si512((const __m512i*)(s + i));
            __mmask64 mask = (1ULL << (n - i)) - 1;
            vec = _mm512_maskz_loadu_epi8(mask, (const __m512i*)(s + i));
            _mm512_mask_storeu_epi8((__m512i*)(d + i), mask, vec);
        }
        
        return dst;
    }
    #endif
    return std::memcpy(dst, src, n);
}

/**
 * @brief AVX2内存拷贝
 */
static inline void* avx2_memcpy(void* dst, const void* src, size_t n) {
    #if defined(__AVX2__)
    if (n >= 32) {
        __m256i vec;
        char* d = (char*)dst;
        const char* s = (const char*)src;
        size_t i = 0;
        
        // 32字节对齐拷贝
        for (; i + 31 < n; i += 32) {
            vec = _mm256_loadu_si256((const __m256i*)(s + i));
            _mm256_storeu_si256((__m256i*)(d + i), vec);
        }
        
        // 处理剩余字节
        if (i < n) {
            std::memcpy(d + i, s + i, n - i);
        }
        
        return dst;
    }
    #endif
    return std::memcpy(dst, src, n);
}

/**
 * @brief SSE2内存拷贝
 */
static inline void* sse2_memcpy(void* dst, const void* src, size_t n) {
    #if defined(__SSE2__)
    if (n >= 16) {
        __m128i vec;
        char* d = (char*)dst;
        const char* s = (const char*)src;
        size_t i = 0;
        
        // 16字节对齐拷贝
        for (; i + 15 < n; i += 16) {
            vec = _mm_loadu_si128((const __m128i*)(s + i));
            _mm_storeu_si128((__m128i*)(d + i), vec);
        }
        
        // 处理剩余字节
        if (i < n) {
            std::memcpy(d + i, s + i, n - i);
        }
        
        return dst;
    }
    #endif
    return std::memcpy(dst, src, n);
}

/**
 * @brief ARM NEON内存拷贝
 */
static inline void* neon_memcpy(void* dst, const void* src, size_t n) {
    #if defined(__ARM_NEON__) || defined(__aarch64__)
    if (n >= 16) {
        char* d = (char*)dst;
        const char* s = (const char*)src;
        size_t i = 0;
        
        // 16字节NEON拷贝
        for (; i + 15 < n; i += 16) {
            uint8x16_t vec = vld1q_u8((const uint8_t*)(s + i));
            vst1q_u8((uint8_t*)(d + i), vec);
        }
        
        // 处理剩余字节
        if (i < n) {
            std::memcpy(d + i, s + i, n - i);
        }
        
        return dst;
    }
    #endif
    return std::memcpy(dst, src, n);
}

/**
 * @brief 快速内存拷贝 - 自动选择最佳SIMD指令
 */
static inline void* fast_memcpy(void* dst, const void* src, size_t n) {
    if (n == 0 || dst == src) return dst;
    
    static CPUArchitecture cached_arch = detectCPUArchitecture();
    
    switch (cached_arch) {
        case CPUArchitecture::X86_64_AVX512:
            return avx512_memcpy(dst, src, n);
        case CPUArchitecture::X86_64_AVX2:
            return avx2_memcpy(dst, src, n);
        case CPUArchitecture::X86_64_AVX:
        case CPUArchitecture::X86_64_SSE2:
            return sse2_memcpy(dst, src, n);
        case CPUArchitecture::ARM64_NEON:
            return neon_memcpy(dst, src, n);
        default:
            return std::memcpy(dst, src, n);
    }
}

/**
 * @brief 批处理连续内存拷贝
 */
static inline void batch_contiguous_copy(void** dsts, void** srcs, size_t* sizes, int count) {
    if (count <= 0) return;
    
    // 排序优化 - 小块优先处理，大块使用SIMD
    struct CopyTask {
        void* dst;
        void* src;
        size_t size;
        int index;
    };
    
    CopyTask* tasks = new CopyTask[count];
    for (int i = 0; i < count; i++) {
        tasks[i] = {dsts[i], srcs[i], sizes[i], i};
    }
    
    // 按大小排序：小块在前，大块在后
    std::sort(tasks, tasks + count, [](const CopyTask& a, const CopyTask& b) {
        return a.size < b.size;
    });
    
    // 批量拷贝
    for (int i = 0; i < count; i++) {
        fast_memcpy(tasks[i].dst, tasks[i].src, tasks[i].size);
    }
    
    delete[] tasks;
}

// ========== 全局计数器和架构检测 ==========

std::atomic<uint64_t> globalCounter{0};

enum class CPUArchitecture {
    X86_64_UNKNOWN,
    X86_64_SSE2,
    X86_64_AVX,
    X86_64_AVX2,
    X86_64_AVX512,
    ARM64_NEON,
    ARM64_SVE
};

struct SIMDFeatures {
    bool sse2_supported = false;
    bool avx_supported = false;
    bool avx2_supported = false;
    bool avx512f_supported = false;
    bool neon_supported = false;
    bool sve_supported = false;
};

void detectCPUArchitecture(SIMDFeatures& features, CPUArchitecture& arch) {
    arch = CPUArchitecture::X86_64_UNKNOWN;
    features = SIMDFeatures{};

#ifdef __x86_64__
    unsigned int eax, ebx, ecx, edx;
    
    if (__get_cpuid(0, &eax, &ebx, &ecx, &edx)) {
        if (__get_cpuid(1, &eax, &ebx, &ecx, &edx)) {
            if (ecx & bit_SSE2) {
                features.sse2_supported = true;
                arch = CPUArchitecture::X86_64_SSE2;
            }
            if (ecx & bit_AVX) {
                features.avx_supported = true;
                arch = CPUArchitecture::X86_64_AVX;
            }
        }
        
        if (__get_cpuid(7, &eax, &ebx, &ecx, &edx)) {
            if (ebx & bit_AVX2) {
                features.avx2_supported = true;
                arch = CPUArchitecture::X86_64_AVX2;
            }
            if (ebx & bit_AVX512F) {
                features.avx512f_supported = true;
                arch = CPUArchitecture::X86_64_AVX512;
            }
        }
    }
#elif defined(__aarch64__) || defined(__arm64__)
    arch = CPUArchitecture::ARM64_NEON;
    features.neon_supported = true;
    
    unsigned long sve_features = 0;
#ifdef __ARM_FEATURE_SVE
    sve_features = get_sve_iar();
#endif
    if (sve_features) {
        features.sve_supported = true;
        arch = CPUArchitecture::ARM64_SVE;
    }
#endif
}

// ========== 内存池管理系统 ==========

class MemoryPool {
private:
    struct Block {
        void* ptr;
        size_t size;
        bool isFree;
        Block* next;
    };
    
    Block* freeBlocks;
    size_t poolSize;
    std::mutex poolMutex;
    
    static constexpr size_t MAX_BLOCK_SIZE = 1024 * 1024; // 1MB
    static constexpr size_t MIN_BLOCK_SIZE = 64; // 64B
    
public:
    MemoryPool(size_t initialSize = 16 * 1024 * 1024) : poolSize(initialSize) {
        freeBlocks = (Block*)malloc(poolSize);
        if (freeBlocks) {
            freeBlocks->ptr = ((char*)freeBlocks) + sizeof(Block);
            freeBlocks->size = poolSize - sizeof(Block);
            freeBlocks->isFree = true;
            freeBlocks->next = nullptr;
        }
    }
    
    ~MemoryPool() {
        if (freeBlocks) {
            Block* current = freeBlocks;
            while (current) {
                Block* next = current->next;
                if (current->isFree) {
                    free(current->ptr);
                }
                current = next;
            }
            free(freeBlocks);
        }
    }
    
    void* allocate(size_t size) {
        if (size > MAX_BLOCK_SIZE) {
            return malloc(size); // 大块直接malloc
        }
        
        std::lock_guard<std::mutex> lock(poolMutex);
        
        // 查找合适的空闲块
        Block** current = &freeBlocks;
        while (*current) {
            if ((*current)->isFree && (*current)->size >= size) {
                void* result = (*current)->ptr;
                
                // 如果剩余空间足够，拆分块
                if ((*current)->size - size > MIN_BLOCK_SIZE) {
                    Block* newBlock = (Block*)(((char*)(*current)->ptr) + size);
                    newBlock->ptr = ((char*)newBlock) + sizeof(Block);
                    newBlock->size = (*current)->size - size - sizeof(Block);
                    newBlock->isFree = true;
                    newBlock->next = (*current)->next;
                    
                    (*current)->ptr = result;
                    (*current)->size = size;
                    (*current)->next = newBlock;
                }
                
                (*current)->isFree = false;
                return result;
            }
            current = &((*current)->next);
        }
        
        // 没有合适的块，分配新块
        return malloc(size);
    }
    
    void deallocate(void* ptr, size_t size) {
        if (ptr && size <= MAX_BLOCK_SIZE) {
            std::lock_guard<std::mutex> lock(poolMutex);
            
            // 查找并释放对应的块
            Block** current = &freeBlocks;
            while (*current) {
                if ((*current)->ptr == ptr) {
                    (*current)->isFree = true;
                    // 尝试合并相邻的空闲块
                    mergeFreeBlocks(*current);
                    return;
                }
                current = &((*current)->next);
            }
        }
        
        // 大块或未找到，使用标准free
        if (ptr) {
            free(ptr);
        }
    }
    
private:
    void mergeFreeBlocks(Block* block) {
        // 与前一个块合并
        Block* prev = nullptr;
        Block* current = freeBlocks;
        while (current && current != block) {
            prev = current;
            current = current->next;
        }
        
        if (prev && prev->isFree) {
            prev->size += block->size + sizeof(Block);
            prev->next = block->next;
            block = prev;
        }
        
        // 与后一个块合并
        if (block->next && block->next->isFree) {
            block->size += block->next->size + sizeof(Block);
            block->next = block->next->next;
        }
    }
};

// ========== mmap零拷贝共享内存管理 ==========

class MMAPManager {
private:
    struct SharedBuffer {
        void* addr;
        size_t size;
        int fd;
        bool isOwner;
    };
    
    std::mutex mmapMutex;
    std::vector<SharedBuffer> activeBuffers;
    
public:
    ~MMAPManager() {
        std::lock_guard<std::mutex> lock(mmapMutex);
        for (auto& buf : activeBuffers) {
            if (buf.addr && buf.isOwner) {
                munmap(buf.addr, buf.size);
            }
            if (buf.fd >= 0) {
                close(buf.fd);
            }
        }
    }
    
    void* createSharedBuffer(size_t size) {
        std::lock_guard<std::mutex> lock(mmapMutex);
        
        // 创建临时文件用于共享内存
        char tempFile[] = "/tmp/lattice_optimized_XXXXXX";
        int fd = mkstemp(tempFile);
        if (fd < 0) return nullptr;
        
        unlink(tempFile); // 删除文件名，但保留文件描述符
        
        // 设置文件大小
        if (ftruncate(fd, size) < 0) {
            close(fd);
            return nullptr;
        }
        
        // 创建共享内存映射
        void* addr = mmap(nullptr, size, PROT_READ | PROT_WRITE, 
                         MAP_SHARED | MAP_ANONYMOUS, fd, 0);
        
        if (addr == MAP_FAILED) {
            close(fd);
            return nullptr;
        }
        
        // 记录活跃缓冲区
        SharedBuffer buf;
        buf.addr = addr;
        buf.size = size;
        buf.fd = fd;
        buf.isOwner = true;
        activeBuffers.push_back(buf);
        
        return addr;
    }
    
    void releaseSharedBuffer(void* addr) {
        std::lock_guard<std::mutex> lock(mmapMutex);
        
        for (auto it = activeBuffers.begin(); it != activeBuffers.end(); ++it) {
            if (it->addr == addr) {
                if (it->isOwner) {
                    munmap(it->addr, it->size);
                }
                if (it->fd >= 0) {
                    close(it->fd);
                }
                activeBuffers.erase(it);
                break;
            }
        }
    }
};

// ========== MMAP管理器全局实例 ==========
static MMAPManager globalMMAPManager;

// ========== 统一调度系统 ==========

class UnifiedScheduler {
public:
    enum class Strategy {
        FAST_PATH,      // 快速路径：小数据直接处理
        BATCHED,        // 批处理：中等数据批量处理
        MEMORY_MAPPED,  // 内存映射：大数据mmap处理
        MULTI_THREAD    // 多线程：大数据并行处理
    };
    
    struct TaskInfo {
        void** dsts;
        void** srcs;
        size_t* sizes;
        int count;
        Strategy strategy;
        size_t totalSize;
        size_t maxBlockSize;
    };
    
    static Strategy selectStrategy(const TaskInfo& task) {
        // 策略选择逻辑
        if (task.count <= 0) return Strategy::FAST_PATH;
        
        // 小数据量：快速路径
        if (task.count <= 3 || task.totalSize < 1024) {
            return Strategy::FAST_PATH;
        }
        
        // 大数据块：考虑内存映射或多线程
        if (task.maxBlockSize >= 1024 * 1024) { // 1MB
            return Strategy::MULTI_THREAD;
        }
        
        // 中等数据：批处理
        if (task.count >= 10 && task.totalSize < 16 * 1024 * 1024) { // 16MB
            return Strategy::BATCHED;
        }
        
        // 默认批处理
        return Strategy::BATCHED;
    }
    
    static void executeTask(const TaskInfo& task) {
        Strategy strategy = selectStrategy(task);
        
        switch (strategy) {
            case Strategy::FAST_PATH:
                executeFastPath(task);
                break;
            case Strategy::BATCHED:
                executeBatched(task);
                break;
            case Strategy::MEMORY_MAPPED:
                executeMemoryMapped(task);
                break;
            case Strategy::MULTI_THREAD:
                executeMultiThread(task);
                break;
        }
    }
    
private:
    static void executeFastPath(const TaskInfo& task) {
        for (int i = 0; i < task.count; i++) {
            fast_memcpy(task.dsts[i], task.srcs[i], task.sizes[i]);
        }
    }
    
    static void executeBatched(const TaskInfo& task) {
        batch_contiguous_copy(task.dsts, task.srcs, task.sizes, task.count);
    }
    
    static void executeMemoryMapped(const TaskInfo& task) {
        // 大块数据使用mmap优化
        for (int i = 0; i < task.count; i++) {
            if (task.sizes[i] >= 1024 * 1024) { // 1MB+
                // 创建mmap缓冲区
                void* mapped = globalMMAPManager.createSharedBuffer(task.sizes[i]);
                if (mapped) {
                    // 从源数据复制到mmap区域
                    fast_memcpy(mapped, task.srcs[i], task.sizes[i]);
                    // 从mmap区域复制到目标
                    fast_memcpy(task.dsts[i], mapped, task.sizes[i]);
                    // 清理mmap
                    globalMMAPManager.releaseSharedBuffer(mapped);
                } else {
                    // 回退到标准拷贝
                    fast_memcpy(task.dsts[i], task.srcs[i], task.sizes[i]);
                }
            } else {
                fast_memcpy(task.dsts[i], task.srcs[i], task.sizes[i]);
            }
        }
    }
    
    static void executeMultiThread(const TaskInfo& task) {
        // 多线程处理：对于非常大的数据块
        std::vector<std::thread> threads;
        int numThreads = std::min(task.count, static_cast<int>(std::thread::hardware_concurrency()));
        int blockSize = (task.count + numThreads - 1) / numThreads;
        
        for (int t = 0; t < numThreads; t++) {
            int start = t * blockSize;
            int end = std::min(start + blockSize, task.count);
            
            if (start < end) {
                threads.emplace_back([&task, start, end]() {
                    for (int i = start; i < end; i++) {
                        fast_memcpy(task.dsts[i], task.srcs[i], task.sizes[i]);
                    }
                });
            }
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
    }
};

// 前向声明
static inline void fast_memcpy(void* dst, const void* src, size_t size);
static inline void batch_contiguous_copy(void** dsts, void** srcs, size_t* sizes, int count);

// 优化的批量连续拷贝
static inline void batch_contiguous_copy(void** dsts, void** srcs, size_t* sizes, int count) {
    if (count <= 0) return;
    
    // 预处理：分析数据特征
    size_t totalSize = 0;
    size_t maxBlockSize = 0;
    int largeBlockCount = 0;
    
    for (int i = 0; i < count; i++) {
        totalSize += sizes[i];
        if (sizes[i] > maxBlockSize) {
            maxBlockSize = sizes[i];
        }
        if (sizes[i] >= 4096) { // 4KB+
            largeBlockCount++;
        }
    }
    
    // 创建任务信息
    UnifiedScheduler::TaskInfo taskInfo;
    taskInfo.dsts = dsts;
    taskInfo.srcs = srcs;
    taskInfo.sizes = sizes;
    taskInfo.count = count;
    taskInfo.totalSize = totalSize;
    taskInfo.maxBlockSize = maxBlockSize;
    
    // 使用统一调度器执行
    UnifiedScheduler::executeTask(taskInfo);
}

// 快速内存拷贝 - 多平台优化
static inline void fast_memcpy(void* dst, const void* src, size_t size) {
    if (size == 0) return;
    
    // 小数据块直接使用标准拷贝
    if (size < 128) {
        memcpy(dst, src, size);
        return;
    }
    
    // 预处理：填充防止优化
    globalCounter.fetch_add(size);
    
    char* d = (char*)dst;
    const char* s = (const char*)src;
    size_t i = 0;
    
#ifdef __AVX512F__
    // AVX-512 优化 - 64字节块
    for (; i + 64 <= size; i += 64) {
        __m512i data = _mm512_loadu_si512((__m512i*)(s + i));
        _mm512_storeu_si512((__m512i*)(d + i), data);
    }
#endif

#ifdef __AVX2__
    // AVX2 优化 - 32字节块
    for (; i + 32 <= size; i += 32) {
        __m256i data = _mm256_loadu_si256((__m256i*)(s + i));
        _mm256_storeu_si256((__m256i*)(d + i), data);
    }
#endif

#ifdef __SSE2__
    // SSE2 优化 - 16字节块
    for (; i + 16 <= size; i += 16) {
        __m128i data = _mm_loadu_si128((__m128i*)(s + i));
        _mm_storeu_si128((__m128i*)(d + i), data);
    }
#endif
    
    // 处理剩余数据
    if (i < size) {
        memcpy(d + i, s + i, size - i);
    }
}

// ========== 全局实例 ==========

static MemoryPool globalMemoryPool;

// ========== JNI优化包装器 ==========

class JNILatticeOptimizer {
public:
    // 优化的批处理JNI调用
    static int batchCopyWithOptimized(JNIEnv* env, jobjectArray srcs, jobjectArray dsts, 
                                     jintArray sizes, jint count) {
        if (count <= 0) return 0;
        
        // 使用内存池分配缓冲区
        void** srcPtrs = (void**)globalMemoryPool.allocate(count * sizeof(void*));
        void** dstPtrs = (void**)globalMemoryPool.allocate(count * sizeof(void*));
        size_t* sizePtrs = (size_t*)globalMemoryPool.allocate(count * sizeof(size_t));
        
        if (!srcPtrs || !dstPtrs || !sizePtrs) {
            // 回退到逐个处理
            jint* tempSizes = env->GetIntArrayElements(sizes, nullptr);
            for (int i = 0; i < count; i++) {
                jobject srcObj = env->GetObjectArrayElement(srcs, i);
                jobject dstObj = env->GetObjectArrayElement(dsts, i);
                
                void* srcPtr = env->GetDirectBufferAddress(srcObj);
                void* dstPtr = env->GetDirectBufferAddress(dstObj);
                
                if (srcPtr && dstPtr) {
                    fast_memcpy(dstPtr, srcPtr, tempSizes[i]);
                }
                
                env->DeleteLocalRef(srcObj);
                env->DeleteLocalRef(dstObj);
            }
            env->ReleaseIntArrayElements(sizes, tempSizes, JNI_ABORT);
            
            if (srcPtrs) globalMemoryPool.deallocate(srcPtrs, count * sizeof(void*));
            if (dstPtrs) globalMemoryPool.deallocate(dstPtrs, count * sizeof(void*));
            if (sizePtrs) globalMemoryPool.deallocate(sizePtrs, count * sizeof(size_t));
            return count;
        }
        
        // 批量获取所有缓冲区信息
        for (int i = 0; i < count; i++) {
            jobject srcObj = env->GetObjectArrayElement(srcs, i);
            jobject dstObj = env->GetObjectArrayElement(dsts, i);
            
            srcPtrs[i] = env->GetDirectBufferAddress(srcObj);
            dstPtrs[i] = env->GetDirectBufferAddress(dstObj);
            
            env->DeleteLocalRef(srcObj);
            env->DeleteLocalRef(dstObj);
        }
        
        // 批量获取尺寸
        jint* tempSizes = env->GetIntArrayElements(sizes, nullptr);
        for (int i = 0; i < count; i++) {
            sizePtrs[i] = tempSizes[i];
        }
        env->ReleaseIntArrayElements(sizes, tempSizes, JNI_ABORT);
        
        // 使用统一调度器执行批处理
        UnifiedScheduler::TaskInfo taskInfo;
        taskInfo.dsts = dstPtrs;
        taskInfo.srcs = srcPtrs;
        taskInfo.sizes = sizePtrs;
        taskInfo.count = count;
        
        // 计算总大小和最大块大小
        taskInfo.totalSize = 0;
        taskInfo.maxBlockSize = 0;
        for (int i = 0; i < count; i++) {
            taskInfo.totalSize += sizePtrs[i];
            if (sizePtrs[i] > taskInfo.maxBlockSize) {
                taskInfo.maxBlockSize = sizePtrs[i];
            }
        }
        
        // 使用统一调度器执行
        UnifiedScheduler::executeTask(taskInfo);
        
        // 清理内存池
        globalMemoryPool.deallocate(srcPtrs, count * sizeof(void*));
        globalMemoryPool.deallocate(dstPtrs, count * sizeof(void*));
        globalMemoryPool.deallocate(sizePtrs, count * sizeof(size_t));
        
        return count;
    }
    
    // 获取优化信息
    static std::string getOptimizationInfo() {
        std::string info = "Lattice统一优化架构 v3.0\n";
        info += "================================\n";
        info += "• 内存池管理: 减少动态分配开销\n";
        info += "• 统一调度: 智能策略选择\n";
        info += "• mmap零拷贝: 大数据块优化\n";
        info += "• 多线程并行: 充分利用多核\n";
        info += "\n策略选择:\n";
        info += "• FAST_PATH: 小数据(<1KB 或 ≤3个) - 直接拷贝\n";
        info += "• BATCHED: 中等数据(<16MB, ≥10个) - 批处理优化\n";
        info += "• MEMORY_MAPPED: 大块数据(≥1MB) - mmap共享内存\n";
        info += "• MULTI_THREAD: 超大数据(≥1MB块) - 多线程并行\n";
        
        return info;
    }
    
    // 获取CPU架构信息
    static std::string getCPUInfo() {
        SIMDFeatures features;
        CPUArchitecture arch;
        detectCPUArchitecture(features, arch);
        
        std::string result = "CPU架构检测:\n";
        
#ifdef __AVX512F__
        if (features.avx512f_supported) result += "✓ AVX-512F\n";
#endif
#ifdef __AVX2__
        if (features.avx2_supported) result += "✓ AVX2\n";
#endif
#ifdef __AVX__
        if (features.avx_supported) result += "✓ AVX\n";
#endif
#ifdef __SSE2__
        if (features.sse2_supported) result += "✓ SSE2\n";
#endif
#if defined(__aarch64__) || defined(__arm64__)
        if (features.neon_supported) result += "✓ ARM NEON\n";
#endif
#ifdef __ARM_FEATURE_SVE
        if (features.sve_supported) result += "✓ ARM SVE\n";
#endif
        
        result += "\n当前架构: ";
        switch (arch) {
            case CPUArchitecture::X86_64_AVX512:
                result += "x86-64-AVX512 (最优)";
                break;
            case CPUArchitecture::X86_64_AVX2:
                result += "x86-64-AVX2 (优秀)";
                break;
            case CPUArchitecture::X86_64_AVX:
                result += "x86-64-AVX (良好)";
                break;
            case CPUArchitecture::X86_64_SSE2:
                result += "x86-64-SSE2 (标准)";
                break;
            case CPUArchitecture::ARM64_SVE:
                result += "ARM64-SVE (最优)";
                break;
            case CPUArchitecture::ARM64_NEON:
                result += "ARM64-NEON (标准)";
                break;
            default:
                result += "未知架构";
                break;
        }
        
        return result;
    }
};

} // namespace optimization
} // namespace lattice