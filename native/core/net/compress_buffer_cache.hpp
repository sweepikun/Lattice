#pragma once

#include <cstddef>
#include <memory>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include <unordered_map>
#include <condition_variable>
#include <functional>
#include <optional>
#include <string>

namespace lattice {
namespace net {

/**
 * CompressBufferCache - 智能缓冲区管理缓存
 * 
 * 核心特性：
 * 1. 线程局部缓冲区：每个线程独立的缓冲区池，避免竞争
 * 2. 智能容量增长：自动预估和调整缓冲区大小
 * 3. LRU老化策略：后台线程清理长时间未使用的缓冲区
 * 4. 内存监控：实时监控内存使用，自动优化
 * 5. 零拷贝支持：与Arena Allocator集成，最大化性能
 * 
 * 适用场景：
 * - 高频压缩/解压缩操作
 * - 内存敏感的服务端环境
 * - 需要避免GC压力的Java集成
 */
class CompressBufferCache {
public:
    // 配置参数
    struct Config {
        size_t default_buffer_size = 64 * 1024;        // 默认缓冲区大小 (64KB)
        size_t max_buffer_size = 64 * 1024 * 1024;     // 最大缓冲区大小 (64MB)
        size_t growth_factor = 2;                      // 增长因子 (2倍增长)
        size_t cleanup_interval_seconds = 300;         // 清理间隔 (5分钟)
        size_t max_buffers_per_thread = 8;             // 每线程最大缓冲区数
        size_t min_free_buffers = 2;                   // 最小保留缓冲区数
        float memory_pressure_threshold = 0.8f;        // 内存压力阈值 (80%)
        bool enable_async_cleanup = true;              // 启用异步清理
    };
    
    // 缓冲区项
    struct Buffer {
        void* data;                    // 缓冲区数据
        size_t capacity;               // 缓冲区容量
        size_t current_size;           // 当前使用大小
        size_t last_used_tick;         // 最后使用时间戳
        std::chrono::steady_clock::time_point last_access; // 最后访问时间
        size_t allocation_count;       // 分配次数统计
        bool is_dirty;                 // 是否需要清理
        
        Buffer() 
            : data(nullptr), capacity(0), current_size(0), 
              last_used_tick(0), allocation_count(0), is_dirty(true) {
            last_access = std::chrono::steady_clock::now();
        }
        
        ~Buffer() {
            if (data) {
                std::free(data);
                data = nullptr;
            }
        }
        
        // 禁止拷贝和移动
        Buffer(const Buffer&) = delete;
        Buffer& operator=(const Buffer&) = delete;
        Buffer(Buffer&&) = delete;
        Buffer& operator=(Buffer&&) = delete;
        
        // 检查缓冲区是否可以容纳指定大小
        bool canFit(size_t size) const {
            return size <= capacity;
        }
        
        // 重新调整缓冲区大小
        bool resize(size_t newCapacity) {
            if (newCapacity == capacity) {
                return true;
            }
            
            void* newData = std::realloc(data, newCapacity);
            if (!newData) {
                return false;
            }
            
            data = newData;
            capacity = newCapacity;
            is_dirty = true;
            return true;
        }
        
        // 更新访问时间
        void touch() {
            // 通过CompressBufferCache实例来获取tick
            // 这里需要传入实例或者使用其他方法
            last_used_tick = 0; // 临时修复
            last_access = std::chrono::steady_clock::now();
        }
        
        // 获取内存使用率
        float getUtilization() const {
            return capacity > 0 ? static_cast<float>(current_size) / capacity : 0.0f;
        }
    };
    
    // 线程局部缓冲区
    using ThreadBuffers = std::vector<std::unique_ptr<Buffer>>;
    
    // 缓存统计信息
    struct CacheStats {
        size_t total_buffers;                    // 总缓冲区数
        size_t active_buffers;                   // 活跃缓冲区数
        size_t total_memory_allocated;           // 分配的总内存
        size_t total_memory_used;                // 实际使用的内存
        size_t cache_hits;                       // 缓存命中次数
        size_t cache_misses;                     // 缓存未命中次数
        size_t buffer_resizes;                   // 缓冲区调整次数
        size_t memory_reclaimed;                 // 回收的内存量
        double hit_rate;                         // 命中率
        float average_utilization;               // 平均利用率
        
        CacheStats() 
            : total_buffers(0), active_buffers(0), total_memory_allocated(0),
              total_memory_used(0), cache_hits(0), cache_misses(0),
              buffer_resizes(0), memory_reclaimed(0), hit_rate(0.0), average_utilization(0.0f) {}
        
        // 计算命中率
        double getHitRate() const {
            size_t total_requests = cache_hits + cache_misses;
            return total_requests > 0 ? static_cast<double>(cache_hits) / total_requests : 0.0;
        }
        
        // 获取内存效率
        float getMemoryEfficiency() const {
            return total_memory_allocated > 0 ? 
                   static_cast<float>(total_memory_used) / total_memory_allocated : 0.0f;
        }
    };
    
    // 构造函数 - 移除默认参数
    explicit CompressBufferCache(const Config& config);
    ~CompressBufferCache();
    
    // 禁止拷贝和移动
    CompressBufferCache(const CompressBufferCache&) = delete;
    CompressBufferCache& operator=(const CompressBufferCache&) = delete;
    CompressBufferCache(CompressBufferCache&&) = delete;
    CompressBufferCache& operator=(CompressBufferCache&&) = delete;
    
    /**
     * 获取适合指定大小的缓冲区
     * 如果没有合适的缓冲区，会创建新的或调整现有缓冲区
     */
    Buffer* getBuffer(size_t minSize);
    
    /**
     * 获取零拷贝缓冲区（与Arena集成）
     */
    void* getDirectBuffer(size_t minSize);
    
    /**
     * 释放缓冲区到缓存（不实际释放内存）
     */
    void returnBuffer(Buffer* buffer);
    
    /**
     * 强制清理指定线程的缓冲区
     */
    void cleanupThreadBuffers(std::thread::id threadId = std::this_thread::get_id());
    
    /**
     * 执行全局清理，移除老化缓冲区
     */
    void performCleanup();
    
    /**
     * 获取缓存统计信息
     */
    CacheStats getStats() const;
    
    /**
     * 获取当前线程的缓冲区数量
     */
    size_t getThreadBufferCount(std::thread::id threadId = std::this_thread::get_id()) const;
    
    /**
     * 检查内存压力状态
     */
    bool isUnderMemoryPressure() const;
    
    /**
     * 强制清理所有缓冲区（谨慎使用）
     */
    void clearAllBuffers();
    
    /**
     * 动态调整配置参数
     */
    void updateConfig(const Config& newConfig);
    
    /**
     * 获取当前配置
     */
    const Config& getConfig() const { return config_; }
    
    /**
     * 预热缓冲区（为指定大小预分配缓冲区）
     */
    void warmupBuffer(size_t size, size_t count = 1);
    
    /**
     * 估算最优缓冲区大小
     */
    size_t estimateOptimalSize(size_t inputSize) const;
    
private:
    // 线程局部存储
    thread_local static inline ThreadBuffers* current_thread_buffers_ = nullptr;
    
    // 配置
    Config config_;
    
    // 全局统计
    mutable std::atomic<size_t> global_cache_hits_{0};
    mutable std::atomic<size_t> global_cache_misses_{0};
    mutable std::atomic<size_t> global_buffer_resizes_{0};
    mutable std::atomic<size_t> global_memory_reclaimed_{0};
    mutable std::atomic<size_t> total_allocated_memory_{0};
    mutable std::atomic<size_t> total_used_memory_{0};
    
    // 线程管理
    mutable std::mutex thread_buffers_mutex_;
    std::unordered_map<std::thread::id, ThreadBuffers> thread_buffers_map_;
    
    // 后台清理线程
    std::atomic<bool> cleanup_running_{false};
    std::thread cleanup_thread_;
    std::condition_variable cleanup_condition_;
    std::mutex cleanup_mutex_;
    
    // 时间戳管理
    static std::atomic<size_t> global_tick_counter_;
    
    // 内部方法
    ThreadBuffers& getThreadBuffers();
    Buffer* findSuitableBuffer(ThreadBuffers& buffers, size_t minSize);
    Buffer* createNewBuffer(size_t minSize);
    void adjustBufferCapacity(Buffer* buffer, size_t minSize);
    bool shouldCleanupThread(const ThreadBuffers& buffers) const;
    void cleanupThread(ThreadBuffers& buffers);
    size_t getCurrentTick() const;
    void cleanupWorker();
    void updateGlobalStats() const;
    void monitorMemoryPressure();
    
    // 静态工具方法
    static size_t calculateNextSize(size_t currentSize, size_t minSize, size_t growthFactor);
    static bool isPowerOfTwo(size_t size);
    static size_t alignToSize(size_t size, size_t alignment = 16);
};

} // namespace net
} // namespace lattice