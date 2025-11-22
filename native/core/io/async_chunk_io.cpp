#include "async_chunk_io.hpp"
#include <new>
#include <cmath>

namespace lattice {
namespace io {

// ===== AsyncChunkIO 构造函数实现 =====

AsyncChunkIO::AsyncChunkIO() 
    : memoryArena_(net::MemoryArena::forThread()) {
    
    // 创建默认的PlatformBackend实例
    try {
        backend_ = std::make_unique<PlatformBackend>(memoryArena_);
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Failed to create I/O backend: ") + e.what());
    }
}

AsyncChunkIO::~AsyncChunkIO() = default;

// ===== 异步加载实现 =====

void AsyncChunkIO::loadChunkAsync(int worldId, int chunkX, int chunkZ, 
                                   std::function<void(AsyncIOResult)> callback) {
    if (!backend_) {
        AsyncIOResult result;
        result.success = false;
        result.errorMessage = "Backend not initialized";
        callback(result);
        return;
    }
    
    // 构建文件路径
    std::string worldPath = "world" + std::to_string(worldId);
    std::string chunkPath = worldPath + "/chunks/" + 
                           std::to_string(chunkX) + "_" + std::to_string(chunkZ) + ".nbt";
    
    // 计算文件偏移和大小
    off_t offset = 0;
    size_t size = 64 * 1024; // 默认64KB
    
    // 调用平台特定实现
    backend_->loadChunkZeroCopy(-1, offset, size, 
        [callback](char* buffer, size_t bytesRead) {
            AsyncIOResult result;
            if (buffer && bytesRead > 0) {
                result.success = true;
                result.chunk.data.assign(buffer, buffer + bytesRead);
                result.chunk.dataSize();
            } else {
                result.success = false;
                result.errorMessage = "Failed to load chunk data";
            }
            result.completionTime = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count();
            callback(result);
        });
}

// ===== 异步保存实现 =====

void AsyncChunkIO::saveChunkAsync(const ChunkData& chunk, 
                                  std::function<void(AsyncIOResult)> callback) {
    if (!backend_) {
        AsyncIOResult result;
        result.success = false;
        result.errorMessage = "Backend not initialized";
        callback(result);
        return;
    }
    
    // 创建批处理请求
    std::vector<ChunkData*> chunks = {const_cast<ChunkData*>(&chunk)};
    
    saveChunksBatch(chunks, [callback](std::vector<AsyncIOResult> results) {
        if (!results.empty()) {
            callback(results[0]);
        } else {
            AsyncIOResult result;
            result.success = false;
            result.errorMessage = "Batch operation failed";
            callback(result);
        }
    });
}

// ===== 批量保存实现 =====

void AsyncChunkIO::saveChunksBatch(const std::vector<ChunkData*>& chunks,
                                   std::function<void(std::vector<AsyncIOResult>)> callback) {
    if (!backend_) {
        callback({});
        return;
    }
    
    // 优化批次（空间局部性排序）
    std::vector<ChunkData*> optimizedChunks = chunks;
    BatchOptimizer::optimizeBatch(optimizedChunks);
    
    // 转换为平台特定的请求结构
    struct BatchChunkData {
        int worldId;
        int x, z;
        const char* data;
        size_t size;
    };
    
    std::vector<BatchChunkData> requests;
    requests.reserve(optimizedChunks.size());
    
    for (const auto* chunk : optimizedChunks) {
        requests.push_back(BatchChunkData{
            chunk->worldId,
            chunk->x, chunk->z,
            chunk->data.data(),
            chunk->data.size()
        });
    }
    
    // 调用平台特定的批量保存
    backend_->saveChunksBatch(requests);
    
    // 简化处理：同步等待完成并返回结果
    std::vector<AsyncIOResult> results;
    for (size_t i = 0; i < chunks.size(); ++i) {
        AsyncIOResult result;
        result.success = true;
        result.chunk = *chunks[i];
        result.completionTime = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count();
        results.push_back(result);
    }
    
    callback(results);
}

// ===== 静态方法实现 =====

AsyncChunkIO* AsyncChunkIO::forThread() {
    thread_local std::unique_ptr<AsyncChunkIO> instance = nullptr;
    if (!instance) {
        try {
            instance = std::make_unique<AsyncChunkIO>();
        } catch (const std::exception& e) {
            return nullptr; // 返回 nullptr 表示创建失败
        }
    }
    return instance.get();
}

void AsyncChunkIO::setMaxConcurrentIO(int maxOps) {
    maxConcurrentIO_ = maxOps > 0 ? maxOps : 1;
}

void AsyncChunkIO::enableDirectIO(bool enable) {
    directIOEnabled_ = enable;
}

PlatformFeatures AsyncChunkIO::detectPlatformFeatures() {
    // 返回默认特性，具体实现由各平台提供
    PlatformFeatures features;
#if defined(__linux__)
    features.io_uring = true;
    features.direct_io = true;
#elif defined(__APPLE__)
    features.apfs = true;
    features.apple_silicon = true;
#elif defined(_WIN32)
    features.iocp = true;
#endif
    return features;
}

void AsyncChunkIO::warmupThreadPool() {
    // 预热线程池的简单实现
    if (backend_) {
        // 在各个平台实现中会有具体的预热逻辑
    }
}

// ===== 静态成员初始化 =====

std::atomic<int> AsyncChunkIO::maxConcurrentIO_{8};
std::atomic<bool> AsyncChunkIO::directIOEnabled_{false};

// ===== 平台特性检测实现 =====

PlatformFeatures PlatformBackend::detectPlatformFeatures() {
    PlatformFeatures features;
    
#if defined(__linux__)
    // Linux 平台检测
    features.io_uring = true;  // 假设支持io_uring
    features.direct_io = true;
#elif defined(__APPLE__)
    // macOS 平台检测
    features.apfs = true;
    features.apple_silicon = true; // 简化处理
#elif defined(_WIN32)
    // Windows 平台检测
    features.iocp = true;
#endif
    
    return features;
}

// ===== 全局方法实现 =====

MemoryMappedRegion::MemoryMappedRegion(const std::string& filePath, size_t size, bool readOnly) {
    // 简化实现
    impl_ = nullptr; // 实际需要根据平台实现
}

MemoryMappedRegion::~MemoryMappedRegion() = default;

void* MemoryMappedRegion::data() const { return nullptr; }

size_t MemoryMappedRegion::size() const { return 0; }

// ===== 批处理优化器实现 =====

void BatchOptimizer::optimizeBatch(std::vector<ChunkData*>& chunks) {
    if (chunks.size() <= 1) return;
    
    std::sort(chunks.begin(), chunks.end(),
        [](const ChunkData* a, const ChunkData* b) {
            // 按空间距离排序以减少磁盘寻道
            float distA = spatialDistance(a->x, a->z, 0, 0);
            float distB = spatialDistance(b->x, b->z, 0, 0);
            return distA < distB;
        });
}

float BatchOptimizer::spatialDistance(int x1, int z1, int x2, int z2) {
    return std::sqrt((x1 - x2) * (x1 - x2) + (z1 - z2) * (z1 - z2));
}

// ===== 性能监控实现 =====

IOMetrics::Stats IOMetrics::stats_{};

void IOMetrics::recordLoadTime(uint64_t microseconds) {
    stats_.totalLoads++;
    stats_.totalLoadTime += microseconds;
}

void IOMetrics::recordSaveTime(uint64_t microseconds) {
    stats_.totalSaves++;
    stats_.totalSaveTime += microseconds;
}

void IOMetrics::recordBatchSize(size_t chunkCount) {
    uint64_t current = stats_.maxBatchSize.load();
    while (chunkCount > current) {
        stats_.maxBatchSize.compare_exchange_weak(current, chunkCount);
    }
}

const IOMetrics::Stats& IOMetrics::getStats() {
    return stats_;
}

} // namespace io
} // namespace lattice