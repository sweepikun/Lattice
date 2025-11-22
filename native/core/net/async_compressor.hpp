#pragma once
#include <memory>
#include <functional>
#include <queue>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <vector>
#include <stack>
#include "native_compressor.hpp"

namespace lattice {
namespace net {

// 异步压缩任务
struct CompressTask {
    std::shared_ptr<std::vector<char>> input;
    std::shared_ptr<std::vector<char>> output;
    int compressionLevel;
    std::function<void(bool success, size_t outputSize)> callback;
};

// 对象池模板类
template<typename T>
class ObjectPool {
public:
    std::shared_ptr<T> acquire() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!pool_.empty()) {
            auto obj = std::move(pool_.top());
            pool_.pop();
            return obj;
        }
        return std::make_shared<T>();
    }

    void release(std::shared_ptr<T> obj) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (pool_.size() < max_pool_size_) {
            // 重置对象状态
            *obj = T{};
            pool_.push(std::move(obj));
        }
        // 如果池已满，对象将被自动销毁
    }

    void setMaxPoolSize(size_t size) {
        std::lock_guard<std::mutex> lock(mutex_);
        max_pool_size_ = size;
    }

private:
    std::stack<std::shared_ptr<T>> pool_;
    std::mutex mutex_;
    size_t max_pool_size_ = 50; // 默认最大池大小
};

class AsyncCompressor {
public:
    static AsyncCompressor& getInstance();
    
    // 提交异步压缩任务（Zlib 格式）
    void compressAsync(std::shared_ptr<std::vector<char>> inputData,
                       std::shared_ptr<std::vector<char>> outputBuffer,
                       int level,
                       std::function<void(bool, size_t)> callback);

    // 动态调整线程数
    void setWorkerCount(int count);
    ~AsyncCompressor();

    // 禁止拷贝
    AsyncCompressor(const AsyncCompressor&) = delete;
    AsyncCompressor& operator=(const AsyncCompressor&) = delete;

private:
    explicit AsyncCompressor(int workerCount = -1); // -1 表示自动检测

    void workerLoop();
    void ensureWorkers();

    std::queue<CompressTask> taskQueue_;
    std::mutex queueMutex_;
    std::condition_variable cv_;
    std::vector<std::thread> workers_;
    std::atomic<bool> stop_{false};
    std::atomic<int> workerCount_;
    
    // 对象池用于管理CompressTask对象
    ObjectPool<CompressTask> taskPool_;
};

// 动态压缩类
class DynamicCompression {
public:
    struct Stats {
        size_t bytesSent;
        double rttMs; // Round-trip time
        int playerCount;
        double cpuUsage; // CPU使用率 (0.0 - 100.0)
    };

    // 调整压缩级别的策略
    static int suggestLevel(const Stats& stats, int baseLevel, int minLevel, int maxLevel);
    
    // 根据CPU核心数建议工作线程数
    static int suggestWorkerCount();
};

} // namespace net
} // namespace lattice