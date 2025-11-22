#pragma once

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <chrono>
#include <atomic>
#include <cstdint>
#include <optional>
#include <mutex>
#include <condition_variable>
#include <queue>
#include "../net/memory_arena.hpp"
#include "../net/native_compressor.hpp"
#include "../optimization/simd_optimization.hpp"

namespace lattice {
namespace world {

// ===========================================
// 1. 区块数据结构
// ===========================================
struct ChunkData {
    int x, z;
    int world_id;
    uint64_t last_modified;
    std::vector<uint8_t> data; // 原始NBT数据
    size_t data_size() const { return data.size(); }
    
    // 性能统计
    struct PerformanceMetrics {
        std::chrono::microseconds load_time{0};
        std::chrono::microseconds save_time{0};
        std::chrono::microseconds compression_time{0};
        size_t compressed_size{0};
        double compression_ratio{0.0};
        bool is_compressed{false};
    } metrics;
};

// ===========================================
// 2. I/O操作类型
// ===========================================
enum class IOOperation : uint8_t {
    LOAD,
    SAVE,
    DELETE,
    REGION_SCAN,
    BATCH_SAVE
};

// ===========================================
// 3. 异步操作结果
// ===========================================
struct AsyncIOResult {
    bool success{false};
    std::string error_message;
    ChunkData chunk;
    std::chrono::steady_clock::time_point completion_time;
    
    // 计算操作耗时
    std::chrono::microseconds elapsed_time() const {
        return std::chrono::duration_cast<std::chrono::microseconds>(
            completion_time.time_since_epoch());
    }
};

// ===========================================
// 4. 平台特性检测
// ===========================================
struct PlatformFeatures {
    bool io_uring{false};          // Linux io_uring
    bool iocp{false};              // Windows IOCP
    bool gcd{false};               // macOS Grand Central Dispatch
    bool direct_io{false};         // Direct I/O支持
    bool memory_mapping{false};    // 内存映射支持
    bool apfs{false};              // macOS APFS
    bool simd_available{false};    // SIMD指令集可用
    int cpu_cores{1};              // CPU核心数
    
    // 自动检测平台特性
    static PlatformFeatures detect();
};

// ===========================================
// 5. 批处理优化配置
// ===========================================
struct BatchConfig {
    size_t max_batch_size{64};
    size_t min_batch_size{4};
    std::chrono::milliseconds batch_timeout{100};
    bool enable_parallel_compression{true};
    bool enable_zero_copy{true};
    int compression_level{6};
    size_t io_queue_depth{256};
    size_t thread_pool_size{0}; // 0表示自动检测
};

// ===========================================
// 6. SIMD优化的区块处理
// ===========================================
class SIMDChunkProcessor {
public:
    explicit SIMDChunkProcessor(const BatchConfig& config);
    ~SIMDChunkProcessor();
    
    // SIMD优化的批量压缩
    void compress_batch_simd(std::vector<std::shared_ptr<ChunkData>>& chunks);
    
    // SIMD优化的批量解压
    void decompress_batch_simd(std::vector<std::shared_ptr<ChunkData>>& chunks);
    
    // 批量数据验证
    bool validate_batch_simd(const std::vector<std::shared_ptr<ChunkData>>& chunks);
    
    // 智能压缩级别选择
    int select_compression_level(const ChunkData& chunk);
    
private:
    BatchConfig config_;
    
#if defined(SIMD_LEVEL_AVX512)
    void process_batch_avx512(std::vector<std::shared_ptr<ChunkData>>& chunks);
#elif defined(SIMD_LEVEL_AVX2)
    void process_batch_avx2(std::vector<std::shared_ptr<ChunkData>>& chunks);
#elif defined(SIMD_LEVEL_SSE4)
    void process_batch_sse4(std::vector<std::shared_ptr<ChunkData>>& chunks);
#elif defined(SIMD_LEVEL_NEON)
    void process_batch_neon(std::vector<std::shared_ptr<ChunkData>>& chunks);
#else
    void process_batch_fallback(std::vector<std::shared_ptr<ChunkData>>& chunks);
#endif
};

// ===========================================
// 7. 平台特定后端基类
// ===========================================
class AsyncChunkIO::PlatformBackend {
public:
    virtual ~PlatformBackend() = default;
    
    // 异步读取
    virtual void load_chunk_async(int fd, off_t offset, size_t size,
                                 std::function<void(std::shared_ptr<uint8_t>, size_t)> callback) = 0;
    
    // 异步写入
    virtual void save_chunk_async(int fd, off_t offset, const std::shared_ptr<uint8_t>& data, size_t size,
                                 std::function<void(bool, std::string)> callback) = 0;
    
    // 批量保存
    virtual void save_chunks_batch(const std::vector<std::shared_ptr<ChunkData>>& chunks,
                                  std::function<void(std::vector<AsyncIOResult>)> callback) = 0;
    
    // 内存映射
    virtual std::shared_ptr<void> memory_map_file(const std::string& filepath, size_t size, bool read_only) = 0;
    
    // 获取平台特性
    virtual PlatformFeatures get_platform_features() const = 0;
    
    // 关闭文件描述符
    virtual void close_file_descriptor(int fd) = 0;
};

// ===========================================
// 8. Linux io_uring后端
// ===========================================
#if defined(PLATFORM_LINUX) && !defined(NO_IO_URING)
#include <liburing.h>

class LinuxIOUringBackend : public AsyncChunkIO::PlatformBackend {
public:
    explicit LinuxIOUringBackend(lattice::net::MemoryPool<64 * 1024>& memory_pool);
    ~LinuxIOUringBackend();
    
    void load_chunk_async(int fd, off_t offset, size_t size,
                         std::function<void(std::shared_ptr<uint8_t>, size_t)> callback) override;
    
    void save_chunk_async(int fd, off_t offset, const std::shared_ptr<uint8_t>& data, size_t size,
                         std::function<void(bool, std::string)> callback) override;
    
    void save_chunks_batch(const std::vector<std::shared_ptr<ChunkData>>& chunks,
                          std::function<void(std::vector<AsyncIOResult>)> callback) override;
    
    std::shared_ptr<void> memory_map_file(const std::string& filepath, size_t size, bool read_only) override;
    
    PlatformFeatures get_platform_features() const override;
    
    void close_file_descriptor(int fd) override;
    
private:
    lattice::net::MemoryPool<64 * 1024>& memory_pool_;
    io_uring ring_;
    std::atomic<bool> initialized_{false};
    
    // 操作上下文
    struct IOContext {
        int fd;
        off_t offset;
        size_t size;
        std::shared_ptr<uint8_t> buffer;
        std::function<void(std::shared_ptr<uint8_t>, size_t)> callback;
    };
    
    // 完成队列处理
    void process_completions();
    void handle_io_error(IOContext* ctx, int error);
    void handle_io_complete(IOContext* ctx, ssize_t result);
    
    // 批处理优化
    void optimize_batch_io(std::vector<std::shared_ptr<ChunkData>>& chunks);
};
#endif

// ===========================================
// 9. Windows IOCP后端
// ===========================================
#if defined(PLATFORM_WINDOWS)
#include <windows.h>
#include <io.h>

class WindowsIOCPBackend : public AsyncChunkIO::PlatformBackend {
public:
    explicit WindowsIOCPBackend(lattice::net::MemoryPool<64 * 1024>& memory_pool);
    ~WindowsIOCPBackend();
    
    void load_chunk_async(int fd, off_t offset, size_t size,
                         std::function<void(std::shared_ptr<uint8_t>, size_t)> callback) override;
    
    void save_chunk_async(int fd, off_t offset, const std::shared_ptr<uint8_t>& data, size_t size,
                         std::function<void(bool, std::string)> callback) override;
    
    void save_chunks_batch(const std::vector<std::shared_ptr<ChunkData>>& chunks,
                          std::function<void(std::vector<AsyncIOResult>)> callback) override;
    
    std::shared_ptr<void> memory_map_file(const std::string& filepath, size_t size, bool read_only) override;
    
    PlatformFeatures get_platform_features() const override;
    
    void close_file_descriptor(int fd) override;
    
private:
    lattice::net::MemoryPool<64 * 1024>& memory_pool_;
    HANDLE iocp_handle_;
    std::atomic<bool> initialized_{false};
    
    // OVERLAPPED结构扩展
    struct OVERLAPPED_EX : OVERLAPPED {
        std::function<void(DWORD, DWORD)> callback;
        std::shared_ptr<uint8_t> buffer;
        size_t buffer_size;
    };
    
    // 完成端口工作线程
    std::vector<std::thread> worker_threads_;
    std::atomic<bool> shutdown_{false};
    
    void worker_thread();
    void process_completion(DWORD bytes_transferred, OVERLAPPED_EX* overlapped);
};
#endif

// ===========================================
// 10. macOS GCD后端
// ===========================================
#if defined(PLATFORM_MACOS)
#include <dispatch/dispatch.h>
#include <sys/clonefile.h>
#include <sys/mman.h>

class MacOSGCDBackend : public AsyncChunkIO::PlatformBackend {
public:
    explicit MacOSGCDBackend(lattice::net::MemoryPool<64 * 1024>& memory_pool);
    ~MacOSGCDBackend();
    
    void load_chunk_async(int fd, off_t offset, size_t size,
                         std::function<void(std::shared_ptr<uint8_t>, size_t)> callback) override;
    
    void save_chunk_async(int fd, off_t offset, const std::shared_ptr<uint8_t>& data, size_t size,
                         std::function<void(bool, std::string)> callback) override;
    
    void save_chunks_batch(const std::vector<std::shared_ptr<ChunkData>>& chunks,
                          std::function<void(std::vector<AsyncIOResult>)> callback) override;
    
    std::shared_ptr<void> memory_map_file(const std::string& filepath, size_t size, bool read_only) override;
    
    PlatformFeatures get_platform_features() const override;
    
    void close_file_descriptor(int fd) override;
    
private:
    lattice::net::MemoryPool<64 * 1024>& memory_pool_;
    dispatch_queue_t io_queue_;
    dispatch_queue_t completion_queue_;
    std::atomic<bool> initialized_{false};
    
    // GCD I/O通道
    std::unordered_map<int, dispatch_io_t> io_channels_;
    mutable std::mutex channels_mutex_;
    
    // APFS优化
    bool is_apfs_filesystem(const std::string& filepath);
    bool clone_file_optimized(const std::string& src, const std::string& dst);
    
    // 统一内存架构优化
    void optimize_for_apple_silicon(size_t buffer_size);
    void set_io_quality_of_service();
};
#endif

// ===========================================
// 11. 回退后端（通用实现）
// ===========================================
class FallbackBackend : public AsyncChunkIO::PlatformBackend {
public:
    explicit FallbackBackend(lattice::net::MemoryPool<64 * 1024>& memory_pool);
    ~FallbackBackend();
    
    void load_chunk_async(int fd, off_t offset, size_t size,
                         std::function<void(std::shared_ptr<uint8_t>, size_t)> callback) override;
    
    void save_chunk_async(int fd, off_t offset, const std::shared_ptr<uint8_t>& data, size_t size,
                         std::function<void(bool, std::string)> callback) override;
    
    void save_chunks_batch(const std::vector<std::shared_ptr<ChunkData>>& chunks,
                          std::function<void(std::vector<AsyncIOResult>)> callback) override;
    
    std::shared_ptr<void> memory_map_file(const std::string& filepath, size_t size, bool read_only) override;
    
    PlatformFeatures get_platform_features() const override;
    
    void close_file_descriptor(int fd) override;
    
private:
    lattice::net::MemoryPool<64 * 1024>& memory_pool_;
    std::thread_pool thread_pool_;
    
    // 异步文件操作
    void async_read_file(int fd, off_t offset, size_t size, std::function<void(std::shared_ptr<uint8_t>, size_t)> callback);
    void async_write_file(int fd, off_t offset, const std::shared_ptr<uint8_t>& data, size_t size, 
                         std::function<void(bool, std::string)> callback);
};

// ===========================================
// 12. 内存映射区域
// ===========================================
class MemoryMappedRegion {
public:
    explicit MemoryMappedRegion(const std::string& filepath, size_t size, bool read_only);
    ~MemoryMappedRegion();
    
    void* data() const { return mapped_address_; }
    size_t size() const { return mapped_size_; }
    bool is_valid() const { return mapped_address_ != nullptr && mapped_address_ != MAP_FAILED; }
    
    // 预取页面到内存
    void prefetch_range(size_t offset, size_t length);
    
    // 同步映射区域
    void sync_range(size_t offset, size_t length);
    
    // 获取平台特定信息
    struct PlatformInfo {
        bool supports_huge_pages{false};
        bool supports_write_combining{false};
        size_t page_size{4096};
        std::string filesystem_type;
    };
    
    PlatformInfo get_platform_info() const;
    
private:
    void* mapped_address_{nullptr};
    size_t mapped_size_{0};
    int file_descriptor_{-1};
    bool read_only_{false};
    
    // 平台特定实现
#if defined(PLATFORM_WINDOWS)
    HANDLE file_mapping_{nullptr};
#elif defined(PLATFORM_MACOS)
    bool is_apfs_{false};
    std::string compression_type_;
#endif
};

// ===========================================
// 13. 主异步区块I/O类
// ===========================================
class AsyncChunkIO {
public:
    explicit AsyncChunkIO(const BatchConfig& config = BatchConfig{});
    ~AsyncChunkIO();
    
    // 异步加载区块
    void load_chunk_async(int world_id, int chunk_x, int chunk_z,
                         std::function<void(AsyncIOResult)> callback);
    
    // 异步保存区块
    void save_chunk_async(const ChunkData& chunk,
                         std::function<void(AsyncIOResult)> callback);
    
    // 批量保存区块（关键优化）
    void save_chunks_batch(const std::vector<std::shared_ptr<ChunkData>>& chunks,
                          std::function<void(std::vector<AsyncIOResult>)> callback);
    
    // 内存映射加载
    std::shared_ptr<MemoryMappedRegion> load_chunk_mapped(const std::string& filepath, bool read_only = true);
    
    // 每线程实例（与NativeCompressor一致的设计模式）
    static AsyncChunkIO* for_thread();
    
    // 运行时配置
    static void set_max_concurrent_io(int max_ops);
    static void enable_direct_io(bool enable);
    static void set_batch_config(const BatchConfig& config);
    
    // 性能监控
    struct PerformanceStats {
        size_t total_loads{0};
        size_t total_saves{0};
        size_t total_batch_operations{0};
        std::chrono::milliseconds avg_load_time{0};
        std::chrono::milliseconds avg_save_time{0};
        std::chrono::milliseconds avg_batch_time{0};
        double throughput_mb_per_sec{0.0};
        double hit_ratio{0.0}; // 缓存命中率
    };
    
    PerformanceStats get_performance_stats() const;
    void reset_performance_stats();
    
    // 平台检测和自动选择
    static void detect_and_select_backend();
    
    // 非拷贝/移动
    AsyncChunkIO(const AsyncChunkIO&) = delete;
    AsyncChunkIO& operator=(const AsyncChunkIO&) = delete;
    AsyncChunkIO(AsyncChunkIO&&) = delete;
    AsyncChunkIO& operator=(AsyncChunkIO&&) = delete;
    
private:
    BatchConfig config_;
    std::unique_ptr<PlatformBackend> backend_;
    std::unique_ptr<SIMDChunkProcessor> simd_processor_;
    lattice::net::MemoryPool<64 * 1024>& memory_pool_;
    
    // 性能统计
    mutable std::mutex stats_mutex_;
    PerformanceStats stats_;
    
    // 每线程实例管理
    static thread_local std::unique_ptr<AsyncChunkIO> thread_instance_;
    static std::mutex instances_mutex_;
    static std::unordered_map<std::thread::id, std::weak_ptr<AsyncChunkIO>> thread_instances_;
    
    // 缓存管理
    std::unordered_map<std::string, std::weak_ptr<MemoryMappedRegion>> mapped_regions_;
    mutable std::mutex regions_mutex_;
    
    // 批量操作优化
    void optimize_batch_operation(std::vector<std::shared_ptr<ChunkData>>& chunks);
    void schedule_batch_processing(std::vector<std::shared_ptr<ChunkData>>& chunks,
                                  std::function<void(std::vector<AsyncIOResult>)> callback);
    
    // 平台特定初始化（私有方法）
#if defined(PLATFORM_LINUX)
    void initializeLinuxSpecific();
    void performLinuxAsyncOperation(int chunkX, int chunkZ);
#elif defined(PLATFORM_WINDOWS)
    void initializeWindowsSpecific();
    void performWindowsAsyncOperation(int chunkX, int chunkZ);
#elif defined(PLATFORM_MACOS)
    void initializeMacOSSpecific();
    void performMacOSSpecificAsyncOperation(int chunkX, int chunkZ);
#endif
    
    // 内部工具方法
    std::string build_chunk_path(int world_id, int chunk_x, int chunk_z) const;
    int open_chunk_file(const std::string& path, bool read_only) const;
    void update_performance_stats(const std::chrono::steady_clock::time_point& start,
                                 bool is_batch = false, size_t data_size = 0);
};

// ===========================================
// 14. JNI桥接优化
// ===========================================
namespace jni {

// DirectByteBuffer优化的缓存管理器
class JNICacheManager {
public:
    static JNICacheManager& get_instance();
    
    // 零拷贝Java回调
    void register_chunk_load_callback(void* env, void* callback_object,
                                     std::function<void(jboolean, jstring, jobject)> callback);
    
    // 批量处理Java对象
    std::vector<std::shared_ptr<ChunkData>> extract_chunk_data_from_java(void* env, jobjectArray chunks_array);
    
    // 将结果转换回Java
    void convert_results_to_java(void* env, const std::vector<AsyncIOResult>& results, 
                                jobjectArray success_array, jobjectArray error_array);
    
    // 内存池管理
    std::shared_ptr<uint8_t> allocate_direct_buffer(size_t size);
    void release_direct_buffer(std::shared_ptr<uint8_t> buffer);
    
private:
    JNICacheManager() = default;
    
    std::unordered_map<void*, std::function<void(jboolean, jstring, jobject)>> callbacks_;
    mutable std::mutex callbacks_mutex_;
    
    lattice::net::MemoryPool<64 * 1024> buffer_pool_;
};

} // namespace jni

// ===========================================
// 15. 批处理优化器
// ===========================================
class BatchOptimizer {
public:
    explicit BatchOptimizer(const BatchConfig& config);
    ~BatchOptimizer() = default;
    
    // 按空间局部性排序区块
    void optimize_spatial_locality(std::vector<std::shared_ptr<ChunkData>>& chunks);
    
    // 按世界分组
    void group_by_world(std::vector<std::shared_ptr<ChunkData>>& chunks);
    
    // 智能批处理大小
    size_t calculate_optimal_batch_size(const std::vector<std::shared_ptr<ChunkData>>& chunks) const;
    
    // 预测I/O模式
    struct IOPattern {
        bool sequential_access{false};
        bool random_access{false};
        bool hotspot_regions{false};
        double locality_score{0.0};
    };
    
    IOPattern analyze_io_pattern(const std::vector<std::shared_ptr<ChunkData>>& chunks) const;
    
private:
    BatchConfig config_;
    
    // 空间距离计算
    double calculate_spatial_distance(int x1, int z1, int x2, int z2) const;
    
    // 热点检测
    std::vector<std::shared_ptr<ChunkData>> detect_hotspot_chunks(
        const std::vector<std::shared_ptr<ChunkData>>& chunks) const;
};

// ===========================================
// 16. 平台检测工具
// ===========================================
namespace platform_detection {

// 检查CPU特性
struct CPUFeatures {
    bool avx512{false};
    bool avx2{false};
    bool sse4{false};
    bool neon{false};
    int core_count{1};
    size_t cache_line_size{64};
    bool supports_huge_pages{false};
};

CPUFeatures detect_cpu_features();

// 检查I/O特性
struct IOFeatures {
    bool io_uring_supported{false};
    bool iocp_supported{false};
    bool gcd_supported{false};
    bool direct_io_supported{false};
    bool memory_mapping_supported{false};
    bool apfs_filesystem{false};
};

IOFeatures detect_io_features();

// 检查内存特性
struct MemoryFeatures {
    bool huge_pages_supported{false};
    bool write_combining_supported{false};
    size_t page_size{4096};
    size_t huge_page_size{2097152};
    bool unified_memory_architecture{false};
};

MemoryFeatures detect_memory_features();

// 统一的平台信息
struct PlatformInfo {
    CPUFeatures cpu;
    IOFeatures io;
    MemoryFeatures memory;
    std::string os_name;
    std::string architecture;
    int version_major;
    int version_minor;
};

PlatformInfo detect_platform_info();

// 自动选择最优后端
std::unique_ptr<AsyncChunkIO::PlatformBackend> select_optimal_backend(
    lattice::net::MemoryPool<64 * 1024>& memory_pool);

} // namespace platform_detection

} // namespace world
} // namespace lattice