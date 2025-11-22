#pragma once
#include <memory>
#include <functional>
#include <chrono>
#include <atomic>
#include <string>
#include <vector>
#include <map>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <future>
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <cstdlib>
#include <unordered_map>

namespace lattice {
namespace jni_framework {

// ========== 异常类 ==========
class JNIFrameworkException : public std::runtime_error {
public:
    enum class ErrorCode {
        MEMORY_ALLOCATION_FAILED,
        POINTER_INVALID,
        TIMEOUT,
        BATCH_TOO_LARGE
    };
    
    ErrorCode error_code;
    std::chrono::steady_clock::time_point timestamp;
    
    JNIFrameworkException(ErrorCode code, const std::string& message)
        : std::runtime_error(message), error_code(code), 
          timestamp(std::chrono::steady_clock::now()) {}
};

// ========== 配置类 ==========
class FrameworkConfig {
public:
    struct MemoryConfig {
        size_t max_pool_size = 64 * 1024 * 1024;    // 64MB池大小
        size_t block_alignment = 64;                // 64字节对齐
        bool enable_leak_detection = true;          // 启用泄漏检测
    } memory;
};

// ========== 内存管理器 ==========
class JNIMemoryManager {
private:
    struct Allocation {
        void* ptr;
        size_t size;
        std::string tag;
        std::chrono::steady_clock::time_point timestamp;
    };
    
    FrameworkConfig::MemoryConfig config_;
    std::vector<Allocation> allocations_;
    std::mutex mutex_;
    std::atomic<size_t> total_allocated_{0};
    std::atomic<size_t> peak_allocated_{0};
    
public:
    explicit JNIMemoryManager(const FrameworkConfig::MemoryConfig& config)
        : config_(config) {}
    
    void* allocate(size_t size, const std::string& tag = "") {
        std::unique_lock<std::mutex> lock(mutex_);
        
        if (total_allocated_.load() + size > config_.max_pool_size) {
            throw JNIFrameworkException(
                JNIFrameworkException::ErrorCode::MEMORY_ALLOCATION_FAILED,
                "Memory pool exhausted");
        }
        
        void* ptr = std::malloc(size);
        if (!ptr) {
            throw JNIFrameworkException(
                JNIFrameworkException::ErrorCode::MEMORY_ALLOCATION_FAILED,
                "malloc failed");
        }
        
        Allocation alloc{ptr, size, tag, std::chrono::steady_clock::now()};
        allocations_.push_back(alloc);
        
        size_t current_total = total_allocated_.fetch_add(size) + size;
        size_t peak = peak_allocated_.load();
        while (current_total > peak && 
               !peak_allocated_.compare_exchange_weak(peak, current_total)) {}
        
        return ptr;
    }
    
    void deallocate(void* ptr) {
        if (!ptr) return;
        
        std::unique_lock<std::mutex> lock(mutex_);
        
        auto it = std::find_if(allocations_.begin(), allocations_.end(),
            [ptr](const Allocation& alloc) { return alloc.ptr == ptr; });
        
        if (it != allocations_.end()) {
            size_t size = it->size;
            total_allocated_.fetch_sub(size);
            allocations_.erase(it);
            std::free(ptr);
        }
    }
    
    void detectLeaks() {
        if (!config_.enable_leak_detection) return;
        
        std::unique_lock<std::mutex> lock(mutex_);
        
        if (!allocations_.empty()) {
            std::cerr << "[JNIMemoryManager] Potential memory leaks detected:\n";
            for (const auto& alloc : allocations_) {
                auto age = std::chrono::steady_clock::now() - alloc.timestamp;
                auto age_ms = std::chrono::duration_cast<std::chrono::milliseconds>(age).count();
                std::cerr << "  - " << alloc.tag << " (" << alloc.size 
                         << " bytes, age: " << age_ms << "ms)\n";
            }
        }
    }
    
    size_t getCurrentUsage() const {
        return total_allocated_.load();
    }
    
    size_t getPeakUsage() const {
        return peak_allocated_.load();
    }
};

// ========== 简单的SafeBuffer ==========
class SafeBuffer {
private:
    void* data_;
    size_t size_;
    std::string tag_;
    JNIMemoryManager* manager_; // 添加管理器引用
    
public:
    SafeBuffer(void* data, size_t size, const std::string& tag = "", JNIMemoryManager* manager = nullptr)
        : data_(data), size_(size), tag_(tag), manager_(manager) {}
    
    SafeBuffer(const SafeBuffer&) = delete;
    SafeBuffer& operator=(const SafeBuffer&) = delete;
    
    SafeBuffer(SafeBuffer&& other) noexcept
        : data_(other.data_), size_(other.size_), tag_(std::move(other.tag_)),
          manager_(other.manager_) {
        other.data_ = nullptr;
        other.size_ = 0;
        other.manager_ = nullptr;
    }
    
    ~SafeBuffer() {
        // 通过管理器释放内存，避免双重释放
        if (data_ && manager_) {
            manager_->deallocate(data_);
        }
    }
    
    void* data() const { return data_; }
    size_t size() const { return size_; }
    const std::string& tag() const { return tag_; }
};

// ========== 简化的线程池 ==========
class JVMThreadPool {
private:
    std::queue<std::function<void()>> tasks_;
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::vector<std::thread> workers_;
    std::atomic<bool> shutdown_{false};
    
public:
    explicit JVMThreadPool(size_t num_threads) {
        for (size_t i = 0; i < num_threads; ++i) {
            workers_.emplace_back([this] { this->workerLoop(); });
        }
    }
    
    ~JVMThreadPool() {
        shutdown_.store(true);
        condition_.notify_all();
        
        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }
    
    void submit(std::function<void()> task) {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            tasks_.emplace(std::move(task));
        }
        condition_.notify_one();
    }
    
    size_t getThreadCount() const {
        return workers_.size();
    }
    
private:
    void workerLoop() {
        while (!shutdown_.load()) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                condition_.wait(lock, [this] { 
                    return !tasks_.empty() || shutdown_.load(); 
                });
                
                if (shutdown_.load() && tasks_.empty()) {
                    return;
                }
                
                if (!tasks_.empty()) {
                    task = std::move(tasks_.front());
                    tasks_.pop();
                }
            }
            
            if (task) {
                try {
                    task();
                } catch (const std::exception& e) {
                    std::cerr << "[JVMThreadPool] Task exception: " << e.what() << "\n";
                }
            }
        }
    }
};

// ========== 性能指标 ==========
struct PerformanceMetrics {
    std::atomic<uint64_t> total_operations{0};
    std::atomic<uint64_t> total_time_ns{0};
    std::atomic<uint64_t> min_time_ns{UINT64_MAX};
    std::atomic<uint64_t> max_time_ns{0};
    
    void recordLatency(const std::chrono::steady_clock::duration& duration) {
        uint64_t duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
        
        total_operations.fetch_add(1);
        total_time_ns.fetch_add(duration_ns);
        
        // 更新最小/最大时间
        uint64_t current_min = min_time_ns.load();
        while (duration_ns < current_min && 
               !min_time_ns.compare_exchange_weak(current_min, duration_ns)) {}
        
        uint64_t current_max = max_time_ns.load();
        while (duration_ns > current_max && 
               !max_time_ns.compare_exchange_weak(current_max, duration_ns)) {}
    }
    
    double getAverageLatencyUs() const {
        uint64_t ops = total_operations.load();
        uint64_t time = total_time_ns.load();
        return ops > 0 ? (static_cast<double>(time) / ops) / 1000.0 : 0.0;
    }
    
    bool isHealthy() const {
        return total_operations.load() > 0;
    }
};

} // namespace jni_framework
} // namespace lattice