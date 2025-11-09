package io.lattice.world;

import io.lattice.config.LatticeConfig;
import io.lattice.nativeutil.LatticeNativeInitializer;
import net.minecraft.core.BlockPos;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * Native light updater using high-performance C++ implementation.
 * This class provides a JNI interface to a C++ light propagation algorithm
 * that is optimized for Minecraft-like world lighting updates.
 */
public class NativeLightUpdater {
    private static final Logger LOGGER = LoggerFactory.getLogger(NativeLightUpdater.class);
    
    // 原生库是否已加载
    private static boolean nativeLibraryLoaded = false;
    
    // 原生优化是否可用
    private static boolean nativeOptimizationAvailable = false;
    
    // 使用统一的本地库初始化器
    static {
        nativeLibraryLoaded = LatticeNativeInitializer.isNativeLibraryLoaded();
        nativeOptimizationAvailable = LatticeNativeInitializer.isFeatureNativeOptimizationAvailable(
            LatticeConfig.isLightUpdateOptimizationEnabled());
        
        // 如果本地优化未启用，检查配置
        if (!nativeOptimizationAvailable) {
            if (!LatticeConfig.isLightUpdateOptimizationEnabled()) {
                LOGGER.info("光照更新原生优化已禁用");
            } else {
                LOGGER.warn("光照更新原生优化不可用");
            }
        } else {
            LOGGER.info("光照更新原生优化已启用");
        }
    }
    
    // Thread-local instance to avoid synchronization issues
    private static final ThreadLocal<NativeLightUpdater> INSTANCE = ThreadLocal.withInitial(() -> {
        NativeLightUpdater updater = new NativeLightUpdater();
        updater.initLightUpdater();
        return updater;
    });
    
    /**
     * Get the thread-local instance of the light updater
     * @return The light updater instance for this thread
     */
    public static NativeLightUpdater getInstance() {
        return INSTANCE.get();
    }
    
    /**
     * Initialize the native light updater for this thread
     */
    public native void initLightUpdater();
    
    /**
     * Release the native light updater for this thread
     */
    public native void releaseLightUpdater();
    
    /**
     * Add a light source at the specified position
     * @param pos The block position
     * @param level The light level (0-15)
     * @param isSkyLight Whether this is a sky light or block light
     */
    public native void addLightSource(BlockPos pos, int level, boolean isSkyLight);
    
    /**
     * Remove a light source at the specified position
     * @param pos The block position
     * @param isSkyLight Whether this is a sky light or block light
     */
    public native void removeLightSource(BlockPos pos, boolean isSkyLight);
    
    /**
     * Propagate all pending light updates
     */
    public native void propagateLightUpdates();
    
    /**
     * Check if there are pending light updates
     * @return true if there are pending updates, false otherwise
     */
    public native boolean hasUpdates();
    
    /**
     * Get the light level at the specified position
     * @param pos The block position
     * @param isSkyLight Whether to get sky light or block light
     * @return The light level (0-15)
     */
    public native int getLightLevel(BlockPos pos, boolean isSkyLight);
    
    /**
     * Private constructor to enforce singleton pattern per thread
     */
    private NativeLightUpdater() {
        if (nativeOptimizationAvailable) {
            initLightUpdater();
        }
    }
    
    /**
     * Clean up native resources when the object is garbage collected
     */
    @Override
    protected void finalize() throws Throwable {
        try {
            if (nativeOptimizationAvailable) {
                releaseLightUpdater();
            }
        } finally {
            super.finalize();
        }
    }
    
    /**
     * 检查原生优化是否可用
     * @return 原生优化是否可用
     */
    public static boolean isNativeOptimizationAvailable() {
        return nativeOptimizationAvailable && LatticeConfig.isLightUpdateOptimizationEnabled();
    }
    
    /**
     * 获取原生库加载状态
     * @return 原生库是否已加载
     */
    public static boolean isNativeLibraryLoaded() {
        return nativeLibraryLoaded;
    }
}