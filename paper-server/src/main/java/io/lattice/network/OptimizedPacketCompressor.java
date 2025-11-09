package io.lattice.network;

import io.lattice.config.LatticeConfig;
import java.nio.ByteBuffer;
import java.util.ArrayDeque;
import java.util.Queue;
import java.util.zip.Deflater;
import java.util.zip.Inflater;

/**
 * Optimized packet compressor using native compression with thread-local and zero-copy optimizations.
 * This implementation follows the best practices outlined in the awa.md document to minimize
 * JNI overhead and maximize compression performance for small packets.
 */
public class OptimizedPacketCompressor {
    
    // Native compression instance, will be null if optimization is disabled
    private final NativeCompression nativeCompression = LatticeConfig.isPacketCompressionOptimizationEnabled() ? 
        new NativeCompression() : null;
    
    // Fallback Java compression instances
    private final Deflater deflater;
    private final Inflater inflater;
    
    // Pre-allocated direct buffers for compression operations
    private final ThreadLocal<ByteBuffer> srcBufferLocal = ThreadLocal.withInitial(() -> 
        ByteBuffer.allocateDirect(8 * 1024 * 1024)); // 8MB should be enough for most packets
    
    private final ThreadLocal<ByteBuffer> dstBufferLocal = ThreadLocal.withInitial(() -> 
        ByteBuffer.allocateDirect(8 * 1024 * 1024)); // 8MB should be enough for most packets
    
    // Batch compression support
    private final Queue<ByteBuffer> packetQueue = new ArrayDeque<>();
    private final int maxBatchSize;
    
    public OptimizedPacketCompressor() {
        this(100); // Default max batch size of 100 packets
    }
    
    public OptimizedPacketCompressor(int maxBatchSize) {
        this.maxBatchSize = maxBatchSize;
        this.deflater = new Deflater(LatticeConfig.getCompressionLevel());
        this.inflater = new Inflater();
    }
    
    /**
     * Compress a single packet using the optimized native compression.
     * This method uses thread-local compressors and direct byte buffers to minimize overhead.
     * 
     * @param data The packet data to compress
     * @return Compressed packet data, or null if compression failed
     */
    public byte[] compressPacket(byte[] data) {
        if (nativeCompression != null && NativeCompression.isNativeAvailable()) {
            // Use native compression if optimization is enabled and available
            // Get thread-local buffers
            ByteBuffer srcBuffer = srcBufferLocal.get();
            ByteBuffer dstBuffer = dstBufferLocal.get();
            
            // Prepare source buffer
            srcBuffer.clear();
            if (data.length > srcBuffer.capacity()) {
                // Handle packets larger than our buffer
                srcBuffer = ByteBuffer.allocateDirect(data.length);
            }
            srcBuffer.put(data);
            srcBuffer.flip();
            
            // Prepare destination buffer
            int maxCompressedSize = NativeCompression.zlibCompressBound(data.length);
            dstBuffer.clear();
            if (maxCompressedSize > dstBuffer.capacity()) {
                // Handle packets that might compress to larger than our buffer
                dstBuffer = ByteBuffer.allocateDirect(maxCompressedSize);
            }
            
            // Perform compression using thread-local optimized method
            int compressedSize = NativeCompression.compressZlibDirectThreadLocal(
                srcBuffer, 0, data.length, dstBuffer, 0);
            
            if (compressedSize <= 0) {
                return null; // Compression failed
            }
            
            // Copy compressed data to byte array
            byte[] result = new byte[compressedSize];
            dstBuffer.position(0);
            dstBuffer.limit(compressedSize);
            dstBuffer.get(result);
            
            return result;
        } else {
            // Fallback to Java implementation if native is not available or optimization is disabled
            deflater.reset();
            deflater.setInput(data);
            deflater.finish();
            
            byte[] output = new byte[data.length];
            int compressedLength = deflater.deflate(output);
            
            if (!deflater.finished()) {
                // Buffer was too small, try with a larger one
                output = new byte[data.length * 2];
                deflater.reset();
                deflater.setInput(data);
                deflater.finish();
                compressedLength = deflater.deflate(output);
            }
            
            if (compressedLength < data.length) {
                // Compression was effective
                byte[] compressed = new byte[compressedLength];
                System.arraycopy(output, 0, compressed, 0, compressedLength);
                return compressed;
            } else {
                // Compression was not effective, return original data
                return data;
            }
        }
    }
    
    /**
     * Batch compress multiple packets to reduce JNI call overhead.
     * This method can significantly improve performance when compressing many small packets.
     * 
     * @param packets The packets to compress
     * @return Array of compressed packets in the same order
     */
    public byte[][] compressPacketsBatch(byte[][] packets) {
        if (nativeCompression != null && NativeCompression.isNativeAvailable()) {
            // Use native compression if optimization is enabled and available
            byte[][] results = new byte[packets.length][];
            for (int i = 0; i < packets.length; i++) {
                results[i] = compressPacket(packets[i]);
            }
            return results;
        } else {
            // Fallback to individual compression if native is not available or optimization is disabled
            byte[][] results = new byte[packets.length][];
            for (int i = 0; i < packets.length; i++) {
                results[i] = compressPacket(packets[i]);
            }
            return results;
        }
    }
    
    /**
     * Clean up thread-local resources when the compressor is no longer needed.
     * This should be called when a thread is shutting down to prevent memory leaks.
     */
    public void cleanup() {
        if (nativeCompression != null) {
            NativeCompression.cleanupThreadLocal();
        }
        // Note: Direct ByteBuffers will be garbage collected automatically
    }
    
    /**
     * Close the compressor and release all resources.
     * This should be called when the compressor is no longer needed.
     */
    public void close() {
        if (nativeCompression != null) {
            nativeCompression.close();
        }
        deflater.end();
        inflater.end();
    }
}