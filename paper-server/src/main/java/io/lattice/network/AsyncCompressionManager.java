package io.lattice.network;

import io.lattice.config.LatticeConfig;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.nio.ByteBuffer;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * 异步压缩管理器
 * 管理异步压缩任务和动态压缩级别调整
 */
public class AsyncCompressionManager {
    private static final Logger LOGGER = LoggerFactory.getLogger(AsyncCompressionManager.class);
    
    // 单例实例
    private static AsyncCompressionManager instance;
    
    // 工作线程数
    private volatile int workerCount;
    
    // 压缩统计信息
    private final AtomicInteger bytesSent = new AtomicInteger(0);
    private volatile double rttMs = 0.0;
    private volatile int playerCount = 0;
    private volatile double cpuUsage = 0.0;
    
    // 压缩级别配置
    private volatile int baseLevel;
    private volatile int minLevel;
    private volatile int maxLevel;
    private volatile double sensitivity;
    
    // 动态压缩开关
    private volatile boolean dynamicCompressionEnabled;
    
    /**
     * 私有构造函数
     */
    private AsyncCompressionManager() {
        reloadConfig();
        initializeWorkerCount();
    }
    
    /**
     * 获取单例实例
     * @return AsyncCompressionManager实例
     */
    public static synchronized AsyncCompressionManager getInstance() {
        if (instance == null) {
            instance = new AsyncCompressionManager();
        }
        return instance;
    }
    
    /**
     * 重新加载配置
     */
    public void reloadConfig() {
        baseLevel = LatticeConfig.getCompressionLevel();
        minLevel = LatticeConfig.getMinCompressionLevel();
        maxLevel = LatticeConfig.getMaxCompressionLevel();
        sensitivity = LatticeConfig.getCompressionSensitivity();
        dynamicCompressionEnabled = LatticeConfig.isDynamicCompressionEnabled();
        
        // 更新工作线程数
        if (NativeCompression.isNativeOptimizationAvailable()) {
            try {
                NativeCompression.setWorkerCount(workerCount);
            } catch (Throwable t) {
                LOGGER.warn("设置工作线程数失败", t);
            }
        }
    }
    
    /**
     * 初始化工作线程数
     */
    private void initializeWorkerCount() {
        if (NativeCompression.isNativeOptimizationAvailable()) {
            try {
                workerCount = NativeCompression.suggestWorkerCount();
                NativeCompression.setWorkerCount(workerCount);
                LOGGER.info("异步压缩器初始化，使用 {} 个工作线程", workerCount);
            } catch (Throwable t) {
                workerCount = 4; // 默认值
                LOGGER.warn("获取建议工作线程数失败，使用默认值: 4", t);
            }
        } else {
            workerCount = 4; // 默认值
            LOGGER.info("原生优化不可用，异步压缩器使用默认 {} 个工作线程", workerCount);
        }
    }
    
    /**
     * 异步压缩数据
     * @param srcBuffer 源数据缓冲区
     * @param srcLen 源数据长度
     * @param dstBuffer 目标数据缓冲区
     * @param dstCapacity 目标缓冲区容量
     * @param callback 压缩结果回调
     */
    public void compressAsync(ByteBuffer srcBuffer, long srcLen, ByteBuffer dstBuffer, long dstCapacity, 
                              AsyncCompressionCallback callback) {
        int level = getEffectiveCompressionLevel();
        
        if (NativeCompression.isNativeOptimizationAvailable()) {
            try {
                NativeCompression.compressAsync(srcBuffer, srcLen, dstBuffer, dstCapacity, level, callback);
                return;
            } catch (Throwable t) {
                LOGGER.warn("原生异步压缩失败，回退到同步实现", t);
            }
        }
        
        // 回退到同步实现
        int result = NativeCompression.compressZlibDirect(srcBuffer, (int) srcLen, dstBuffer, (int) dstCapacity, level);
        if (result > 0) {
            callback.onSuccess(result);
        } else {
            callback.onError(result);
        }
    }
    
    /**
     * 获取有效的压缩级别
     * @return 压缩级别
     */
    public int getEffectiveCompressionLevel() {
        if (!dynamicCompressionEnabled) {
            return baseLevel;
        }
        
        if (NativeCompression.isNativeOptimizationAvailable()) {
            try {
                return NativeCompression.suggestCompressionLevel(
                    bytesSent.get(), rttMs, playerCount, cpuUsage,
                    baseLevel, minLevel, maxLevel);
            } catch (Throwable t) {
                LOGGER.warn("获取建议压缩级别失败，使用基础级别", t);
            }
        }
        
        // 回退实现 - 基于简单规则调整压缩级别
        int level = baseLevel;
        
        // 高CPU使用率：降低压缩以减少CPU负载
        if (cpuUsage > 80) {
            level = Math.max(level - 2, minLevel);
        } else if (cpuUsage < 30) {
            level = Math.min(level + 1, maxLevel);
        }
        
        // 高延迟：提高压缩（节省带宽）
        if (rttMs > 200) {
            level = Math.min(level + 2, maxLevel);
        } else if (rttMs < 50) {
            level = Math.max(level - 1, minLevel);
        }
        
        // 玩家数多：降低压缩以减少CPU负载
        if (playerCount > 150) {
            level = Math.max(level - 2, minLevel);
        }
        
        return level;
    }
    
    /**
     * 更新网络统计信息
     * @param bytesSent 已发送字节数
     * @param rttMs 往返时间（毫秒）
     * @param playerCount 玩家数量
     * @param cpuUsage CPU使用率（0-100）
     */
    public void updateNetworkStats(long bytesSent, double rttMs, int playerCount, double cpuUsage) {
        this.bytesSent.set((int) bytesSent);
        this.rttMs = rttMs;
        this.playerCount = playerCount;
        this.cpuUsage = cpuUsage;
    }
    
    /**
     * 获取工作线程数
     * @return 工作线程数
     */
    public int getWorkerCount() {
        return workerCount;
    }
    
    /**
     * 设置工作线程数
     * @param workerCount 工作线程数
     */
    public void setWorkerCount(int workerCount) {
        this.workerCount = workerCount;
        if (NativeCompression.isNativeOptimizationAvailable()) {
            try {
                NativeCompression.setWorkerCount(workerCount);
            } catch (Throwable t) {
                LOGGER.warn("设置工作线程数失败", t);
            }
        }
    }
    
    /**
     * 获取基础压缩级别
     * @return 基础压缩级别
     */
    public int getBaseLevel() {
        return baseLevel;
    }
    
    /**
     * 设置基础压缩级别
     * @param baseLevel 基础压缩级别
     */
    public void setBaseLevel(int baseLevel) {
        this.baseLevel = baseLevel;
    }
    
    /**
     * 获取最小压缩级别
     * @return 最小压缩级别
     */
    public int getMinLevel() {
        return minLevel;
    }
    
    /**
     * 设置最小压缩级别
     * @param minLevel 最小压缩级别
     */
    public void setMinLevel(int minLevel) {
        this.minLevel = minLevel;
    }
    
    /**
     * 获取最大压缩级别
     * @return 最大压缩级别
     */
    public int getMaxLevel() {
        return maxLevel;
    }
    
    /**
     * 设置最大压缩级别
     * @param maxLevel 最大压缩级别
     */
    public void setMaxLevel(int maxLevel) {
        this.maxLevel = maxLevel;
    }
    
    /**
     * 检查动态压缩是否启用
     * @return 动态压缩是否启用
     */
    public boolean isDynamicCompressionEnabled() {
        return dynamicCompressionEnabled;
    }
    
    /**
     * 设置动态压缩启用状态
     * @param dynamicCompressionEnabled 动态压缩启用状态
     */
    public void setDynamicCompressionEnabled(boolean dynamicCompressionEnabled) {
        this.dynamicCompressionEnabled = dynamicCompressionEnabled;
    }
    
    /**
     * 获取压缩灵敏度
     * @return 压缩灵敏度
     */
    public double getSensitivity() {
        return sensitivity;
    }
    
    /**
     * 设置压缩灵敏度
     * @param sensitivity 压缩灵敏度
     */
    public void setSensitivity(double sensitivity) {
        this.sensitivity = sensitivity;
    }
}