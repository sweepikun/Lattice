#pragma once

#include <memory>
#include <string>
#include <vector>
#include <cstdint>
#include <functional>
#include <atomic>
#include "../net/native_compressor.hpp"
#include "../net/memory_arena.hpp"
#include "io_types.hpp"

namespace lattice {
namespace io {

// ===== 核心异步I/O类 =====

class AsyncChunkIO {
public:
    AsyncChunkIO();
    ~AsyncChunkIO();
    
    // 异步加载区块（零拷贝）
    void loadChunkAsync(int worldId, int chunkX, int chunkZ, 
                       std::function<void(AsyncIOResult)> callback);
    
    // 异步保存区块（批处理）
    void saveChunkAsync(const ChunkData& chunk, 
                       std::function<void(AsyncIOResult)> callback);
    
    // 批量保存（关键优化）
    void saveChunksBatch(const std::vector<ChunkData*>& chunks,
                        std::function<void(std::vector<AsyncIOResult>)> callback);
    
    // ===== Anvil格式支持 =====
    // 设置存储格式（默认传统格式）
    void setStorageFormatInstance(StorageFormat format) { storageFormat_ = format; }
    StorageFormat getStorageFormat() const { return storageFormat_; }
    
    // Anvil格式异步操作
    void loadChunkAnvilAsync(int worldId, int chunkX, int chunkZ,
                           std::function<void(AsyncIOResult)> callback);
    void saveChunkAnvilAsync(const ChunkData& chunk,
                           std::function<void(AsyncIOResult)> callback);
    void saveChunksAnvilBatch(const std::vector<ChunkData*>& chunks,
                            std::function<void(std::vector<AsyncIOResult>)> callback);
    
    // 检查文件格式兼容性
    bool isAnvilFormatAvailable(const std::string& worldPath) const;
    StorageFormat detectStorageFormat(const std::string& worldPath) const;
    
    // 每线程实例（与 NativeCompressor 风格一致）
    static AsyncChunkIO* forThread();
    
    // 运行时配置
    static void setMaxConcurrentIO(int maxOps);
    static void enableDirectIO(bool enable);
    static void setStorageFormat(StorageFormat format);
    static StorageFormat getDefaultStorageFormat();
    
    // Anvil格式压缩选项（仅支持ZLIB）
    enum class CompressionFormat : uint8_t {
        ZLIB_ONLY = 0  // 仅支持ZLIB格式（Minecraft原版兼容）
    };
    static void setCompressionFormat(CompressionFormat format);
    static CompressionFormat getCompressionFormat();
    
    // 平台特性检测
    static PlatformFeatures detectPlatformFeatures();
    
    // 预热线程池
    void warmupThreadPool();
    
    // 禁止拷贝/移动
    AsyncChunkIO(const AsyncChunkIO&) = delete;
    AsyncChunkIO& operator=(const AsyncChunkIO&) = delete;
    AsyncChunkIO(AsyncChunkIO&&) = delete;
    AsyncChunkIO& operator=(AsyncChunkIO&&) = delete;

    // 平台特定后端完整定义
    class PlatformBackend {
    public:
        PlatformBackend(lattice::net::MemoryArena& arena) : memoryArena_(arena) {}
        virtual ~PlatformBackend() = default;
        
        // 基本异步I/O操作
        virtual void loadChunkAsync(int fd, off_t offset, size_t size,
                                  std::function<void(std::shared_ptr<uint8_t>, size_t)> callback) {
            callback(nullptr, 0); // 默认空实现
        }
        
        virtual void saveChunkAsync(int fd, off_t offset, const std::shared_ptr<uint8_t>& data, size_t size,
                                  std::function<void(bool, std::string)> callback) {
            callback(true, ""); // 默认成功
        }
        
        virtual void saveChunksBatch(const std::vector<std::shared_ptr<ChunkData>>& chunks,
                                   std::function<void(std::vector<AsyncIOResult>)> callback) {
            std::vector<AsyncIOResult> results;
            callback(results);
        }
        
        virtual std::shared_ptr<void> memoryMapFile(const std::string& filepath, size_t size, bool read_only) {
            return nullptr;
        }
        
        virtual PlatformFeatures getPlatformFeatures() const {
            PlatformFeatures features;
            features.io_uring = false;
            features.direct_io = false;
            features.apfs = false;
            features.apple_silicon = false;
            features.iocp = false;
            return features;
        }
        
        virtual void closeFileDescriptor(int fd) {}
        
    private:
        lattice::net::MemoryArena& memoryArena_;
    };
    
    std::unique_ptr<PlatformBackend> backend_;
    
    // 内存池（使用 MemoryArena 实现）
    lattice::net::MemoryArena& memoryArena_;
    
    // 全局配置
    static std::atomic<int> maxConcurrentIO_;
    static std::atomic<bool> directIOEnabled_;
    static std::atomic<StorageFormat> defaultStorageFormat_;
    static std::atomic<CompressionFormat> compressionFormat_;
    
    // 实例级配置
    StorageFormat storageFormat_;
};

// ===== 内存映射区域 =====

class MemoryMappedRegion {
public:
    class PlatformImpl;
    
    MemoryMappedRegion(const std::string& filePath, size_t size, bool readOnly);
    ~MemoryMappedRegion();
    
    void* data() const;
    size_t size() const;
    bool isValid() const { return impl_ != nullptr; }
    
    // 禁止拷贝
    MemoryMappedRegion(const MemoryMappedRegion&) = delete;
    MemoryMappedRegion& operator=(const MemoryMappedRegion&) = delete;
    
private:
    std::unique_ptr<PlatformImpl> impl_;
};

// ===== 批处理优化器 =====

class BatchOptimizer {
public:
    static void optimizeBatch(std::vector<ChunkData*>& chunks);
    static float spatialDistance(int x1, int z1, int x2, int z2);
};

// ===== 性能监控 =====

class IOMetrics {
public:
    static void recordLoadTime(uint64_t microseconds);
    static void recordSaveTime(uint64_t microseconds);
    static void recordBatchSize(size_t chunkCount);
    
    struct Stats {
        std::atomic<uint64_t> totalLoads{0};
        std::atomic<uint64_t> totalSaves{0};
        std::atomic<uint64_t> totalLoadTime{0};
        std::atomic<uint64_t> totalSaveTime{0};
        std::atomic<uint64_t> maxBatchSize{0};
    };
    
    static const Stats& getStats();
    
private:
    static Stats stats_;
};

// ===== 平台特定实现 =====
// 平台特定实现在相应的 .cpp 文件中

} // namespace io
} // namespace lattice