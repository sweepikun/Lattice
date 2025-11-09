package io.lattice.chunk;

import io.netty.buffer.ByteBuf;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.chunk.LevelChunk;
import net.minecraft.world.level.chunk.storage.SerializableChunkData;
import io.lattice.config.LatticeConfig; // 添加配置导入

import java.io.IOException;
import java.nio.channels.FileChannel;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/**
 * Chunk storage manager that uses optimized serialization and compression.
 * This class demonstrates how to integrate the native libdeflate-based compression
 * into a real chunk storage system.
 */
public class ChunkStorageManager {
    
    private final ServerLevel level;
    private final Path regionFolder;
    private final ExecutorService ioExecutor;
    private final OptimizedChunkSerializer chunkSerializer; // 添加序列化器字段
    
    public ChunkStorageManager(ServerLevel level, Path regionFolder, int compressionLevel) {
        this.level = level;
        this.regionFolder = regionFolder;
        this.ioExecutor = Executors.newFixedThreadPool(
            Math.max(1, Runtime.getRuntime().availableProcessors() / 2),
            r -> {
                Thread t = new Thread(r, "ChunkIO-" + r.hashCode());
                t.setDaemon(true);
                return t;
            }
        );
        
        // 根据配置决定是否启用优化
        this.chunkSerializer = LatticeConfig.isChunkSerializationOptimizationEnabled() ? 
            new OptimizedChunkSerializer() : new OptimizedChunkSerializer(false);
    }
    
    /**
     * Save a chunk asynchronously using optimized serialization
     * 
     * @param chunk The chunk to save
     * @return CompletableFuture that completes when the chunk is saved
     */
    public CompletableFuture<Void> saveChunkAsync(LevelChunk chunk) {
        return CompletableFuture.runAsync(() -> {
            try {
                saveChunk(chunk);
            } catch (Exception e) {
                throw new RuntimeException("Failed to save chunk at " + chunk.getPos(), e);
            }
        }, ioExecutor);
    }
    
    /**
     * Save a chunk synchronously using optimized serialization
     * 
     * @param chunk The chunk to save
     * @throws IOException If saving fails
     */
    public void saveChunk(LevelChunk chunk) throws IOException {
        ChunkPos pos = chunk.getPos();
        Path regionFile = getRegionFilePath(pos);
        
        // Ensure the region folder exists
        java.nio.file.Files.createDirectories(regionFile.getParent());
        
        // 使用配置选择的序列化器
        if (LatticeConfig.isChunkSerializationOptimizationEnabled()) {
            // Serialize and compress the chunk using the optimized serializer
            try (FileChannel fileChannel = FileChannel.open(regionFile, 
                    StandardOpenOption.CREATE, StandardOpenOption.WRITE, StandardOpenOption.SPARSE)) {
                
                // Write using optimized serializer
                OptimizedChunkSerializer.writeCompressedChunk(chunk, fileChannel, compressionLevel);
            }
        } else {
            // 使用默认的序列化方法
            try (FileChannel fileChannel = FileChannel.open(regionFile, 
                    StandardOpenOption.CREATE, StandardOpenOption.WRITE, StandardOpenOption.SPARSE)) {
                
                // Write using default serialization
                chunkSerializer.writeUncompressedChunk(chunk, fileChannel);
            }
        }
    }
    
    /**
     * Load a chunk asynchronously using optimized deserialization
     * 
     * @param pos The position of the chunk to load
     * @return CompletableFuture with the loaded chunk data
     */
    public CompletableFuture<CompoundTag> loadChunkAsync(ChunkPos pos) {
        return CompletableFuture.supplyAsync(() -> {
            try {
                return loadChunk(pos);
            } catch (Exception e) {
                throw new RuntimeException("Failed to load chunk at " + pos, e);
            }
        }, ioExecutor);
    }
    
    /**
     * Load a chunk synchronously using optimized deserialization
     * 
     * @param pos The position of the chunk to load
     * @return The loaded chunk data
     * @throws IOException If loading fails
     */
    public CompoundTag loadChunk(ChunkPos pos) throws IOException {
        Path regionFile = getRegionFilePath(pos);
        
        if (!java.nio.file.Files.exists(regionFile)) {
            return null; // Chunk doesn't exist
        }
        
        try (FileChannel fileChannel = FileChannel.open(regionFile, StandardOpenOption.READ)) {
            // For this example, we'll assume the entire file is one chunk
            // In a real implementation, you'd need to parse the region file format
            long fileSize = fileChannel.size();
            if (fileSize > Integer.MAX_VALUE) {
                throw new IOException("Chunk file too large: " + fileSize + " bytes");
            }
            
            // 根据配置选择序列化方法
            if (LatticeConfig.isChunkSerializationOptimizationEnabled()) {
                return OptimizedChunkSerializer.readCompressedChunk(fileChannel, (int) fileSize);
            } else {
                return chunkSerializer.readUncompressedChunk(fileChannel, (int) fileSize);
            }
        }
    }
    
    /**
     * Get the region file path for a chunk position
     * 
     * @param pos The chunk position
     * @return The path to the region file
     */
    private Path getRegionFilePath(ChunkPos pos) {
        int regionX = pos.x >> 5;
        int regionZ = pos.z >> 5;
        return regionFolder.resolve("r." + regionX + "." + regionZ + ".mca");
    }
    
    /**
     * Shutdown the storage manager and its thread pool
     */
    public void shutdown() {
        ioExecutor.shutdown();
    }
    
    /**
     * Create a chunk storage manager with default settings
     * 
     * @param level The server level
     * @param regionFolder The folder where region files are stored
     * @return A new chunk storage manager
     */
    public static ChunkStorageManager createDefault(ServerLevel level, Path regionFolder) {
        return new ChunkStorageManager(level, regionFolder, 6); // Default compression level
    }
}