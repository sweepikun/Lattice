package io.lattice.network;

import io.lattice.config.LatticeConfig;
import io.lattice.nativeutil.LatticeNativeInitializer;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import java.nio.ByteBuffer;

public class NativeCompression {
    private static final Logger LOGGER = LoggerFactory.getLogger(NativeCompression.class);
    
    // Maximum allowed decompressed packet size (8 MB)
    public static final int MAX_DECOMPRESSED_SIZE = 8 * 1024 * 1024;
    
    // 通过统一初始化器检查本地库是否可用
    private static boolean nativeAvailable = false;
    
    static {
        // 检查本地库是否已加载并可用
        nativeAvailable = LatticeNativeInitializer.isFeatureNativeOptimizationAvailable(
            LatticeConfig.isPacketCompressionOptimizationEnabled());
        
        if (nativeAvailable) {
            LOGGER.info("网络压缩原生优化已启用");
        } else {
            LOGGER.info("网络压缩原生优化不可用，将使用标准Java实现");
        }
    }

    public static boolean isNativeAvailable() {
        return nativeAvailable;
    }

    // Manage compression level via LatticeConfig
    private static volatile int runtimeCompressionLevel = LatticeConfig.getCompressionLevel();

    public static int getCompressionLevel() {
        return runtimeCompressionLevel;
    }

    public static void reloadConfig() {
        LatticeConfig.reload(Path.of("config"));
        runtimeCompressionLevel = LatticeConfig.getCompressionLevel();
    }

    // byte[] based APIs (existing)
    /**
     * Compress data using zlib (DEFLATE)
     *
     * @param data Data to compress
     * @param compressionLevel Compression level
     * @return Compressed data
     */
    public static native byte[] compressDeflate(byte[] data, int compressionLevel);

    /**
     * Decompress data using zlib (DEFLATE)
     * 
     * @param data Compressed data
     * @param decompressedSize Expected size of decompressed data
     * @return Decompressed data
     */
    public static native byte[] decompressDeflate(byte[] data, int decompressedSize);

    // Direct ByteBuffer (zero-copy) APIs. Return written size or -1 on error.
    /**
     * Compress data using zlib with direct ByteBuffer
     */
    public static native int compressDeflateDirect(ByteBuffer srcDirect, int srcLen, ByteBuffer dstDirect, int dstCapacity, int compressionLevel);

    /**
     * Decompress data using zlib with direct ByteBuffer
     */
    public static native int decompressDeflateDirect(ByteBuffer srcDirect, int srcLen, ByteBuffer dstDirect, int dstCapacity);

    // Helpers to get native required bounds for allocation
    /**
     * Get maximum compressed size for zlib
     */
    public static native int deflateCompressBound(int srcLen);
}
