package io.lattice.world;

import net.minecraft.core.BlockPos;
import net.minecraft.world.level.BlockGetter;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LightLayer;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.chunk.ChunkAccess;
import net.minecraft.world.level.lighting.LevelLightEngine;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * A world implementation that uses the optimized native light updater
 * for improved performance in light propagation operations.
 * If optimization is disabled via config, it will use the standard Minecraft light updating system.
 */
public class LightOptimizedWorld {
    
    private static final Logger LOGGER = LoggerFactory.getLogger(LightOptimizedWorld.class);
    
    private final Level world;
    private final LevelLightEngine lightEngine;
    
    public LightOptimizedWorld(Level world) {
        this.world = world;
        this.lightEngine = world.getLightEngine();
        
        // 设置世界引用，供原生代码使用
        if (NativeLightUpdater.isNativeLibraryLoaded()) {
            try {
                NativeLightUpdater.setWorldReference(world);
            } catch (Throwable t) {
                LOGGER.warn("无法设置原生光照更新器的世界引用", t);
            }
        }
    }
    
    /**
     * 更新指定位置的光照
     * @param pos 需要更新光照的位置
     * @param lightLayer 光照类型（方块光或天空光）
     * @param chunk 区块对象
     */
    public void updateLight(BlockPos pos, LightLayer lightLayer, ChunkAccess chunk) {
        // 检查是否启用原生光照优化
        if (NativeLightUpdater.isNativeOptimizationAvailable()) {
            try {
                // 使用原生光照更新器
                NativeLightUpdater lightUpdater = NativeLightUpdater.getInstance();
                
                // 获取当前光照级别
                int currentLight = lightEngine.getLayerListener(lightLayer).getLightValue(pos);
                
                // 添加光源
                boolean isSkyLight = lightLayer == LightLayer.SKY;
                lightUpdater.addLightSource(pos, currentLight, isSkyLight);
                
                // 传播光照更新
                lightUpdater.propagateLightUpdates();
                
                return; // 成功使用原生实现
            } catch (Throwable t) {
                LOGGER.warn("原生光照更新失败，回退到标准实现", t);
            }
        }
        
        // 回退到标准实现
        lightEngine.checkBlock(pos);
    }
    
    /**
     * 获取指定位置的光照级别
     * @param pos 位置
     * @param lightLayer 光照类型
     * @return 光照级别 (0-15)
     */
    public int getLightLevel(BlockPos pos, LightLayer lightLayer) {
        // 检查是否启用原生光照优化
        if (NativeLightUpdater.isNativeOptimizationAvailable()) {
            try {
                // 使用原生光照更新器获取光照级别
                NativeLightUpdater lightUpdater = NativeLightUpdater.getInstance();
                boolean isSkyLight = lightLayer == LightLayer.SKY;
                return lightUpdater.getLightLevel(pos, isSkyLight);
            } catch (Throwable t) {
                LOGGER.warn("原生光照获取失败，回退到标准实现", t);
            }
        }
        
        // 回退到标准实现
        return lightEngine.getLayerListener(lightLayer).getLightValue(pos);
    }
    
    /**
     * Add a light source at the specified position
     * @param pos The position to add the light source
     * @param lightLevel The intensity of the light (0-15)
     * @param isSkyLight Whether this is a sky light source
     */
    public void addLightSource(BlockPos pos, int lightLevel, boolean isSkyLight) {
        if (nativeLightUpdater != null) {
            // Use native implementation for adding light source
            nativeLightUpdater.addLightSource(pos, lightLevel, isSkyLight);
        } else {
            // Fallback to standard implementation
            if (isSkyLight) {
                world.getLightEngine().skyEngine.setLightValue(pos, lightLevel);
            } else {
                world.getLightEngine().blockEngine.setLightValue(pos, lightLevel);
            }
        }
    }
    
    /**
     * Remove a light source from the specified position
     * @param pos The position to remove the light source from
     * @param isSkyLight Whether this is a sky light source
     */
    public void removeLightSource(BlockPos pos, boolean isSkyLight) {
        if (nativeLightUpdater != null) {
            // Use native implementation for removing light source
            nativeLightUpdater.removeLightSource(pos, isSkyLight);
        } else {
            // Fallback to standard implementation
            if (isSkyLight) {
                world.getLightEngine().skyEngine.setLightValue(pos, 0);
            } else {
                world.getLightEngine().blockEngine.setLightValue(pos, 0);
            }
        }
    }
    
    /**
     * Propagate light updates through the world
     */
    public void propagateLightUpdates() {
        if (nativeLightUpdater != null) {
            // Use native implementation for propagating light updates
            if (nativeLightUpdater.hasUpdates()) {
                nativeLightUpdater.propagateLightUpdates();
            }
        } else {
            // Fallback - light updates are handled by the standard engine
            // No additional action needed
        }
    }
    
    /**
     * Handle a block placement event
     * @param pos The position where the block was placed
     * @param state The block state that was placed
     */
    public void onBlockPlaced(BlockPos pos, BlockState state) {
        // 更新光照，因为放置了新方块
        updateLighting(pos);
    }
    
    /**
     * Handle a block removal event
     * @param pos The position where the block was removed
     */
    public void onBlockRemoved(BlockPos pos) {
        // 更新光照，因为移除了方块
        updateLighting(pos);
    }
}