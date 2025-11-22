#pragma once

#include <memory>
#include "libdeflate.h"
#include "memory_arena.hpp"
#include "compress_buffer_cache.hpp"

namespace lattice {
namespace net {

class NativeCompressor {
public:
    NativeCompressor(int compressionLevel = 6);
    ~NativeCompressor();

    // ====== 传统接口（保持向后兼容）======
    // libdeflate compression (zlib format)
    size_t compressZlib(const char* src, size_t srcLen, char* dst, size_t dstCapacity);
    size_t decompressZlib(const char* src, size_t srcLen, char* dst, size_t dstCapacity);

    // Runtime configuration
    void setCompressionLevel(int level);

    // Get per-thread instance (creates if needed). Use this to avoid sharing contexts across threads.
    static NativeCompressor* forThread(int compressionLevel);

    // ====== Arena优化接口（内存零拷贝）======
    
    /**
     * 使用Arena Allocator进行压缩，自动管理内存
     * 返回结果包含压缩数据和内存统计信息
     */
    struct ArenaCompressionResult {
        void* compressed_data;    // Arena分配的压缩数据
        size_t compressed_size;   // 压缩后的大小
        size_t memory_used;       // Arena中实际使用的内存
        MemoryArena::MemoryStats arena_stats;  // Arena内存统计
        
        // 获取压缩数据的C字符串指针
        const char* getData() const { return static_cast<const char*>(compressed_data); }
        
        // 检查压缩是否成功
        bool success() const { return compressed_data != nullptr && compressed_size > 0; }
    };
    
    /**
     * 使用Arena进行高效压缩（零拷贝内存分配）
     * @param src 源数据
     * @param srcLen 源数据长度
     * @param preferredCompressionLevel 压缩级别（可选）
     * @return Arena压缩结果
     */
    ArenaCompressionResult compressZlibArena(const void* src, size_t srcLen, 
                                           std::optional<int> preferredCompressionLevel = std::nullopt);
    
    /**
     * 使用Arena进行解压缩
     * @param compressedData 压缩数据
     * @param compressedSize 压缩数据大小
     * @param expectedOutputSize 期望的输出大小（可选）
     * @return 解压缩结果
     */
    struct ArenaDecompressionResult {
        void* decompressed_data;  // Arena分配的解压缩数据
        size_t decompressed_size; // 解压缩后的大小
        size_t memory_used;       // Arena中实际使用的内存
        MemoryArena::MemoryStats arena_stats;  // Arena内存统计
        
        // 获取解压缩数据的C字符串指针
        const char* getData() const { return static_cast<const char*>(decompressed_data); }
        
        // 检查解压缩是否成功
        bool success() const { return decompressed_data != nullptr && decompressed_size > 0; }
    };
    
    ArenaDecompressionResult decompressZlibArena(const void* compressedData, size_t compressedSize,
                                                std::optional<size_t> expectedOutputSize = std::nullopt);
    
    /**
     * 批量压缩接口，使用arena优化内存分配
     * 适合处理多个小数据包，减少内存分配开销
     */
    struct BatchCompressionResult {
        std::vector<ArenaCompressionResult> results;
        MemoryArena::MemoryStats total_stats;  // 总的Arena统计
        
        size_t getTotalInputSize() const {
            // This method should be called with the original input sizes
            // For now, return 0 as we don't track input sizes in the results
            (void)results; // Suppress unused parameter warning
            return 0;
        }
        
        size_t getTotalOutputSize() const {
            size_t total = 0;
            for (const auto& result : results) {
                total += result.compressed_size;
            }
            return total;
        }
    };
    
    /**
     * 批量压缩多个数据块
     * @param inputs 输入数据块数组
     * @param inputSizes 输入数据块大小数组
     * @param count 数据块数量
     * @return 批量压缩结果
     */
    BatchCompressionResult compressBatchArena(const void* const* inputs, const size_t* inputSizes, size_t count);
    
    /**
     * 批量解压缩
     * @param compressedInputs 压缩输入数组
     * @param compressedSizes 压缩数据大小数组
     * @param expectedOutputs 期望输出大小数组（可选）
     * @param count 数据块数量
     * @return 批量解压缩结果
     */
    struct BatchDecompressionResult {
        std::vector<ArenaDecompressionResult> results;
        MemoryArena::MemoryStats total_stats;  // 总的Arena统计
    };
    
    BatchDecompressionResult decompressBatchArena(const void* const* compressedInputs, 
                                                 const size_t* compressedSizes,
                                                 const std::optional<size_t>* expectedOutputSizes,
                                                 size_t count);
    
    /**
     * 获取内存使用统计
     */
    MemoryArena::MemoryStats getArenaStats() const;
    
    /**
     * 清理当前线程的Arena内存
     * 注意：这会释放所有arena分配的内存，需要确保数据不再需要
     */
    static void clearThreadArena();
    
    /**
     * 获取压缩性能统计
     */
    struct CompressionStats {
        size_t total_compressions;
        size_t total_decompressions;
        size_t total_input_bytes;
        size_t total_output_bytes;
        double average_compression_ratio;
        double total_processing_time_ms;
        
        double getCompressionRatio() const {
            return total_output_bytes > 0 ? 
                   static_cast<double>(total_input_bytes) / total_output_bytes : 0.0;
        }
    };
    
    CompressionStats getCompressionStats() const;
    void resetCompressionStats();
    
    // ====== 智能缓冲区管理集成（CompressBufferCache优化）======
    
    /**
     * 使用智能缓冲区缓存进行压缩
     * 自动管理缓冲区大小，提供最佳的内存使用效率
     */
    struct SmartCompressionResult {
        void* compressed_data;           // 压缩数据（来自BufferCache）
        size_t compressed_size;          // 压缩后大小
        size_t buffer_capacity;          // 分配的缓冲区容量
        size_t memory_saved;             // 相比传统方式节省的内存
        double compression_time_ms;      // 压缩耗时
        CompressBufferCache::Buffer* source_buffer; // 使用的源缓冲区
        
        // 获取压缩数据的C字符串指针
        const char* getData() const { return static_cast<const char*>(compressed_data); }
        
        // 检查压缩是否成功
        bool success() const { return compressed_data != nullptr && compressed_size > 0; }
        
        // 获取压缩比
        double getCompressionRatio(size_t originalSize) const {
            return originalSize > 0 ? static_cast<double>(originalSize) / compressed_size : 0.0;
        }
    };
    
    /**
     * 智能压缩：使用BufferCache自动管理缓冲区
     * @param src 源数据
     * @param srcLen 源数据长度
     * @param preferredLevel 压缩级别（可选）
     * @return 智能压缩结果
     */
    SmartCompressionResult compressZlibSmart(const void* src, size_t srcLen,
                                           std::optional<int> preferredLevel = std::nullopt);
    
    /**
     * 智能解压缩：使用BufferCache自动管理缓冲区
     */
    struct SmartDecompressionResult {
        void* decompressed_data;         // 解压缩数据（来自BufferCache）
        size_t decompressed_size;        // 解压缩后大小
        size_t buffer_capacity;          // 分配的缓冲区容量
        size_t memory_saved;             // 相比传统方式节省的内存
        double decompression_time_ms;    // 解压缩耗时
        CompressBufferCache::Buffer* source_buffer; // 使用的源缓冲区
        
        const char* getData() const { return static_cast<const char*>(decompressed_data); }
        bool success() const { return decompressed_data != nullptr && decompressed_size > 0; }
    };
    
    SmartDecompressionResult decompressZlibSmart(const void* compressedData, size_t compressedSize,
                                                std::optional<size_t> expectedOutputSize = std::nullopt);
    
    /**
     * 零拷贝压缩：直接使用BufferCache的缓冲区，避免内存拷贝
     * 这是最高性能的压缩方式
     */
    struct ZeroCopyCompressionResult {
        void* compressed_data;           // 压缩数据（直接在BufferCache中）
        size_t compressed_size;          // 压缩后大小
        size_t buffer_capacity;          // 缓冲区容量
        CompressBufferCache::Buffer* buffer;     // 使用的缓冲区
        bool is_compressed_inplace;      // 是否为原地压缩
        
        const char* getData() const { return static_cast<const char*>(compressed_data); }
        bool success() const { return compressed_data != nullptr && compressed_size > 0; }
    };
    
    /**
     * 零拷贝压缩（最高性能）
     * 数据直接在BufferCache分配的缓冲区中处理，完全避免拷贝
     * @param src 源数据
     * @param srcLen 源数据长度
     * @param preferredLevel 压缩级别
     * @return 零拷贝压缩结果
     */
    ZeroCopyCompressionResult compressZlibZeroCopy(const void* src, size_t srcLen,
                                                  std::optional<int> preferredLevel = std::nullopt);
    
    /**
     * 获取BufferCache统计信息
     */
    struct BufferCacheStats {
        CompressBufferCache::CacheStats cache_stats;
        size_t smart_compressions;
        size_t zero_copy_compressions;
        size_t memory_saved_bytes;
        double average_buffer_efficiency;
        
        BufferCacheStats() 
            : smart_compressions(0), zero_copy_compressions(0), 
              memory_saved_bytes(0), average_buffer_efficiency(0.0) {}
    };
    
    /**
     * 获取BufferCache统计信息
     */
    BufferCacheStats getBufferCacheStats() const;
    
    /**
     * 强制清理BufferCache（内存压力时调用）
     */
    static void cleanupBufferCache();
    
    /**
     * 预热BufferCache（服务器启动时调用）
     */
    static void warmupBufferCache();
    
    /**
     * 获取全局BufferCache实例
     */
    static CompressBufferCache& getGlobalBufferCache();
    
    /**
     * 智能批量压缩：结合Arena和BufferCache的最佳性能
     */
    struct SmartBatchCompressionResult {
        std::vector<SmartCompressionResult> results;
        size_t total_input_bytes;
        size_t total_output_bytes;
        size_t total_memory_saved;
        double total_processing_time_ms;
        size_t buffers_reused;
        size_t new_buffers_created;
        
        double getOverallCompressionRatio() const {
            return total_output_bytes > 0 ?
                   static_cast<double>(total_input_bytes) / total_output_bytes : 0.0;
        }
        
        float getBufferReuseRate() const {
            size_t total_ops = buffers_reused + new_buffers_created;
            return total_ops > 0 ?
                   static_cast<float>(buffers_reused) / total_ops : 0.0f;
        }
    };
    
    /**
     * 智能批量压缩
     * 自动选择最优的压缩策略（Arena vs BufferCache）
     */
    SmartBatchCompressionResult compressBatchSmart(const void* const* inputs,
                                                  const size_t* inputSizes,
                                                  size_t count,
                                                  std::optional<int> preferredLevel = std::nullopt);

    // Non-copyable, non-movable
    NativeCompressor(const NativeCompressor&) = delete;
    NativeCompressor& operator=(const NativeCompressor&) = delete;
    NativeCompressor(NativeCompressor&&) = delete;
    NativeCompressor& operator=(NativeCompressor&&) = delete;

private:
    // libdeflate context
    struct libdeflate_compressor* deflate_compressor_;
    struct libdeflate_decompressor* deflate_decompressor_;

    int compressionLevel_;
    
    // 性能统计
    mutable std::atomic<size_t> stats_compressions_{0};
    mutable std::atomic<size_t> stats_decompressions_{0};
    mutable std::atomic<size_t> stats_input_bytes_{0};
    mutable std::atomic<size_t> stats_output_bytes_{0};
    mutable std::atomic<double> stats_processing_time_ms_{0.0};
    
    // BufferCache集成统计
    mutable std::atomic<size_t> stats_smart_compressions_{0};
    mutable std::atomic<size_t> stats_zero_copy_compressions_{0};
    mutable std::atomic<size_t> stats_memory_saved_bytes_{0};
    mutable std::atomic<size_t> stats_buffers_reused_{0};
    mutable std::atomic<size_t> stats_new_buffers_created_{0};
    
    // 辅助方法
    size_t calculateOptimalBufferSize(size_t inputSize) const;
    void updateStats(size_t inputBytes, size_t outputBytes, double processingTimeMs) const;
    
    // BufferCache辅助方法
    void updateBufferCacheStats(size_t memorySaved, bool bufferReused) const;
    CompressBufferCache::Buffer* selectOptimalBuffer(size_t inputSize) const;
    void logSmartCompression(const SmartCompressionResult& result, size_t originalSize) const;
};
    
} // namespace net
} // namespace lattice