
import io.lattice.config.LatticeConfig;
import io.lattice.chunk.OptimizedChunkSerializer;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.world.level.chunk.LevelChunk;

public class ChunkSerializer {
    
    // Lattice optimized chunk serializer
    private static final OptimizedChunkSerializer optimizedSerializer = 
        LatticeConfig.isChunkSerializationOptimizationEnabled() ? new OptimizedChunkSerializer() : null;
    
    public static CompoundTag write(LevelChunk chunk) {
        if (optimizedSerializer != null) {
            try {
                // Use Lattice optimized serialization
                return optimizedSerializer.serialize(chunk);
            } catch (Throwable t) {
                // Fallback to vanilla if optimization fails
                LOGGER.warn("Failed to use Lattice optimized chunk serialization, falling back to vanilla", t);
            }
        }
        
        // Vanilla implementation
        return SerializableChunkData.copyOf(chunk.getServerLevel(), chunk).write();
    }
    
    public static LevelChunk read(ServerLevel level, ChunkPos pos, CompoundTag tag) {
        if (optimizedSerializer != null) {
            try {
                // Use Lattice optimized deserialization
                // This would require implementing the read method in OptimizedChunkSerializer
                // For now, we'll fall back to vanilla
            } catch (Throwable t) {
                // Fallback to vanilla if optimization fails
                LOGGER.warn("Failed to use Lattice optimized chunk deserialization, falling back to vanilla", t);
            }
        }
        
        // Vanilla implementation
        return (LevelChunk) SerializableChunkData.parse(level, pos, tag).create(level.getLevel(), level.getChunkSource().getGenerator());
    }
    
}