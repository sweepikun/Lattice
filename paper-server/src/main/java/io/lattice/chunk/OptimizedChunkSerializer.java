package io.lattice.chunk;

import io.netty.buffer.ByteBuf;
import io.netty.buffer.ByteBufAllocator;
import io.netty.buffer.CompositeByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.nbt.NbtAccounter;
import net.minecraft.nbt.NbtIo;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.world.level.chunk.LevelChunk;
import net.minecraft.world.level.chunk.storage.SerializableChunkData;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.channels.FileChannel;
import java.util.zip.DataFormatException;
import java.util.zip.Deflater;

/**
 * Optimized chunk serializer that uses native libdeflate compression for improved performance.
 * This implementation reduces CPU overhead during chunk serialization and deserialization
 * by using high-performance native compression.
 */
public class OptimizedChunkSerializer {
    
    // Default compression level for chunk data (6 is a good balance between speed and compression)
    private static final int DEFAULT_COMPRESSION_LEVEL = 6;
    
    // Thread-local direct buffers to reduce allocation overhead
    private static final ThreadLocal<ByteBuffer> SERIALIZE_BUFFER = ThreadLocal.withInitial(() ->
        ByteBuffer.allocateDirect(1024 * 1024)); // 1MB initial buffer
    
    private static final ThreadLocal<ByteBuffer> COMPRESS_BUFFER = ThreadLocal.withInitial(() ->
        ByteBuffer.allocateDirect(1024 * 1024)); // 1MB initial buffer
    
    private final boolean useNativeCompression;
    private final NativeChunkCompressor nativeChunkCompressor;
    
    public OptimizedChunkSerializer() {
        this.useNativeCompression = LatticeConfig.isChunkSerializationOptimizationEnabled();
        this.nativeChunkCompressor = useNativeCompression ? new NativeChunkCompressor() : null;
    }
    
    /**
     * Serialize and compress a chunk using native libdeflate compression
     * 
     * @param chunk The chunk to serialize
     * @param compressionLevel The compression level (1-12 for libdeflate)
     * @return ByteBuf containing the compressed chunk data
     * @throws IOException If serialization fails
     */
    public ByteBuf serializeAndCompress(LevelChunk chunk, int compressionLevel) throws IOException {
        // Serialize chunk to NBT
        SerializableChunkData serializableChunk = SerializableChunkData.copyOf(chunk.level, chunk);
        CompoundTag chunkNBT = serializableChunk.write();
        
        // Serialize NBT to binary format
        ByteBuffer serializedData = serializeNBTToBuffer(chunkNBT);
        
        ByteBuffer compressedData;
        if (useNativeCompression && nativeChunkCompressor != null) {
            // Use native libdeflate compression
            compressedData = nativeChunkCompressor.compressChunkData(
                serializedData, serializedData.remaining(), compressionLevel);
        } else {
            // Fallback to standard Java compression
            Deflater deflater = new Deflater(compressionLevel);
            deflater.setInput(serializedData.array());
            deflater.finish();
            
            byte[] outputBuffer = new byte[serializedData.remaining()];
            int compressedSize = deflater.deflate(outputBuffer);
            deflater.end();
            
            compressedData = ByteBuffer.wrap(outputBuffer, 0, compressedSize);
        }
        
        if (compressedData == null) {
            throw new IOException("Failed to compress chunk data");
        }
        
        // Wrap in Netty ByteBuf
        byte[] compressedArray = new byte[compressedData.remaining()];
        compressedData.get(compressedArray);
        return Unpooled.wrappedBuffer(compressedArray);
    }
    
    /**
     * Serialize and compress a chunk using native libdeflate compression with default compression level
     * 
     * @param chunk The chunk to serialize
     * @return ByteBuf containing the compressed chunk data
     * @throws IOException If serialization fails
     */
    public static ByteBuf serializeAndCompress(LevelChunk chunk) throws IOException {
        return serializeAndCompress(chunk, DEFAULT_COMPRESSION_LEVEL);
    }
    
    /**
     * Decompress and deserialize chunk data
     * 
     * @param compressedData The compressed chunk data
     * @param expectedSize The expected decompressed size (if known)
     * @return The deserialized chunk data
     * @throws IOException If decompression or deserialization fails
     */
    public CompoundTag decompressAndDeserialize(ByteBuf compressedData, int expectedSize) throws IOException {
        // Get direct buffer for compressed data
        ByteBuffer compressedBuffer;
        if (compressedData.hasMemoryAddress()) {
            // Already a direct buffer
            compressedBuffer = compressedData.nioBuffer();
        } else {
            // Copy to direct buffer for zero-copy operations
            int dataSize = compressedData.readableBytes();
            compressedBuffer = ByteBuffer.allocateDirect(dataSize);
            compressedData.getBytes(compressedData.readerIndex(), compressedBuffer);
            compressedBuffer.flip();
        }
        
        ByteBuffer decompressedBuffer;
        if (useNativeCompression && nativeChunkCompressor != null) {
            // Use native libdeflate decompression
            decompressedBuffer = NativeChunkCompressor.decompressChunkData(
                compressedBuffer, compressedBuffer.remaining(), expectedSize);
        } else {
            // Fallback to standard Java decompression
            java.util.zip.Inflater inflater = new java.util.zip.Inflater();
            inflater.setInput(compressedBuffer.array());
            
            byte[] outputBuffer = new byte[expectedSize];
            try {
                int decompressedSize = inflater.inflate(outputBuffer);
                decompressedBuffer = ByteBuffer.wrap(outputBuffer, 0, decompressedSize);
            } catch (DataFormatException e) {
                throw new IOException("Failed to decompress chunk data", e);
            } finally {
                inflater.end();
            }
        }
        
        if (decompressedBuffer == null) {
            throw new IOException("Failed to decompress chunk data");
        }
        
        // Deserialize NBT from binary data
        byte[] nbtData = new byte[decompressedBuffer.remaining()];
        decompressedBuffer.get(nbtData);
        
        try (DataInputStream dataInput = new DataInputStream(new java.io.ByteArrayInputStream(nbtData))) {
            return NbtIo.read(dataInput, NbtAccounter.unlimitedHeap());
        } catch (Exception e) {
            throw new IOException("Failed to deserialize NBT data", e);
        }
    }
    
    /**
     * Decompress and deserialize chunk data without knowing the expected size
     * 
     * @param compressedData The compressed chunk data
     * @return The deserialized chunk data
     * @throws IOException If decompression or deserialization fails
     */
    public static CompoundTag decompressAndDeserialize(ByteBuf compressedData) throws IOException {
        // For now, we'll use a large buffer and hope it's enough
        // In practice, you might want to store the expected size or use streaming decompression
        return decompressAndDeserialize(compressedData, 1024 * 1024); // 1MB guess
    }
    
    /**
     * Serialize NBT data to a direct ByteBuffer
     * 
     * @param nbt The NBT data to serialize
     * @return ByteBuffer containing the serialized data
     * @throws IOException If serialization fails
     */
    private static ByteBuffer serializeNBTToBuffer(CompoundTag nbt) throws IOException {
        ByteBuffer buffer = SERIALIZE_BUFFER.get();
        buffer.clear();
        
        // If buffer is too small, allocate a larger one
        if (buffer.capacity() < 1024) {
            buffer = ByteBuffer.allocateDirect(1024 * 1024); // 1MB
            SERIALIZE_BUFFER.set(buffer);
        }
        
        // Serialize NBT to buffer
        byte[] tempArray = new byte[buffer.capacity()];
        try (java.io.ByteArrayOutputStream baos = new java.io.ByteArrayOutputStream(tempArray);
             DataOutputStream dataOutput = new DataOutputStream(baos)) {
            
            NbtIo.write(nbt, dataOutput);
            dataOutput.flush();
            
            int size = baos.size();
            if (size > buffer.capacity()) {
                // Need a larger buffer
                buffer = ByteBuffer.allocateDirect(size + (size / 4)); // Add 25% extra space
                SERIALIZE_BUFFER.set(buffer);
            }
            
            buffer.clear();
            buffer.put(tempArray, 0, size);
            buffer.flip();
            
            return buffer;
        }
    }
    
    /**
     * Write compressed chunk data directly to a file channel using zero-copy operations
     * 
     * @param chunk The chunk to serialize and write
     * @param fileChannel The file channel to write to
     * @param compressionLevel The compression level to use
     * @throws IOException If writing fails
     */
    public void writeCompressedChunk(LevelChunk chunk, FileChannel fileChannel, int compressionLevel) throws IOException {
        ByteBuf compressedData = serializeAndCompress(chunk, compressionLevel);
        try {
            // Convert to ByteBuffer and write to file
            if (compressedData.hasMemoryAddress()) {
                ByteBuffer nioBuffer = compressedData.nioBuffer();
                fileChannel.write(nioBuffer);
            } else {
                // Copy to temporary direct buffer
                byte[] temp = new byte[compressedData.readableBytes()];
                compressedData.readBytes(temp);
                ByteBuffer directBuffer = ByteBuffer.allocateDirect(temp.length);
                directBuffer.put(temp);
                directBuffer.flip();
                fileChannel.write(directBuffer);
            }
        } finally {
            compressedData.release();
        }
    }
    
    /**
     * Read and decompress chunk data directly from a file channel
     * 
     * @param fileChannel The file channel to read from
     * @param length The length of compressed data to read
     * @return The deserialized chunk data
     * @throws IOException If reading fails
     */
    public CompoundTag readCompressedChunk(FileChannel fileChannel, int length) throws IOException {
        ByteBuffer compressedBuffer = ByteBuffer.allocateDirect(length);
        int bytesRead = fileChannel.read(compressedBuffer);
        if (bytesRead != length) {
            throw new IOException("Failed to read expected number of bytes: expected " + length + ", got " + bytesRead);
        }
        compressedBuffer.flip();
        
        // Wrap in Netty ByteBuf
        byte[] temp = new byte[length];
        compressedBuffer.get(temp);
        ByteBuf nettyBuffer = Unpooled.wrappedBuffer(temp);
        
        try {
            return decompressAndDeserialize(nettyBuffer, length * 10); // Guess decompressed size
        } finally {
            nettyBuffer.release();
        }
    }
}