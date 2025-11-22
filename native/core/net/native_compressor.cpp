#include "native_compressor.hpp"
#include <stdexcept>
#include <chrono>
#include <algorithm>
#include <iostream>
#include <cstdio>
#include <cstring>
#include <string>

namespace lattice {
namespace net {

NativeCompressor::NativeCompressor(int compressionLevel) 
    : deflate_compressor_(nullptr), deflate_decompressor_(nullptr), compressionLevel_(compressionLevel)
{
    // Initialize libdeflate contexts
    deflate_compressor_ = libdeflate_alloc_compressor(compressionLevel);
    if (!deflate_compressor_) {
        throw std::runtime_error("Failed to create libdeflate compressor");
    }

    deflate_decompressor_ = libdeflate_alloc_decompressor();
    if (!deflate_decompressor_) {
        libdeflate_free_compressor(deflate_compressor_);
        throw std::runtime_error("Failed to create libdeflate decompressor");
    }
}

NativeCompressor::~NativeCompressor() {
    if (deflate_compressor_) libdeflate_free_compressor(deflate_compressor_);
    if (deflate_decompressor_) libdeflate_free_decompressor(deflate_decompressor_);
}

// ====== 传统接口实现（保持向后兼容）======
size_t NativeCompressor::compressZlib(const char* src, size_t srcLen, char* dst, size_t dstCapacity) {
    return libdeflate_zlib_compress(deflate_compressor_, src, srcLen, dst, dstCapacity);
}

size_t NativeCompressor::decompressZlib(const char* src, size_t srcLen, char* dst, size_t dstCapacity) {
    size_t actualOutSize;
    enum libdeflate_result result = libdeflate_zlib_decompress(
        deflate_decompressor_, src, srcLen, dst, dstCapacity, &actualOutSize);
    
    if (result != LIBDEFLATE_SUCCESS) {
        return 0; // Return 0 to indicate error
    }
    
    return actualOutSize;
}

void NativeCompressor::setCompressionLevel(int level) {
    compressionLevel_ = level;
    
    // We need to recreate the compressor with the new level
    if (deflate_compressor_) {
        libdeflate_free_compressor(deflate_compressor_);
    }
    
    deflate_compressor_ = libdeflate_alloc_compressor(level);
    if (!deflate_compressor_) {
        throw std::runtime_error("Failed to create libdeflate compressor");
    }
}

NativeCompressor* NativeCompressor::forThread(int compressionLevel) {
    thread_local static std::unique_ptr<NativeCompressor> instance = nullptr;
    
    if (!instance || instance->compressionLevel_ != compressionLevel) {
        instance = std::make_unique<NativeCompressor>(compressionLevel);
    }
    
    return instance.get();
}

// ====== Arena优化接口实现 =======

// 辅助方法：计算最优缓冲区大小
size_t NativeCompressor::calculateOptimalBufferSize(size_t inputSize) const {
    // 对于压缩，输出缓冲区通常需要比输入稍大
    // libdeflate通常需要 0.1% 的额外空间 + 少量固定开销
    return std::max(inputSize + (inputSize / 1000) + 1024, inputSize + 1024);
}

// 辅助方法：更新性能统计
void NativeCompressor::updateStats(size_t inputBytes, size_t outputBytes, double processingTimeMs) const {
    stats_compressions_.fetch_add(1, std::memory_order_relaxed);
    stats_input_bytes_.fetch_add(inputBytes, std::memory_order_relaxed);
    stats_output_bytes_.fetch_add(outputBytes, std::memory_order_relaxed);
    stats_processing_time_ms_.fetch_add(processingTimeMs, std::memory_order_relaxed);
}

// Arena压缩实现
NativeCompressor::ArenaCompressionResult NativeCompressor::compressZlibArena(
    const void* src, size_t srcLen, std::optional<int> preferredCompressionLevel) {
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    ArenaCompressionResult result = {};
    result.compressed_data = nullptr;
    result.compressed_size = 0;
    result.memory_used = 0;
    
    if (!src || srcLen == 0) {
        return result;
    }
    
    // 获取线程局部的Arena实例
    MemoryArena& arena = MemoryArena::forThread();
    
    // 预先获取Arena统计（压缩前）
    auto arenaStatsBefore = arena.getMemoryStats();
    
    try {
        // 计算最优缓冲区大小
        size_t optimalBufferSize = calculateOptimalBufferSize(srcLen);
        
        // 从Arena分配缓冲区
        char* outputBuffer = static_cast<char*>(arena.allocate(optimalBufferSize));
        if (!outputBuffer) {
            return result; // 分配失败
        }
        
        // 执行压缩
        size_t compressedSize;
        if (preferredCompressionLevel.has_value() && preferredCompressionLevel.value() != compressionLevel_) {
            // 使用指定的压缩级别
            struct libdeflate_compressor* temp_compressor = libdeflate_alloc_compressor(preferredCompressionLevel.value());
            if (temp_compressor) {
                compressedSize = libdeflate_zlib_compress(temp_compressor, src, srcLen, outputBuffer, optimalBufferSize);
                libdeflate_free_compressor(temp_compressor);
            } else {
                return result;
            }
        } else {
            compressedSize = libdeflate_zlib_compress(deflate_compressor_, src, srcLen, outputBuffer, optimalBufferSize);
        }
        
        if (compressedSize > 0) {
            result.compressed_data = outputBuffer;
            result.compressed_size = compressedSize;
            
            // 获取压缩后的Arena统计
            auto arenaStatsAfter = arena.getMemoryStats();
            result.memory_used = arenaStatsAfter.total_used - arenaStatsBefore.total_used;
            result.arena_stats = arenaStatsAfter;
            
            // 更新性能统计
            auto endTime = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration<double, std::milli>(endTime - startTime).count();
            updateStats(srcLen, compressedSize, duration);
        } else {
            // 压缩失败，但缓冲区仍被分配（arena中的内存）
            // 这种情况下我们不返回错误，而是返回空结果
        }
        
    } catch (const std::exception&) {
        // 发生异常时，返回空结果
        // Arena中的内存将自动清理
    }
    
    return result;
}

// Arena解压缩实现
NativeCompressor::ArenaDecompressionResult NativeCompressor::decompressZlibArena(
    const void* compressedData, size_t compressedSize, std::optional<size_t> expectedOutputSize) {
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    ArenaDecompressionResult result = {};
    result.decompressed_data = nullptr;
    result.decompressed_size = 0;
    result.memory_used = 0;
    
    if (!compressedData || compressedSize == 0) {
        return result;
    }
    
    // 获取线程局部的Arena实例
    MemoryArena& arena = MemoryArena::forThread();
    
    // 预先获取Arena统计
    auto arenaStatsBefore = arena.getMemoryStats();
    
    try {
        // 估算输出大小（如果未提供）
        size_t estimatedOutputSize = expectedOutputSize.value_or(compressedSize * 4); // 4倍是保守估计
        
        // 为安全起见，分阶段分配：先尝试小的，再逐步增大
        size_t bufferSizes[] = {
            estimatedOutputSize,
            estimatedOutputSize * 2,
            compressedSize * 8,  // 最大8倍
            16 * 1024 * 1024    // 16MB上限
        };
        
        for (size_t bufferSize : bufferSizes) {
            char* outputBuffer = static_cast<char*>(arena.allocate(bufferSize));
            if (!outputBuffer) {
                continue; // 分配失败，尝试下一个大小
            }
            
            size_t actualOutputSize;
            enum libdeflate_result libResult = libdeflate_zlib_decompress(
                deflate_decompressor_, 
                compressedData, compressedSize, 
                outputBuffer, bufferSize, 
                &actualOutputSize);
            
            if (libResult == LIBDEFLATE_SUCCESS) {
                result.decompressed_data = outputBuffer;
                result.decompressed_size = actualOutputSize;
                
                // 获取Arena统计
                auto arenaStatsAfter = arena.getMemoryStats();
                result.memory_used = arenaStatsAfter.total_used - arenaStatsBefore.total_used;
                result.arena_stats = arenaStatsAfter;
                
                // 更新性能统计
                auto endTime = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration<double, std::milli>(endTime - startTime).count();
                
                stats_decompressions_.fetch_add(1, std::memory_order_relaxed);
                stats_input_bytes_.fetch_add(compressedSize, std::memory_order_relaxed);
                stats_output_bytes_.fetch_add(actualOutputSize, std::memory_order_relaxed);
                stats_processing_time_ms_.fetch_add(duration, std::memory_order_relaxed);
                
                break; // 成功，解压缩完成
            }
            // 如果失败，继续尝试更大的缓冲区
        }
        
    } catch (const std::exception&) {
        // 发生异常时，返回空结果
    }
    
    return result;
}

// 批量压缩实现
NativeCompressor::BatchCompressionResult NativeCompressor::compressBatchArena(
    const void* const* inputs, const size_t* inputSizes, size_t count) {
    
    BatchCompressionResult result;
    
    if (!inputs || !inputSizes || count == 0) {
        return result;
    }
    
    auto arenaStatsBefore = MemoryArena::forThread().getMemoryStats();
    size_t beforeMemory = arenaStatsBefore.total_used;
    
    // 处理每个输入
    for (size_t i = 0; i < count; ++i) {
        if (inputs[i] && inputSizes[i] > 0) {
            auto compressionResult = compressZlibArena(inputs[i], inputSizes[i]);
            result.results.push_back(std::move(compressionResult));
        }
    }
    
    // 获取总统计
    auto arenaStatsAfter = MemoryArena::forThread().getMemoryStats();
    result.total_stats = arenaStatsAfter;
    
    (void)beforeMemory; // Suppress unused variable warning
    
    return result;
}

// 批量解压缩实现
NativeCompressor::BatchDecompressionResult NativeCompressor::decompressBatchArena(
    const void* const* compressedInputs, 
    const size_t* compressedSizes,
    const std::optional<size_t>* expectedOutputSizes,
    size_t count) {
    
    BatchDecompressionResult result;
    
    if (!compressedInputs || !compressedSizes || count == 0) {
        return result;
    }
    
    auto arenaStatsBefore = MemoryArena::forThread().getMemoryStats();
    size_t beforeMemory = arenaStatsBefore.total_used;
    
    // 处理每个输入
    for (size_t i = 0; i < count; ++i) {
        if (compressedInputs[i] && compressedSizes[i] > 0) {
            std::optional<size_t> expectedSize = expectedOutputSizes ? expectedOutputSizes[i] : std::nullopt;
            auto decompressionResult = decompressZlibArena(compressedInputs[i], compressedSizes[i], expectedSize);
            result.results.push_back(std::move(decompressionResult));
        }
    }
    
    // 获取总统计
    auto arenaStatsAfter = MemoryArena::forThread().getMemoryStats();
    result.total_stats = arenaStatsAfter;
    
    (void)beforeMemory; // Suppress unused variable warning
    
    return result;
}

// 获取Arena统计
MemoryArena::MemoryStats NativeCompressor::getArenaStats() const {
    return MemoryArena::forThread().getMemoryStats();
}

// 清理线程Arena
void NativeCompressor::clearThreadArena() {
    MemoryArena::clearCurrentThread();
}

// 获取压缩统计
NativeCompressor::CompressionStats NativeCompressor::getCompressionStats() const {
    CompressionStats stats;
    stats.total_compressions = stats_compressions_.load(std::memory_order_relaxed);
    stats.total_decompressions = stats_decompressions_.load(std::memory_order_relaxed);
    stats.total_input_bytes = stats_input_bytes_.load(std::memory_order_relaxed);
    stats.total_output_bytes = stats_output_bytes_.load(std::memory_order_relaxed);
    stats.total_processing_time_ms = stats_processing_time_ms_.load(std::memory_order_relaxed);
    
    stats.average_compression_ratio = (stats.total_output_bytes > 0) ? 
        static_cast<double>(stats.total_input_bytes) / stats.total_output_bytes : 0.0;
    
    return stats;
}

// 重置压缩统计
void NativeCompressor::resetCompressionStats() {
    stats_compressions_.store(0, std::memory_order_relaxed);
    stats_decompressions_.store(0, std::memory_order_relaxed);
    stats_input_bytes_.store(0, std::memory_order_relaxed);
    stats_output_bytes_.store(0, std::memory_order_relaxed);
    stats_processing_time_ms_.store(0.0, std::memory_order_relaxed);
}

// ====== 智能缓冲区管理实现（CompressBufferCache优化）======

// 智能压缩
NativeCompressor::SmartCompressionResult NativeCompressor::compressZlibSmart(
    const void* src, size_t srcLen, std::optional<int> preferredLevel) {
    
    (void)preferredLevel; // Unused parameter - using default compression level
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    SmartCompressionResult result;
    result.compressed_data = nullptr;
    result.compressed_size = 0;
    result.buffer_capacity = 0;
    result.memory_saved = 0;
    result.compression_time_ms = 0.0;
    result.source_buffer = nullptr;
    
    try {
        // 获取全局BufferCache
        auto& bufferCache = getGlobalBufferCache();
        
        // 选择最优缓冲区
        auto* buffer = selectOptimalBuffer(srcLen);
        if (!buffer) {
            std::cerr << "[NativeCompressor] Failed to get optimal buffer" << std::endl;
            return result;
        }
        
        // 确保缓冲区有足够容量
        size_t estimatedSize = bufferCache.estimateOptimalSize(srcLen);
        if (buffer->capacity < estimatedSize) {
            // 计算需要调整的大小
            size_t newCapacity = estimatedSize;
            if (buffer->resize(newCapacity)) {
                std::cout << "[NativeCompressor] Buffer resized from " 
                          << buffer->capacity << " to " << newCapacity << std::endl;
            } else {
                std::cerr << "[NativeCompressor] Failed to resize buffer" << std::endl;
                return result;
            }
        }
        
        // 执行压缩
        size_t actualSize = libdeflate_zlib_compress(
            deflate_compressor_, 
            static_cast<const char*>(src), srcLen,
            static_cast<char*>(buffer->data), buffer->capacity
        );
        
        if (actualSize == 0) {
            std::cerr << "[NativeCompressor] Compression failed" << std::endl;
            return result;
        }
        
        // 填充结果
        result.compressed_data = buffer->data;
        result.compressed_size = actualSize;
        result.buffer_capacity = buffer->capacity;
        result.source_buffer = buffer;
        
        // 计算节省的内存（相比直接分配）
        size_t traditionalSize = srcLen + 1024; // 传统方式需要的大小
        result.memory_saved = (traditionalSize > buffer->capacity) ? 
                             (traditionalSize - buffer->capacity) : 0;
        
        // 更新统计
        stats_smart_compressions_.fetch_add(1, std::memory_order_relaxed);
        stats_memory_saved_bytes_.fetch_add(result.memory_saved, std::memory_order_relaxed);
        if (result.memory_saved > 0) {
            stats_buffers_reused_.fetch_add(1, std::memory_order_relaxed);
        } else {
            stats_new_buffers_created_.fetch_add(1, std::memory_order_relaxed);
        }
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
        result.compression_time_ms = duration.count() / 1000.0;
        
        // 记录日志
        logSmartCompression(result, srcLen);
        
    } catch (const std::exception& e) {
        std::cerr << "[NativeCompressor] Smart compression error: " << e.what() << std::endl;
    }
    
    return result;
}

// 智能解压缩
NativeCompressor::SmartDecompressionResult NativeCompressor::decompressZlibSmart(
    const void* compressedData, size_t compressedSize, std::optional<size_t> expectedOutputSize) {
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    SmartDecompressionResult result;
    result.decompressed_data = nullptr;
    result.decompressed_size = 0;
    result.buffer_capacity = 0;
    result.memory_saved = 0;
    result.decompression_time_ms = 0.0;
    result.source_buffer = nullptr;
    
    try {
        auto& bufferCache = getGlobalBufferCache();
        
        // 估算输出大小（如果未提供）
        size_t outputSize = expectedOutputSize.value_or(compressedSize * 2);
        auto* buffer = selectOptimalBuffer(outputSize);
        
        if (!buffer) {
            std::cerr << "[NativeCompressor] Failed to get optimal buffer for decompression" << std::endl;
            return result;
        }
        
        // 确保缓冲区足够大
        if (buffer->capacity < outputSize) {
            size_t newCapacity = bufferCache.estimateOptimalSize(outputSize);
            if (!buffer->resize(newCapacity)) {
                std::cerr << "[NativeCompressor] Failed to resize buffer for decompression" << std::endl;
                return result;
            }
        }
        
        // 执行解压缩
        size_t actualSize;
        enum libdeflate_result libResult = libdeflate_zlib_decompress(
            deflate_decompressor_,
            static_cast<const char*>(compressedData), compressedSize,
            static_cast<char*>(buffer->data), buffer->capacity,
            &actualSize
        );
        
        if (libResult != LIBDEFLATE_SUCCESS) {
            std::cerr << "[NativeCompressor] Decompression failed with error: " << libResult << std::endl;
            return result;
        }
        
        // 填充结果
        result.decompressed_data = buffer->data;
        result.decompressed_size = actualSize;
        result.buffer_capacity = buffer->capacity;
        result.source_buffer = buffer;
        
        // 计算内存节省
        size_t traditionalSize = actualSize + 1024;
        result.memory_saved = (traditionalSize > buffer->capacity) ?
                             (traditionalSize - buffer->capacity) : 0;
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
        result.decompression_time_ms = duration.count() / 1000.0;
        
    } catch (const std::exception& e) {
        std::cerr << "[NativeCompressor] Smart decompression error: " << e.what() << std::endl;
    }
    
    return result;
}

// 零拷贝压缩
NativeCompressor::ZeroCopyCompressionResult NativeCompressor::compressZlibZeroCopy(
    const void* src, size_t srcLen, std::optional<int> preferredLevel) {
    
    (void)preferredLevel; // Unused parameter - using default compression level
    
    ZeroCopyCompressionResult result;
    result.compressed_data = nullptr;
    result.compressed_size = 0;
    result.buffer_capacity = 0;
    result.buffer = nullptr;
    result.is_compressed_inplace = false;
    
    try {
        auto& bufferCache = getGlobalBufferCache();
        
        // 获取适合的缓冲区
        auto* buffer = selectOptimalBuffer(srcLen);
        if (!buffer) {
            std::cerr << "[NativeCompressor] Failed to get buffer for zero-copy compression" << std::endl;
            return result;
        }
        
        // 确保缓冲区足够大
        size_t estimatedSize = bufferCache.estimateOptimalSize(srcLen);
        if (buffer->capacity < estimatedSize) {
            if (!buffer->resize(estimatedSize)) {
                std::cerr << "[NativeCompressor] Failed to resize buffer for zero-copy" << std::endl;
                return result;
            }
        }
        
        // 复制源数据到缓冲区
        std::memcpy(buffer->data, src, srcLen);
        
        // 在缓冲区内进行压缩
        size_t compressedSize = libdeflate_zlib_compress(
            deflate_compressor_,
            static_cast<const char*>(buffer->data), srcLen,
            static_cast<char*>(buffer->data), buffer->capacity
        );
        
        if (compressedSize == 0) {
            std::cerr << "[NativeCompressor] Zero-copy compression failed" << std::endl;
            return result;
        }
        
        // 填充结果
        result.compressed_data = buffer->data;
        result.compressed_size = compressedSize;
        result.buffer_capacity = buffer->capacity;
        result.buffer = buffer;
        result.is_compressed_inplace = true;
        
        // 更新统计
        stats_zero_copy_compressions_.fetch_add(1, std::memory_order_relaxed);
        
    } catch (const std::exception& e) {
        std::cerr << "[NativeCompressor] Zero-copy compression error: " << e.what() << std::endl;
    }
    
    return result;
}

// 获取BufferCache统计
NativeCompressor::BufferCacheStats NativeCompressor::getBufferCacheStats() const {
    BufferCacheStats stats;
    
    stats.cache_stats = getGlobalBufferCache().getStats();
    stats.smart_compressions = stats_smart_compressions_.load(std::memory_order_relaxed);
    stats.zero_copy_compressions = stats_zero_copy_compressions_.load(std::memory_order_relaxed);
    stats.memory_saved_bytes = stats_memory_saved_bytes_.load(std::memory_order_relaxed);
    
    // 计算平均缓冲区效率
    size_t totalOps = stats.smart_compressions + stats.zero_copy_compressions;
    if (totalOps > 0) {
        stats.average_buffer_efficiency = 
            static_cast<double>(stats.memory_saved_bytes) / totalOps;
    }
    
    return stats;
}

// 清理BufferCache
void NativeCompressor::cleanupBufferCache() {
    getGlobalBufferCache().performCleanup();
    std::cout << "[NativeCompressor] BufferCache cleanup completed" << std::endl;
}

// 预热BufferCache
void NativeCompressor::warmupBufferCache() {
    auto& bufferCache = getGlobalBufferCache();
    
    // 预热常见大小的缓冲区
    std::vector<size_t> warmupSizes = {
        1024,           // 1KB
        4 * 1024,       // 4KB
        16 * 1024,      // 16KB
        64 * 1024,      // 64KB
        256 * 1024,     // 256KB
        1024 * 1024     // 1MB
    };
    
    for (size_t size : warmupSizes) {
        bufferCache.warmupBuffer(size, 2); // 每个大小预分配2个缓冲区
    }
    
    std::cout << "[NativeCompressor] BufferCache warmup completed" << std::endl;
}

// 获取全局BufferCache
CompressBufferCache& NativeCompressor::getGlobalBufferCache() {
    static CompressBufferCache globalCache(CompressBufferCache::Config{});
    return globalCache;
}

// 智能批量压缩
NativeCompressor::SmartBatchCompressionResult NativeCompressor::compressBatchSmart(
    const void* const* inputs, const size_t* inputSizes, size_t count,
    std::optional<int> preferredLevel) {
    
    SmartBatchCompressionResult result;
    result.total_input_bytes = 0;
    result.total_output_bytes = 0;
    result.total_memory_saved = 0;
    result.total_processing_time_ms = 0.0;
    result.buffers_reused = 0;
    result.new_buffers_created = 0;
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    result.results.reserve(count);
    
    for (size_t i = 0; i < count; ++i) {
        auto compressionResult = compressZlibSmart(inputs[i], inputSizes[i], preferredLevel);
        result.results.push_back(compressionResult);
        
        if (compressionResult.success()) {
            result.total_input_bytes += inputSizes[i];
            result.total_output_bytes += compressionResult.compressed_size;
            result.total_memory_saved += compressionResult.memory_saved;
            
            if (compressionResult.memory_saved > 0) {
                result.buffers_reused++;
            } else {
                result.new_buffers_created++;
            }
        }
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    result.total_processing_time_ms = duration.count() / 1000.0;
    
    std::cout << "[NativeCompressor] Smart batch compression completed: "
              << count << " items, " << result.total_memory_saved << " bytes saved" << std::endl;
    
    return result;
}

// ====== 私有辅助方法实现 ======

void NativeCompressor::updateBufferCacheStats(size_t memorySaved, bool bufferReused) const {
    stats_memory_saved_bytes_.fetch_add(memorySaved, std::memory_order_relaxed);
    if (bufferReused) {
        stats_buffers_reused_.fetch_add(1, std::memory_order_relaxed);
    } else {
        stats_new_buffers_created_.fetch_add(1, std::memory_order_relaxed);
    }
}

CompressBufferCache::Buffer* NativeCompressor::selectOptimalBuffer(size_t inputSize) const {
    auto& bufferCache = getGlobalBufferCache();
    return bufferCache.getBuffer(inputSize);
}

void NativeCompressor::logSmartCompression(const SmartCompressionResult& result, size_t originalSize) const {
    if (result.success()) {
        std::cout << "[NativeCompressor] Smart compression: "
                  << originalSize << " -> " << result.compressed_size
                  << " bytes (" << result.getCompressionRatio(originalSize) << "x ratio, "
                  << result.memory_saved << " bytes saved, "
                  << result.compression_time_ms << " ms)" << std::endl;
    }
}

} // namespace net
} // namespace lattice