#include "compress_buffer_cache.hpp"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <sstream>

namespace lattice {
namespace net {

// 静态成员初始化
std::atomic<size_t> CompressBufferCache::global_tick_counter_{0};

// 构造函数 - 支持默认配置
CompressBufferCache::CompressBufferCache(const Config& config) 
    : config_(config) {
    
    // 启动后台清理线程
    if (config_.enable_async_cleanup) {
        cleanup_running_ = true;
        cleanup_thread_ = std::thread(&CompressBufferCache::cleanupWorker, this);
        
        // 设置清理线程为后台线程
        std::thread cleanup_copy = std::move(cleanup_thread_);
        cleanup_thread_ = std::thread([this, cleanup_copy = std::move(cleanup_copy)]() mutable {
            // 分离线程，让它独立运行
            cleanup_copy.detach();
            this->cleanupWorker();
        });
    }
    
    std::cout << "[CompressBufferCache] Initialized with config: "
              << "default_size=" << config_.default_buffer_size 
              << ", max_size=" << config_.max_buffer_size
              << ", growth_factor=" << config_.growth_factor << std::endl;
}

// 析构函数
CompressBufferCache::~CompressBufferCache() {
    // 停止后台清理线程
    if (cleanup_running_) {
        cleanup_running_ = false;
        cleanup_condition_.notify_all();
        
        if (cleanup_thread_.joinable()) {
            cleanup_thread_.join();
        }
    }
    
    // 清理所有缓冲区
    clearAllBuffers();
    
    std::cout << "[CompressBufferCache] Destroyed. Final stats: "
              << "cache_hits=" << global_cache_hits_.load()
              << ", cache_misses=" << global_cache_misses_.load()
              << ", memory_reclaimed=" << global_memory_reclaimed_.load() << std::endl;
}

// 获取缓冲区
CompressBufferCache::Buffer* CompressBufferCache::getBuffer(size_t minSize) {
    // 边界检查
    if (minSize == 0) {
        return nullptr;
    }
    
    if (minSize > config_.max_buffer_size) {
        std::cerr << "[CompressBufferCache] Requested size " << minSize 
                  << " exceeds maximum " << config_.max_buffer_size << std::endl;
        return nullptr;
    }
    
    auto& buffers = getThreadBuffers();
    
    // 查找合适的缓冲区
    Buffer* suitableBuffer = findSuitableBuffer(buffers, minSize);
    
    if (suitableBuffer) {
        // 缓存命中
        suitableBuffer->touch();
        global_cache_hits_.fetch_add(1, std::memory_order_relaxed);
        return suitableBuffer;
    }
    
    // 缓存未命中，创建新缓冲区
    global_cache_misses_.fetch_add(1, std::memory_order_relaxed);
    
    Buffer* newBuffer = createNewBuffer(minSize);
    if (newBuffer) {
        buffers.push_back(std::unique_ptr<Buffer>(newBuffer));
        newBuffer->touch();
        return newBuffer;
    }
    
    return nullptr;
}

// 获取直接缓冲区（零拷贝）
void* CompressBufferCache::getDirectBuffer(size_t minSize) {
    Buffer* buffer = getBuffer(minSize);
    return buffer ? buffer->data : nullptr;
}

// 释放缓冲区
void CompressBufferCache::returnBuffer(Buffer* buffer) {
    if (!buffer) {
        return;
    }
    
    // 检查是否需要标记为脏（可以在这里实现更复杂的清理策略）
    buffer->is_dirty = false;
    buffer->touch();
}

// 清理线程缓冲区
void CompressBufferCache::cleanupThreadBuffers(std::thread::id threadId) {
    std::lock_guard<std::mutex> lock(thread_buffers_mutex_);
    
    auto it = thread_buffers_map_.find(threadId);
    if (it != thread_buffers_map_.end()) {
        cleanupThread(it->second);
    }
}

// 执行清理
void CompressBufferCache::performCleanup() {
    std::lock_guard<std::mutex> lock(thread_buffers_mutex_);
    
    size_t threadsCleaned = 0;
    size_t buffersCleaned = 0;
    size_t memoryReclaimed = 0;
    
    auto it = thread_buffers_map_.begin();
    while (it != thread_buffers_map_.end()) {
        size_t threadBuffersBefore = it->second.size();
        cleanupThread(it->second);
        size_t threadBuffersAfter = it->second.size();
        
        if (threadBuffersBefore != threadBuffersAfter) {
            threadsCleaned++;
            buffersCleaned += (threadBuffersBefore - threadBuffersAfter);
        }
        
        // 如果线程没有缓冲区了，移除该条目
        if (it->second.empty()) {
            it = thread_buffers_map_.erase(it);
        } else {
            ++it;
        }
    }
    
    global_memory_reclaimed_.fetch_add(memoryReclaimed, std::memory_order_relaxed);
    
    std::cout << "[CompressBufferCache] Cleanup completed: "
              << threadsCleaned << " threads, "
              << buffersCleaned << " buffers, "
              << memoryReclaimed << " bytes reclaimed" << std::endl;
}

// 获取统计信息
CompressBufferCache::CacheStats CompressBufferCache::getStats() const {
    CacheStats stats;
    
    // 收集当前线程缓冲区的统计
    std::lock_guard<std::mutex> lock(thread_buffers_mutex_);
    
    size_t totalBuffers = 0;
    size_t totalAllocated = 0;
    size_t totalUsed = 0;
    size_t activeBuffers = 0;
    float totalUtilization = 0.0f;
    
    for (const auto& [threadId, buffers] : thread_buffers_map_) {
        totalBuffers += buffers.size();
        
        for (const auto& buffer : buffers) {
            totalAllocated += buffer->capacity;
            totalUsed += buffer->current_size;
            activeBuffers++;
            totalUtilization += buffer->getUtilization();
        }
    }
    
    // 更新统计信息
    stats.total_buffers = totalBuffers;
    stats.active_buffers = activeBuffers;
    stats.total_memory_allocated = totalAllocated;
    stats.total_memory_used = totalUsed;
    stats.cache_hits = global_cache_hits_.load();
    stats.cache_misses = global_cache_misses_.load();
    stats.buffer_resizes = global_buffer_resizes_.load();
    stats.memory_reclaimed = global_memory_reclaimed_.load();
    stats.hit_rate = stats.getHitRate();
    stats.average_utilization = activeBuffers > 0 ? 
        totalUtilization / static_cast<float>(activeBuffers) : 0.0f;
    
    return stats;
}

// 获取线程缓冲区数量
size_t CompressBufferCache::getThreadBufferCount(std::thread::id threadId) const {
    std::lock_guard<std::mutex> lock(thread_buffers_mutex_);
    
    auto it = thread_buffers_map_.find(threadId);
    return it != thread_buffers_map_.end() ? it->second.size() : 0;
}

// 检查内存压力
bool CompressBufferCache::isUnderMemoryPressure() const {
    size_t totalAllocated = total_allocated_memory_.load(std::memory_order_relaxed);
    size_t totalUsed = total_used_memory_.load(std::memory_order_relaxed);
    
    float utilization = totalAllocated > 0 ? 
        static_cast<float>(totalUsed) / totalAllocated : 0.0f;
    
    return utilization > config_.memory_pressure_threshold;
}

// 清理所有缓冲区
void CompressBufferCache::clearAllBuffers() {
    std::lock_guard<std::mutex> lock(thread_buffers_mutex_);
    
    size_t totalMemory = 0;
    for (auto& [threadId, buffers] : thread_buffers_map_) {
        for (auto& buffer : buffers) {
            totalMemory += buffer->capacity;
        }
        buffers.clear();
    }
    thread_buffers_map_.clear();
    
    global_memory_reclaimed_.fetch_add(totalMemory, std::memory_order_relaxed);
    
    std::cout << "[CompressBufferCache] All buffers cleared: "
              << totalMemory << " bytes reclaimed" << std::endl;
}

// 更新配置
void CompressBufferCache::updateConfig(const Config& newConfig) {
    std::lock_guard<std::mutex> lock(thread_buffers_mutex_);
    
    config_ = newConfig;
    
    std::cout << "[CompressBufferCache] Configuration updated" << std::endl;
}

// 预热缓冲区
void CompressBufferCache::warmupBuffer(size_t size, size_t count) {
    if (size > config_.max_buffer_size || count == 0) {
        return;
    }
    
    auto& buffers = getThreadBuffers();
    
    // 创建指定数量的缓冲区
    for (size_t i = 0; i < count; ++i) {
        Buffer* buffer = createNewBuffer(size);
        if (buffer) {
            buffers.push_back(std::unique_ptr<Buffer>(buffer));
        }
    }
    
    std::cout << "[CompressBufferCache] Warmup completed: " 
              << count << " buffers of size " << size << std::endl;
}

// 估算最优大小
size_t CompressBufferCache::estimateOptimalSize(size_t inputSize) const {
    // 基于经验公式估算最优缓冲区大小
    // 压缩后通常比原数据小，所以预分配0.6-0.8倍的大小
    size_t estimatedSize = static_cast<size_t>(inputSize * 0.7f + 1024); // 加上1KB安全余量
    
    // 对齐到2的幂次，便于内存管理
    size_t optimalSize = 1;
    while (optimalSize < estimatedSize) {
        optimalSize <<= 1;
    }
    
    // 限制在配置范围内
    return std::clamp(optimalSize, 
                     config_.default_buffer_size, 
                     config_.max_buffer_size);
}

// ============== 私有方法实现 ==============

// 获取线程缓冲区
CompressBufferCache::ThreadBuffers& CompressBufferCache::getThreadBuffers() {
    std::thread::id threadId = std::this_thread::get_id();
    
    // 如果当前线程没有缓冲区，创建一个
    if (!current_thread_buffers_) {
        std::lock_guard<std::mutex> lock(thread_buffers_mutex_);
        
        auto& buffers = thread_buffers_map_[threadId];
        current_thread_buffers_ = &buffers;
        
        // 确保至少有默认数量的缓冲区
        if (buffers.size() < config_.min_free_buffers) {
            for (size_t i = buffers.size(); i < config_.min_free_buffers; ++i) {
                Buffer* buffer = createNewBuffer(config_.default_buffer_size);
                if (buffer) {
                    buffers.push_back(std::unique_ptr<Buffer>(buffer));
                }
            }
        }
    }
    
    return *current_thread_buffers_;
}

// 查找合适的缓冲区
CompressBufferCache::Buffer* CompressBufferCache::findSuitableBuffer(
    ThreadBuffers& buffers, size_t minSize) {
    
    Buffer* bestBuffer = nullptr;
    size_t bestUtilization = SIZE_MAX;
    
    // 查找可以容纳且利用率最低的缓冲区
    for (auto& buffer : buffers) {
        if (buffer->canFit(minSize)) {
            size_t utilization = buffer->getUtilization();
            if (utilization < bestUtilization) {
                bestUtilization = utilization;
                bestBuffer = buffer.get();
            }
        }
    }
    
    return bestBuffer;
}

// 创建新缓冲区
CompressBufferCache::Buffer* CompressBufferCache::createNewBuffer(size_t minSize) {
    size_t estimatedSize = estimateOptimalSize(minSize);
    size_t alignedSize = alignToSize(estimatedSize);
    
    void* data = std::malloc(alignedSize);
    if (!data) {
        return nullptr;
    }
    
    // 清零内存
    std::memset(data, 0, alignedSize);
    
    Buffer* buffer = new Buffer();
    buffer->data = data;
    buffer->capacity = alignedSize;
    buffer->current_size = 0;
    buffer->allocation_count = 1;
    
    // 更新全局统计
    total_allocated_memory_.fetch_add(alignedSize, std::memory_order_relaxed);
    
    return buffer;
}

// 调整缓冲区容量
void CompressBufferCache::adjustBufferCapacity(Buffer* buffer, size_t minSize) {
    if (!buffer || buffer->capacity >= minSize) {
        return;
    }
    
    size_t newCapacity = calculateNextSize(buffer->capacity, minSize, config_.growth_factor);
    newCapacity = std::min(newCapacity, config_.max_buffer_size);
    
    if (buffer->resize(newCapacity)) {
        global_buffer_resizes_.fetch_add(1, std::memory_order_relaxed);
    }
}

// 检查是否应该清理线程
bool CompressBufferCache::shouldCleanupThread(const ThreadBuffers& buffers) const {
    if (buffers.size() <= config_.min_free_buffers) {
        return false;
    }
    
    // 检查是否有长时间未使用的缓冲区
    auto now = std::chrono::steady_clock::now();
    constexpr auto TIMEOUT = std::chrono::minutes(5);
    
    for (const auto& buffer : buffers) {
        auto age = now - buffer->last_access;
        if (age > TIMEOUT && !buffer->is_dirty) {
            return true;
        }
    }
    
    return false;
}

// 清理线程
void CompressBufferCache::cleanupThread(ThreadBuffers& buffers) {
    if (buffers.size() <= config_.min_free_buffers) {
        return;
    }
    
    auto now = std::chrono::steady_clock::now();
    constexpr auto TIMEOUT = std::chrono::minutes(5);
    
    size_t memoryReclaimed = 0;
    
    // 使用迭代器安全删除
    for (auto it = buffers.begin(); it != buffers.end();) {
        const auto& buffer = *it;
        auto age = now - buffer->last_access;
        
        // 清理长时间未使用且非脏的缓冲区
        if (age > TIMEOUT && !buffer->is_dirty && 
            buffers.size() > config_.min_free_buffers) {
            memoryReclaimed += buffer->capacity;
            it = buffers.erase(it);
        } else {
            ++it;
        }
    }
    
    if (memoryReclaimed > 0) {
        global_memory_reclaimed_.fetch_add(memoryReclaimed, std::memory_order_relaxed);
    }
}

// 获取当前时间戳
size_t CompressBufferCache::getCurrentTick() const {
    return global_tick_counter_.fetch_add(1, std::memory_order_relaxed);
}

// 后台清理工作线程
void CompressBufferCache::cleanupWorker() {
    while (cleanup_running_) {
        try {
            // 等待清理信号或定时清理
            {
                std::unique_lock<std::mutex> lock(cleanup_mutex_);
                cleanup_condition_.wait_for(lock, 
                    std::chrono::seconds(config_.cleanup_interval_seconds));
            }
            
            if (!cleanup_running_) {
                break;
            }
            
            // 执行清理
            performCleanup();
            
        } catch (const std::exception& e) {
            std::cerr << "[CompressBufferCache] Cleanup worker error: " 
                      << e.what() << std::endl;
        }
    }
}

// 监控内存压力
void CompressBufferCache::monitorMemoryPressure() {
    // 这里可以实现更复杂的内存压力监控逻辑
    // 比如调整缓冲区大小、触发清理等
    
    if (isUnderMemoryPressure()) {
        std::cout << "[CompressBufferCache] Memory pressure detected, performing emergency cleanup" << std::endl;
        performCleanup();
    }
}

// ============== 静态工具方法 ==============

// 计算下一个大小
size_t CompressBufferCache::calculateNextSize(size_t currentSize, size_t minSize, size_t growthFactor) {
    if (minSize <= currentSize) {
        return currentSize;
    }
    
    size_t nextSize = currentSize;
    while (nextSize < minSize) {
        nextSize *= growthFactor;
    }
    
    return nextSize;
}

// 检查是否为2的幂次
bool CompressBufferCache::isPowerOfTwo(size_t size) {
    return size > 0 && (size & (size - 1)) == 0;
}

// 对齐到指定大小
size_t CompressBufferCache::alignToSize(size_t size, size_t alignment) {
    if (alignment == 0) {
        return size;
    }
    
    size_t mask = alignment - 1;
    return (size + mask) & ~mask;
}

} // namespace net
} // namespace lattice