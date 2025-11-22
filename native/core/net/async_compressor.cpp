#include "async_compressor.hpp"
#include <iostream>
#include <algorithm>
#include <thread>

namespace lattice {
namespace net {

AsyncCompressor::AsyncCompressor(int workerCount) : stop_(false) {
    if (workerCount == -1) {
        // 自动检测硬件并发数
        unsigned int detected = std::thread::hardware_concurrency();
        // 默认策略：使用 (N-1) 个核心，最多 8 个，最少 1 个
        int defaultWorkers = std::max(1, std::min(8, (int)detected - 1));
        workerCount_ = (detected > 0) ? defaultWorkers : 4;
    } else {
        workerCount_ = std::max(1, workerCount);
    }
    
    ensureWorkers();
}

AsyncCompressor::~AsyncCompressor() {
    stop_ = true;
    cv_.notify_all();
    for (auto& w : workers_) {
        if (w.joinable()) w.join();
    }
}

AsyncCompressor& AsyncCompressor::getInstance() {
    static AsyncCompressor instance;
    return instance;
}

void AsyncCompressor::compressAsync(
    std::shared_ptr<std::vector<char>> inputData,
    std::shared_ptr<std::vector<char>> outputBuffer,
    int level,
    std::function<void(bool, size_t)> callback) {

    auto task = taskPool_.acquire();
    task->input = inputData;
    task->output = outputBuffer;
    task->compressionLevel = level;
    task->callback = callback;

    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        taskQueue_.push(std::move(*task));
    }
    cv_.notify_one();
}

void AsyncCompressor::workerLoop() {
    while (!stop_) {
        std::unique_lock<std::mutex> lock(queueMutex_);
        cv_.wait(lock, [this] { return stop_ || !taskQueue_.empty(); });

        if (stop_ && taskQueue_.empty()) break;

        auto task = std::move(taskQueue_.front());
        taskQueue_.pop();
        lock.unlock();

        // 执行压缩
        auto compressor = NativeCompressor::forThread(task.compressionLevel);
        size_t result = compressor->compressZlib(
            task.input->data(), task.input->size(),
            task.output->data(), task.output->capacity()
        );

        bool success = (result > 0);
        size_t outputSize = success ? result : 0;
        
        if (success) {
            task.output->resize(result); // 设置实际大小
        }

        // 回调（可能在业务线程处理）
        if (task.callback) {
            task.callback(success, outputSize);
        }
    }
}

void AsyncCompressor::ensureWorkers() {
    int target = workerCount_.load();
    
    // 如果当前线程数少于目标数，增加线程
    while (static_cast<int>(workers_.size()) < target) {
        workers_.emplace_back(&AsyncCompressor::workerLoop, this);
    }
    
    // 如果当前线程数多于目标数，减少线程（需要重启整个池）
    if (static_cast<int>(workers_.size()) > target) {
        stop_ = true;
        cv_.notify_all();
        for (auto& w : workers_) {
            if (w.joinable()) w.join();
        }
        workers_.clear();
        stop_ = false;
        
        // 重新创建指定数量的工作线程
        for (int i = 0; i < target; ++i) {
            workers_.emplace_back(&AsyncCompressor::workerLoop, this);
        }
    }
}

void AsyncCompressor::setWorkerCount(int count) {
    if (count < 1) count = 1;
    workerCount_.store(count);
    ensureWorkers();
}

int DynamicCompression::suggestLevel(const Stats& stats, int baseLevel, int minLevel, int maxLevel) {
    // 基于配置的基准级别
    int level = baseLevel;
    
    // 高CPU使用率：降低压缩以减少CPU负载
    if (stats.cpuUsage > 80) {
        level = std::max(level - 2, 1);
    } else if (stats.cpuUsage < 30) {
        level = std::min(level + 1, 9);
    }
    
    // 高延迟：提高压缩（节省带宽）
    if (stats.rttMs > 200) {
        level = std::min(level + 2, 9);
    } else if (stats.rttMs < 50) {
        level = std::max(level - 1, 1);
    }
    
    // 玩家数多：降低压缩以减少CPU负载
    if (stats.playerCount > 150) {
        level = std::max(level - 2, 1);
    }
    
    // 带宽估算（简化）
    if (stats.bytesSent > 10 * 1024 * 1024) { // >10MB/s
        level = std::max(level - 1, 1); // 高带宽 → 低压缩
    }
    
    // 应用管理员设定的边界
    return std::clamp(level, minLevel, maxLevel);
}

int DynamicCompression::suggestWorkerCount() {
    unsigned int detected = std::thread::hardware_concurrency();
    // 默认策略：使用 (N-1) 个核心，最多 8 个，最少 1 个
    return (detected > 0) ? std::max(1, std::min(8, (int)detected - 1)) : 4;
}

} // namespace net
} // namespace lattice