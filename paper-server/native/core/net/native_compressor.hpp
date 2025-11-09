#pragma once

#include <memory>
#include "libdeflate.h"

namespace lattice {
namespace net {

class NativeCompressor {
public:
    NativeCompressor(int compressionLevel);
    ~NativeCompressor();

    // DEFLATE via zlib
    size_t compressDeflate(const char* src, size_t srcLen, char* dst, size_t dstCapacity);
    size_t decompressDeflate(const char* src, size_t srcLen, char* dst, size_t dstCapacity);

    // Legacy-named wrappers (kept for compatibility) delegate to the DEFLATE implementations
    size_t compressZstd(const char* src, size_t srcLen, char* dst, size_t dstCapacity);
    size_t decompressZstd(const char* src, size_t srcLen, char* dst, size_t dstCapacity);

    // Runtime configuration
    void setCompressionLevel(int level);

    // Get per-thread instance (creates if needed). Use this to avoid sharing contexts across threads.
    static NativeCompressor* forThread(int compressionLevel);

    // Non-copyable, non-movable
    NativeCompressor(const NativeCompressor&) = delete;
    NativeCompressor& operator=(const NativeCompressor&) = delete;
    NativeCompressor(NativeCompressor&&) = delete;
    NativeCompressor& operator=(NativeCompressor&&) = delete;

private:
    // libdeflate contexts for DEFLATE (fast zlib-compatible implementation)
    struct libdeflate_compressor* deflate_compressor_;
    struct libdeflate_decompressor* deflate_decompressor_;

    int compressionLevel_;
};
    
} // namespace net
} // namespace lattice
