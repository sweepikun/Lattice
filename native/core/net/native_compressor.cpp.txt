#include "native_compressor.hpp"
#include <stdexcept>

namespace lattice {
namespace net {

NativeCompressor::NativeCompressor(int compressionLevel) 
    : compressionLevel_(compressionLevel), deflate_compressor_(nullptr), deflate_decompressor_(nullptr)
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

} // namespace net
} // namespace lattice