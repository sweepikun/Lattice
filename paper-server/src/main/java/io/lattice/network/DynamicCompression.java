package io.lattice.network;

import io.lattice.config.LatticeConfig;
import io.lattice.nativeutil.LatticeNativeInitializer;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * 动态压缩
 * 根据服务器负载、网络状况自动调整压缩级别
 */
public class DynamicCompression {
    private static final Logger LOGGER = LoggerFactory.getLogger(DynamicCompression.class);
    
    // 原生库是否已加载
    private static boolean nativeLibraryLoaded = false;
    
    // 原生优化是否可用
    private static boolean nativeOptimizationAvailable = false;
    
    static {
        // 使用统一的本地库初始化器
        nativeLibraryLoaded = LatticeNativeInitializer.isNativeLibraryLoaded();
        nativeOptimizationAvailable = LatticeNativeInitializer.isFeatureNativeOptimizationAvailable(
            LatticeConfig.isDynamicCompressionEnabled());
        
        // 如果本地优化未启用，记录日志
        if (!nativeOptimizationAvailable) {
            LOGGER.debug("动态压缩原生优化不可用");
        }
    }
    
    /**
     * 建议压缩级别
     * @param baseLevel 基准压缩级别
     * @param cpuUsage CPU使用率 (0.0 - 100.0)
     * @param rttMs 平均往返时间(毫秒)
     * @param playerCount 玩家数量
     * @param minLevel 最小压缩级别
     * @param maxLevel 最大压缩级别
     * @return 建议的压缩级别
     */
    public static int suggestCompressionLevel(int baseLevel, double cpuUsage, double rttMs, 
                                            int playerCount, int minLevel, int maxLevel) {
        if (nativeOptimizationAvailable) {
            try {
                return nativeSuggestCompressionLevel(baseLevel, cpuUsage, rttMs, playerCount, minLevel, maxLevel);
            } catch (Throwable t) {
                LOGGER.warn("原生动态压缩级别建议失败", t);
                nativeOptimizationAvailable = false; // 禁用原生优化以避免后续错误
            }
        }
        
        // 回退到Java实现
        return suggestCompressionLevelFallback(baseLevel, cpuUsage, rttMs, playerCount, minLevel, maxLevel);
    }
    
    /**
     * 建议工作线程数
     * @return 建议的工作线程数
     */
    public static int suggestWorkerCount() {
        if (nativeOptimizationAvailable) {
            try {
                // 在实际实现中，这里应该调用原生方法
                // 暂时返回默认值
                return Runtime.getRuntime().availableProcessors() - 1;
            } catch (Throwable t) {
                LOGGER.warn("原生工作线程数建议失败", t);
            }
        }
        
        // 回退到Java实现
        return suggestWorkerCountFallback();
    }
    
    /**
     * 原生方法：建议压缩级别
     */
    private static native int nativeSuggestCompressionLevel(int baseLevel, double cpuUsage, double rttMs, 
                                                    int playerCount, int minLevel, int maxLevel);
    
    /**
     * 检查原生优化是否可用
     * @return 原生优化是否可用
     */
    public static boolean isNativeOptimizationAvailable() {
        return nativeOptimizationAvailable;
    }
    
    /**
     * 获取原生库加载状态
     * @return 原生库是否已加载
     */
    public static boolean isNativeLibraryLoaded() {
        return nativeLibraryLoaded;
    }
    
    /**
     * 回退实现：建议压缩级别
     */
    private static int suggestCompressionLevelFallback(int baseLevel, double cpuUsage, double rttMs, 
                                                     int playerCount, int minLevel, int maxLevel) {
        // 基于配置的基准级别
        int level = baseLevel;
        
        // 高CPU使用率：降低压缩以减少CPU负载
        if (cpuUsage > 80) {
            level = Math.max(level - 2, 1);
        } else if (cpuUsage < 30) {
            level = Math.min(level + 1, 9);
        }
        
        // 高延迟：提高压缩（节省带宽）
        if (rttMs > 200) {
            level = Math.min(level + 2, 9);
        } else if (rttMs < 50) {
            level = Math.max(level - 1, 1);
        }
        
        // 玩家数多：降低压缩以减少CPU负载
        if (playerCount > 150) {
            level = Math.max(level - 2, 1);
        }
        
        // 应用管理员设定的边界
        return Math.max(minLevel, Math.min(maxLevel, level));
    }
    
    /**
     * 回退实现：建议工作线程数
     */
    private static int suggestWorkerCountFallback() {
        int cores = Runtime.getRuntime().availableProcessors();
        // 默认策略：使用 (N-1) 个核心，最多 8 个，最少 1 个
        return Math.max(1, Math.min(8, cores - 1));
    }
}