
import io.lattice.config.LatticeConfig;
import io.lattice.world.LightOptimizedWorld;
import net.minecraft.core.BlockPos;

public class LevelLightEngine implements LevelLightEngineLayer {
    
    // Lattice native light engine
    private LightOptimizedWorld lightOptimizedWorld;
    
    public LevelLightEngine() {
        
        // Initialize Lattice native light engine if enabled
        if (LatticeConfig.isLightUpdateOptimizationEnabled()) {
            try {
                // We'll set the world later when it's available
                this.lightOptimizedWorld = new LightOptimizedWorld(null);
            } catch (Throwable t) {
                LOGGER.warn("Failed to initialize Lattice native light engine, falling back to vanilla", t);
                this.lightOptimizedWorld = null;
            }
        }
    }
    
    public void setLevel(net.minecraft.world.level.Level level) {
        this.level = level;
        if (this.lightOptimizedWorld != null) {
            this.lightOptimizedWorld = new LightOptimizedWorld(level);
        }
    }
    
    // Override light update methods to use native implementation if enabled
    @Override
    public int getLightValue(BlockPos pos) {
        if (lightOptimizedWorld != null) {
            // Combine sky light and block light
            int skyLight = lightOptimizedWorld.getLightLevel(pos, true);
            int blockLight = lightOptimizedWorld.getLightLevel(pos, false);
            return Math.max(skyLight, blockLight);
        }
        return super.getLightValue(pos);
    }
    
    public void updateLight(BlockPos pos, int lightLevel, boolean isSkyLight) {
        if (lightOptimizedWorld != null) {
            if (lightLevel > 0) {
                lightOptimizedWorld.setLightLevel(pos, lightLevel, isSkyLight);
            } else {
                lightOptimizedWorld.removeLightSource(pos, isSkyLight);
            }
            lightOptimizedWorld.propagateLightUpdates();
        } else {
            // Fallback to vanilla implementation
            if (isSkyLight) {
                this.skyEngine.setLightValue(pos, lightLevel);
            } else {
                this.blockEngine.setLightValue(pos, lightLevel);
            }
        }
    }
    
}