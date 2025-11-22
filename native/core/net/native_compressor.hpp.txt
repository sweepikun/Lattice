#pragma once

#include <memory>
#include "libdeflate.h"

namespace lattice {
namespace net {

class NativeCompressor {
public:
    NativeCompressor(int compressionLevel = 6);
    ~NativeCompressor();

    // libdeflate compression (zlib format)
    size_t compressZlib(const char* src, size_t srcLen, char* dst, size_t dstCapacity);
    size_t decompressZlib(const char* src, size_t srcLen, char* dst, size_t dstCapacity);

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
    // libdeflate context
    struct libdeflate_compressor* deflate_compressor_;
    struct libdeflate_decompressor* deflate_decompressor_;

    int compressionLevel_;
};
    
} // namespace net
} // namespace lattice