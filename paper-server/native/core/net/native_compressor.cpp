#include "native_compressor.hpp"
#include <stdexcept>
#include <cstring>

namespace lattice {
namespace net {

NativeCompressor::NativeCompressor(int compressionLevel) 
    : deflate_compressor_(nullptr), deflate_decompressor_(nullptr), compressionLevel_(compressionLevel)
{
    // Allocate libdeflate contexts
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

size_t NativeCompressor::compressDeflate(const char* src, size_t srcLen, char* dst, size_t dstCapacity) {
    if (!src || srcLen == 0 || !dst || !deflate_compressor_) return static_cast<size_t>(-1);
    // libdeflate_deflate_compress returns compressed size, or 0 on error (per libdeflate API)
    size_t written = libdeflate_deflate_compress(deflate_compressor_, src, srcLen, dst, dstCapacity);
    if (written == 0) return static_cast<size_t>(-1);
    return written;
}

size_t NativeCompressor::decompressDeflate(const char* src, size_t srcLen, char* dst, size_t dstCapacity) {
    if (!src || srcLen == 0 || !dst || !deflate_decompressor_) return static_cast<size_t>(-1);
    size_t actual_out = 0;
    enum libdeflate_result result = libdeflate_deflate_decompress(deflate_decompressor_, src, srcLen, dst, dstCapacity, &actual_out);
    if (result != LIBDEFLATE_SUCCESS) return static_cast<size_t>(-1);
    return static_cast<size_t>(actual_out);
}

// Set compression level at runtime. For zlib compress2 we just update the level.
void NativeCompressor::setCompressionLevel(int level) {
    if (level == compressionLevel_) return;
    compressionLevel_ = level;
    // libdeflate does not support changing compressor level after allocation; reallocate
    if (deflate_compressor_) libdeflate_free_compressor(deflate_compressor_);
    deflate_compressor_ = libdeflate_alloc_compressor(compressionLevel_);
    if (!deflate_compressor_) throw std::runtime_error("Failed to reallocate libdeflate compressor with new level");
}

NativeCompressor* NativeCompressor::forThread(int compressionLevel) {
    thread_local std::unique_ptr<NativeCompressor> instance;
    if (!instance) {
        instance.reset(new NativeCompressor(compressionLevel));
    } else if (instance->compressionLevel_ != compressionLevel) {
        instance->setCompressionLevel(compressionLevel);
    }
    return instance.get();
}

} // namespace net
} // namespace lattice
