package io.lattice.chunk;

import io.lattice.network.NativeCompression;

import java.nio.ByteBuffer;

/**
 * Native chunk compressor using libdeflate for high-performance chunk serialization.
 * This class provides optimized compression for Minecraft chunk data using native
 * libdeflate library with zero-copy ByteBuffer operations.
 */
public class NativeChunkCompressor {
    
    /**
     * Compress chunk data using libdeflate zlib compression with direct ByteBuffer
     * for zero-copy operations.
     * 
     * @param srcDirect Source direct ByteBuffer containing uncompressed chunk data
     * @param srcLen Length of data to compress
     * @param compressionLevel Compression level (1-12 for libdeflate)
     * @return Compressed data as a new direct ByteBuffer, or null if compression failed
     */
    public static ByteBuffer compressChunkData(ByteBuffer srcDirect, int srcLen, int compressionLevel) {
        if (!NativeCompression.isNativeAvailable()) {
            throw new RuntimeException("Native compression library is not available");
        }
        
        if (!srcDirect.isDirect()) {
            throw new IllegalArgumentException("Source ByteBuffer must be direct for zero-copy operation");
        }
        
        // Calculate maximum compressed size
        int maxCompressedSize = NativeCompression.zlibCompressBound(srcLen);
        
        // Allocate direct buffer for compressed data
        ByteBuffer dstDirect = ByteBuffer.allocateDirect(maxCompressedSize);
        
        // Perform compression
        int compressedSize = NativeCompression.compressZlibDirect(
            srcDirect, srcLen, dstDirect, maxCompressedSize, compressionLevel);
        
        if (compressedSize <= 0) {
            return null; // Compression failed
        }
        
        // Limit the buffer to actual compressed size
        dstDirect.limit(compressedSize);
        dstDirect.position(0);
        
        return dstDirect;
    }
    
    /**
     * Decompress chunk data using libdeflate zlib decompression with direct ByteBuffer
     * for zero-copy operations.
     * 
     * @param srcDirect Source direct ByteBuffer containing compressed chunk data
     * @param srcLen Length of compressed data
     * @param decompressedSize Expected size of decompressed data
     * @return Decompressed data as a new direct ByteBuffer, or null if decompression failed
     */
    public static ByteBuffer decompressChunkData(ByteBuffer srcDirect, int srcLen, int decompressedSize) {
        if (!NativeCompression.isNativeAvailable()) {
            throw new RuntimeException("Native compression library is not available");
        }
        
        if (!srcDirect.isDirect()) {
            throw new IllegalArgumentException("Source ByteBuffer must be direct for zero-copy operation");
        }
        
        // Allocate direct buffer for decompressed data
        ByteBuffer dstDirect = ByteBuffer.allocateDirect(decompressedSize);
        
        // Perform decompression
        int actualDecompressedSize = NativeCompression.decompressZlibDirect(
            srcDirect, srcLen, dstDirect, decompressedSize);
        
        if (actualDecompressedSize != decompressedSize) {
            return null; // Decompression failed or size mismatch
        }
        
        // Prepare buffer for reading
        dstDirect.limit(actualDecompressedSize);
        dstDirect.position(0);
        
        return dstDirect;
    }
    
    /**
     * Get the maximum compressed size for a given input size using zlib compression
     * 
     * @param srcLen Size of uncompressed data
     * @return Maximum possible size of compressed data
     */
    public static int getMaxCompressedSize(int srcLen) {
        if (!NativeCompression.isNativeAvailable()) {
            // Fallback estimation if native library is not available
            return (int) (srcLen * 1.1) + 12; // Rough estimation
        }
        
        return NativeCompression.zlibCompressBound(srcLen);
    }
}