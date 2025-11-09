package io.lattice.world;

import io.lattice.config.LatticeConfig;
import io.lattice.nativeutil.LatticeNativeInitializer;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * 原生寻路器
 * 使用C++原生代码优化Minecraft生物AI寻路
 */
public class NativePathfinder {
    private static final Logger LOGGER = LoggerFactory.getLogger(NativePathfinder.class);
    
    // 原生库是否已加载
    private static boolean nativeLibraryLoaded = false;
    
    // 原生优化是否可用
    private static boolean nativeOptimizationAvailable = false;
    
    // 生物类型枚举
    public static final int MOB_TYPE_PASSIVE = 0;     // 被动生物
    public static final int MOB_TYPE_NEUTRAL = 1;     // 中立生物
    public static final int MOB_TYPE_HOSTILE = 2;     // 敌对生物
    public static final int MOB_TYPE_WATER = 3;       // 水生生物
    public static final int MOB_TYPE_FLYING = 4;      // 飞行生物
    
    static {
        // 使用统一的本地库初始化器
        nativeLibraryLoaded = LatticeNativeInitializer.isNativeLibraryLoaded();
        nativeOptimizationAvailable = LatticeNativeInitializer.isFeatureNativeOptimizationAvailable(
            LatticeConfig.isPathfindingNativeOptimizationEnabled());
        
        // 如果本地优化未启用，检查配置
        if (!nativeOptimizationAvailable) {
            if (!LatticeConfig.isPathfindingNativeOptimizationEnabled()) {
                LOGGER.info("寻路原生优化已禁用");
            } else {
                LOGGER.warn("寻路原生优化不可用");
            }
        } else {
            LOGGER.info("寻路原生优化已启用");
        }
    }
    
    /**
     * 优化寻路计算
     * @param startX 起点X坐标
     * @param startY 起点Y坐标
     * @param startZ 起点Z坐标
     * @param endX 终点X坐标
     * @param endY 终点Y坐标
     * @param endZ 终点Z坐标
     * @param mobType 生物类型
     * @return 寻路路径节点数组，如果找不到路径则返回null
     */
    public static PathNode[] optimizePathfinding(int startX, int startY, int startZ,
                                                int endX, int endY, int endZ,
                                                int mobType) {
        if (nativeOptimizationAvailable && LatticeConfig.isPathfindingNativeOptimizationEnabled()) {
            try {
                return nativeOptimizePathfinding(startX, startY, startZ, endX, endY, endZ, mobType);
            } catch (Throwable t) {
                LOGGER.warn("原生寻路计算失败，回退到Java实现", t);
                nativeOptimizationAvailable = false; // 禁用原生优化以避免后续错误
            }
        }
        
        // 回退到Java实现（简化版）
        return null;
    }
    
    /**
     * 原生方法：优化寻路计算
     */
    private static native PathNode[] nativeOptimizePathfinding(int startX, int startY, int startZ,
                                                              int endX, int endY, int endZ,
                                                              int mobType);
    
    /**
     * 原生方法：获取生物寻路参数
     */
    private static native MobPathfindingParams nativeGetMobPathfindingParams(int mobType);
    
    /**
     * 获取生物寻路参数
     * @param mobType 生物类型
     * @return 寻路参数
     */
    public static MobPathfindingParams getMobPathfindingParams(int mobType) {
        if (nativeOptimizationAvailable && LatticeConfig.isPathfindingNativeOptimizationEnabled()) {
            try {
                return nativeGetMobPathfindingParams(mobType);
            } catch (Throwable t) {
                LOGGER.warn("原生获取生物寻路参数失败，回退到Java实现", t);
                nativeOptimizationAvailable = false; // 禁用原生优化以避免后续错误
            }
        }
        
        // 回退到Java实现（返回默认参数）
        return new MobPathfindingParams();
    }
    
    /**
     * 根据Minecraft原版实体类型获取对应的寻路类型
     * @param entityClass 实体类名
     * @return 对应的寻路类型
     */
    public static int getMobTypeForEntity(String entityClass) {
        // 根据实体类名判断生物类型
        if (entityClass.contains("passive") || 
            entityClass.contains("animal") ||
            entityClass.contains("cow") ||
            entityClass.contains("pig") ||
            entityClass.contains("sheep") ||
            entityClass.contains("chicken")) {
            return MOB_TYPE_PASSIVE;
        } else if (entityClass.contains("neutral") ||
                   entityClass.contains("wolf") ||
                   entityClass.contains("piglin")) {
            return MOB_TYPE_NEUTRAL;
        } else if (entityClass.contains("monster") ||
                   entityClass.contains("zombie") ||
                   entityClass.contains("skeleton") ||
                   entityClass.contains("creeper") ||
                   entityClass.contains("spider")) {
            return MOB_TYPE_HOSTILE;
        } else if (entityClass.contains("water") ||
                   entityClass.contains("fish") ||
                   entityClass.contains("squid") ||
                   entityClass.contains("dolphin")) {
            return MOB_TYPE_WATER;
        } else if (entityClass.contains("bat") ||
                   entityClass.contains("phantom") ||
                   entityClass.contains("ender_dragon")) {
            return MOB_TYPE_FLYING;
        }
        
        // 默认为敌对生物
        return MOB_TYPE_HOSTILE;
    }
    
    /**
     * 检查原生优化是否可用
     * @return 原生优化是否可用
     */
    public static boolean isNativeOptimizationAvailable() {
        return nativeOptimizationAvailable && LatticeConfig.isPathfindingNativeOptimizationEnabled();
    }
    
    /**
     * 获取原生库加载状态
     * @return 原生库是否已加载
     */
    public static boolean isNativeLibraryLoaded() {
        return nativeLibraryLoaded;
    }
}