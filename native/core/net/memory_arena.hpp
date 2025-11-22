#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <concepts>
#include <cstdlib>

namespace lattice {
namespace net {

/**
 * MemoryArena - 高性能Arena内存分配器
 * 
 * 设计特性:
 * - 线程局部内存池：避免多线程竞争
 * - 128KB默认块：平衡内存使用和分配效率
 * - 16字节对齐：SIMD友好，优化缓存访问
 * - 内存零拷贝：直接从arena分配内存，避免malloc/free开销
 * - 自动扩缩容：根据使用情况动态调整块数量
 */
class MemoryArena {
public:
    // 默认配置
    static constexpr size_t DEFAULT_BLOCK_SIZE = 128 * 1024;  // 128KB
    static constexpr size_t DEFAULT_ALIGNMENT = 16;           // 16字节对齐
    static constexpr size_t MAX_BLOCKS_PER_THREAD = 32;       // 每个线程最大块数
    
    // 内存块结构
    struct MemoryBlock {
        void* ptr;
        size_t size;
        size_t used;
        size_t remaining;
        
        MemoryBlock(size_t blockSize) 
            : size(blockSize), used(0), remaining(blockSize) {
            // 使用aligned_alloc确保16字节对齐
            ptr = std::aligned_alloc(DEFAULT_ALIGNMENT, blockSize);
            if (!ptr) {
                throw std::bad_alloc();
            }
        }
        
        ~MemoryBlock() {
            if (ptr) {
                std::free(ptr);
                ptr = nullptr;
            }
        }
        
        // 禁止拷贝和移动
        MemoryBlock(const MemoryBlock&) = delete;
        MemoryBlock& operator=(const MemoryBlock&) = delete;
        MemoryBlock(MemoryBlock&&) = delete;
        MemoryBlock& operator=(MemoryBlock&&) = delete;
        
        // 分配内存，返回对齐的指针
        void* allocate(size_t requestedSize) {
            // 计算实际需要的空间（对齐填充）
            size_t alignedSize = (requestedSize + DEFAULT_ALIGNMENT - 1) & ~(DEFAULT_ALIGNMENT - 1);
            
            if (remaining < alignedSize) {
                return nullptr; // 当前块空间不足
            }
            
            void* result = static_cast<char*>(ptr) + used;
            used += alignedSize;
            remaining -= alignedSize;
            return result;
        }
        
        // 获取当前内存使用统计
        size_t getMemoryUsage() const { return used; }
        size_t getTotalSize() const { return size; }
        float getUtilization() const { return static_cast<float>(used) / size; }
    };
    
    // 线程局部块列表
    using ThreadBlocks = std::vector<std::unique_ptr<MemoryBlock>>;
    
    MemoryArena() = default;
    ~MemoryArena() = default;
    
    // 禁止拷贝和移动
    MemoryArena(const MemoryArena&) = delete;
    MemoryArena& operator=(const MemoryArena&) = delete;
    MemoryArena(MemoryArena&&) = delete;
    MemoryArena& operator=(MemoryArena&&) = delete;
    
    /**
     * 从arena分配内存
     * 返回类型无关的指针，使用者需要正确转换
     */
    void* allocate(size_t size);
    
    /**
     * 分配指定类型的对象，自动构造
     * 使用C++20 concepts确保类型安全
     */
    template<typename T, typename... Args>
    requires std::constructible_from<T, Args...>
    T* allocate_object(Args&&... args) {
        void* memory = allocate(sizeof(T));
        if (!memory) {
            return nullptr;
        }
        
        // 放置构造函数
        return new(memory) T(std::forward<Args>(args)...);
    }
    
    /**
     * 分配数组，自动构造所有元素
     */
    template<typename T>
    requires std::constructible_from<T>
    T* allocate_array(size_t count) {
        void* memory = allocate(count * sizeof(T));
        if (!memory) {
            return nullptr;
        }
        
        T* array = static_cast<T*>(memory);
        // 构造所有元素
        for (size_t i = 0; i < count; ++i) {
            new(&array[i]) T();
        }
        return array;
    }
    
    /**
     * 释放整个arena（析构所有对象）
     * 注意：此方法不会自动调用对象的析构函数
     * 使用者需要手动管理对象生命周期
     */
    void clear();
    
    /**
     * 获取内存使用统计
     */
    struct MemoryStats {
        size_t total_allocated;
        size_t total_used;
        size_t block_count;
        size_t thread_count;
        float average_utilization;
        
        size_t getMemoryWaste() const { return total_allocated - total_used; }
        float getOverallUtilization() const { 
            return total_allocated > 0 ? static_cast<float>(total_used) / total_allocated : 0.0f;
        }
    };
    
    MemoryStats getMemoryStats() const;
    
    /**
     * 获取线程局部分配器实例
     * 每个线程都会获得独立的arena实例
     */
    static MemoryArena& forThread();
    
    /**
     * 强制清理当前线程的arena（谨慎使用）
     */
    static void clearCurrentThread();
    
private:
    // 线程局部存储
    thread_local static inline ThreadBlocks* current_thread_blocks_ = nullptr;
    
    // 全局锁用于管理全局统计
    static std::atomic<size_t> global_total_allocated_;
    static std::atomic<size_t> global_total_used_;
    static std::atomic<size_t> global_thread_count_;
    
    // 获取当前线程的块列表（如果不存在则创建）
    ThreadBlocks& getThreadBlocks();
    
    // 创建新的内存块
    std::unique_ptr<MemoryBlock> createNewBlock(size_t blockSize = DEFAULT_BLOCK_SIZE);
    
    // 寻找有足够空间的块
    MemoryBlock* findSuitableBlock(size_t requiredSize);
    
    // 计算当前线程的内存统计
    MemoryStats calculateThreadStats(const ThreadBlocks& blocks) const;
};

/**
 * ArenaScopedPtr - RAII包装器，自动管理arena分配的内存
 */
template<typename T>
class ArenaScopedPtr {
public:
    using pointer = T*;
    using element_type = T;
    
    explicit ArenaScopedPtr(T* ptr = nullptr) noexcept : ptr_(ptr) {}
    
    ~ArenaScopedPtr() {
        if (ptr_) {
            ptr_->~T();
            // 注意：arena分配的内存不能单独释放
            // ArenaScopedPtr主要用于确保对象正确析构
            ptr_ = nullptr;
        }
    }
    
    // 禁止拷贝，允许移动
    ArenaScopedPtr(const ArenaScopedPtr&) = delete;
    ArenaScopedPtr& operator=(const ArenaScopedPtr&) = delete;
    
    ArenaScopedPtr(ArenaScopedPtr&& other) noexcept : ptr_(other.ptr_) {
        other.ptr_ = nullptr;
    }
    
    ArenaScopedPtr& operator=(ArenaScopedPtr&& other) noexcept {
        if (this != &other) {
            if (ptr_) {
                ptr_->~T();
            }
            ptr_ = other.ptr_;
            other.ptr_ = nullptr;
        }
        return *this;
    }
    
    // 访问操作符
    T& operator*() const noexcept { return *ptr_; }
    T* operator->() const noexcept { return ptr_; }
    
    // 智能指针语义
    explicit operator bool() const noexcept { return ptr_ != nullptr; }
    bool operator!() const noexcept { return ptr_ == nullptr; }
    
    // 释放所有权（返回原始指针）
    T* release() noexcept {
        T* result = ptr_;
        ptr_ = nullptr;
        return result;
    }
    
    // 重置指针
    void reset(T* ptr = nullptr) noexcept {
        if (ptr_) {
            ptr_->~T();
        }
        ptr_ = ptr;
    }
    
    T* get() const noexcept { return ptr_; }
    
private:
    T* ptr_;
};

} // namespace net
} // namespace lattice